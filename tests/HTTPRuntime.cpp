#include <steam/steam_api.h>
#include <algorithm>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace
{
enum
{
    Ready           = 1,
    HTTPAvailable   = 2,
    SendFailure     = 8,
    HeaderFailure   = 16,
    BodySizeFailure = 32,
    BodyDataFailure = 64
};
int                                      mode;
uint32                                   nextRequest = 10;
std::string                              lastURL;
std::vector<CCallbackBase*>              callbacks;
std::map<CCallbackBase*, SteamAPICall_t> results;
class HTTP : public ISteamHTTP
{
public:
    HTTPRequestHandle CreateHTTPRequest(EHTTPMethod, const char* url) override
    {
        lastURL = url;
        return ++nextRequest;
    }
    bool SetHTTPRequestContextValue(HTTPRequestHandle, uint64) override { return true; }
    bool SetHTTPRequestNetworkActivityTimeout(HTTPRequestHandle, uint32 seconds) override { return seconds != 0; }
    bool SetHTTPRequestHeaderValue(HTTPRequestHandle, const char* name, const char* value) override
    {
        return name && value && _stricmp(name, "User-Agent") && !(mode & HeaderFailure);
    }
    bool SetHTTPRequestGetOrPostParameter(HTTPRequestHandle, const char*, const char*) override { return true; }
    bool SendHTTPRequest(HTTPRequestHandle h, SteamAPICall_t* call) override
    {
        *call = h + 1000;
        return !(mode & SendFailure);
    }
    bool SendHTTPRequestAndStreamResponse(HTTPRequestHandle h, SteamAPICall_t* call) override { return SendHTTPRequest(h, call); }
    bool DeferHTTPRequest(HTTPRequestHandle) override { return true; }
    bool PrioritizeHTTPRequest(HTTPRequestHandle) override { return true; }
    bool GetHTTPResponseHeaderSize(HTTPRequestHandle, const char*, uint32* size) override
    {
        *size = 3;
        return true;
    }
    bool GetHTTPResponseHeaderValue(HTTPRequestHandle, const char*, uint8* buffer, uint32 size) override
    {
        if (size < 3) return false;
        memcpy(buffer, "ok", 3);
        return true;
    }
    bool GetHTTPResponseBodySize(HTTPRequestHandle, uint32* size) override
    {
        *size = 2;
        return !(mode & BodySizeFailure);
    }
    bool GetHTTPResponseBodyData(HTTPRequestHandle, uint8* data, uint32 size) override
    {
        if (size != 2 || (mode & BodyDataFailure)) return false;
        memcpy(data, "ok", 2);
        return true;
    }
    bool                      GetHTTPStreamingResponseBodyData(HTTPRequestHandle h, uint32, uint8* data, uint32 size) override { return GetHTTPResponseBodyData(h, data, size); }
    bool                      ReleaseHTTPRequest(HTTPRequestHandle) override { return true; }
    bool                      GetHTTPDownloadProgressPct(HTTPRequestHandle, float*) override { return true; }
    bool                      SetHTTPRequestRawPostBody(HTTPRequestHandle, const char*, uint8*, uint32) override { return true; }
    HTTPCookieContainerHandle CreateCookieContainer(bool) override { return 100; }
    bool                      ReleaseCookieContainer(HTTPCookieContainerHandle) override { return true; }
    bool                      SetCookie(HTTPCookieContainerHandle, const char*, const char*, const char*) override { return true; }
    bool                      SetHTTPRequestCookieContainer(HTTPRequestHandle, HTTPCookieContainerHandle) override { return true; }
    bool                      SetHTTPRequestUserAgentInfo(HTTPRequestHandle, const char*) override { return true; }
    bool                      SetHTTPRequestRequiresVerifiedCertificate(HTTPRequestHandle, bool) override { return true; }
    bool                      SetHTTPRequestAbsoluteTimeoutMS(HTTPRequestHandle, uint32) override { return true; }
    bool                      GetHTTPRequestWasTimedOut(HTTPRequestHandle, bool*) override { return true; }
} http;
} // namespace
extern "C"
{
    int __cdecl                               MockGetUser() { return mode & Ready ? 1 : 0; }
    int __cdecl                               MockGetPipe() { return mode & Ready ? 2 : 0; }
    void* __cdecl                             MockFind(int, const char* version) { return (mode & Ready) && (mode & HTTPAvailable) && !strcmp(version, STEAMHTTP_INTERFACE_VERSION) ? &http : nullptr; }
    void __cdecl                              MockRegister(CCallbackBase* cb, int) { callbacks.push_back(cb); }
    void __cdecl                              MockUnregister(CCallbackBase* cb) { std::erase(callbacks, cb); }
    void __cdecl                              MockRegisterResult(CCallbackBase* cb, SteamAPICall_t call) { results[cb] = call; }
    void __cdecl                              MockUnregisterResult(CCallbackBase* cb, SteamAPICall_t) { results.erase(cb); }
    __declspec(dllexport) void __cdecl        MockMode(int value) { mode = value; }
    __declspec(dllexport) uint32 __cdecl      MockLastRequest() { return nextRequest; }
    __declspec(dllexport) const char* __cdecl MockLastURL() { return lastURL.c_str(); }
    __declspec(dllexport) void __cdecl        MockEmit(int kind, uint32 request)
    {
        if (kind == 3)
        {
            HTTPRequestCompleted_t p{};
            p.m_hRequest           = request;
            p.m_bRequestSuccessful = true;
            p.m_eStatusCode        = k_EHTTPStatusCode200OK;
            p.m_unBodySize         = 2;
            auto snapshot          = results;
            for (auto [cb, call] : snapshot)
                if (call == request + 1000 && results.contains(cb)) cb->Run(&p, false, call);
            return;
        }
        HTTPRequestHeadersReceived_t header{};
        header.m_hRequest = request;
        HTTPRequestDataReceived_t data{};
        data.m_hRequest       = request;
        data.m_cBytesReceived = 2;
        auto snapshot         = callbacks;
        int  id               = kind == 1 ? HTTPRequestHeadersReceived_t::k_iCallback : HTTPRequestDataReceived_t::k_iCallback;
        for (auto cb : snapshot)
            if (std::find(callbacks.begin(), callbacks.end(), cb) != callbacks.end() && cb->GetICallback() == id)
                cb->Run(kind == 1 ? (void*)&header : (void*)&data);
    }
}
