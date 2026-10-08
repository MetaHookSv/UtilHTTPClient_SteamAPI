#include <Windows.h>
#include <chrono>
#include <condition_variable>
#include <format>
#include <memory>
#include <new>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <mutex>
#include <unordered_map>
#include <cctype>

#include <IUtilHTTPClient.h>

#include <ScopeExit/ScopeExit.h>

#include <steam/steam_api.h>
#include <SteamAPIBridge.h>

using BridgeContext = std::shared_ptr<SteamBridgeContext>;

EHTTPMethod UTIL_ConvertUtilHTTPMethodToSteamHTTPMethod(const UtilHTTPMethod method)
{
    switch (method)
    {
        case UtilHTTPMethod::Get:
            return EHTTPMethod::k_EHTTPMethodGET;
        case UtilHTTPMethod::Head:
            return EHTTPMethod::k_EHTTPMethodHEAD;
        case UtilHTTPMethod::Post:
            return EHTTPMethod::k_EHTTPMethodPOST;
        case UtilHTTPMethod::Put:
            return EHTTPMethod::k_EHTTPMethodPUT;
        case UtilHTTPMethod::Delete:
            return EHTTPMethod::k_EHTTPMethodDELETE;
    }

    return EHTTPMethod::k_EHTTPMethodInvalid;
}

class CURLParsedResult : public IURLParsedResult
{
public:
    CURLParsedResult(
        const std::string& scheme,
        const std::string& host,
        unsigned short     port_us,
        const std::string& target,
        bool               secure) : m_scheme(scheme),
                       m_host(host),
                       m_port_us(port_us),
                       m_target(target),
                       m_secure(secure)
    {

        m_port_str = std::format("{0}", port_us);
    }

    void Destroy() override
    {
        delete this;
    }

    const char* GetScheme() const override
    {
        return m_scheme.c_str();
    }

    const char* GetHost() const override
    {
        return m_host.c_str();
    }

    const char* GetTarget() const override
    {
        return m_target.c_str();
    }

    const char* GetPortString() const override
    {
        return m_port_str.c_str();
    }

    unsigned short GetPort() const override
    {
        return m_port_us;
    }

    bool IsSecure() const override
    {
        return m_secure;
    }

private:
    std::string    m_scheme;
    std::string    m_host;
    std::string    m_port_str;
    std::string    m_target;
    unsigned short m_port_us{};
    bool           m_secure{};
};

IURLParsedResult* ParseUrlInternal(const char* input)
{
    if (!input)
        return nullptr;
    const std::string url(input);
    for (unsigned char ch : url)
        if (ch <= 0x20 || ch == 0x7f || ch == '\\')
            return nullptr;
    static const std::regex url_regex(
        R"((http|https|ws|wss|mqtt|mqtts)://(\[[0-9a-f:.]+\]|[^/:?#@\[\]]+)(?::(\d+))?([^#]*)(?:#.*)?)",
        std::regex_constants::icase);

    std::smatch url_match_result;

    if (std::regex_match(url, url_match_result, url_regex))
    {
        // If we found a match
        if (url_match_result.size() >= 4)
        {
            // Extract the matched groups
            std::string scheme = url_match_result[1].str();
            for (auto& ch : scheme)
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            std::string host     = url_match_result[2].str();
            std::string port_str = url_match_result[3].str();
            std::string target   = (url_match_result.size() >= 5) ? url_match_result[4].str() : "";
            if (!target.empty() && target.front() == '?')
                target.insert(target.begin(), '/');
            if (!target.empty() && target.front() != '/')
                return nullptr;

            unsigned port_us = 0;
            bool     secure  = scheme == "https" || scheme == "wss" || scheme == "mqtts";

            if (!port_str.empty())
            {
                try
                {
                    size_t pos;
                    int    port = std::stoi(port_str, &pos);
                    if (pos != port_str.size() || port <= 0 || port > 65535)
                    {
                        return nullptr;
                    }
                    port_us = static_cast<unsigned short>(port);
                }
                catch (const std::invalid_argument&)
                {
                    return nullptr;
                }
                catch (const std::out_of_range&)
                {
                    return nullptr;
                }
            }
            else
            {
                if (scheme == "http")
                {
                    port_us = 80;
                }
                else if (scheme == "https")
                {
                    port_us = 443;
                    secure  = true;
                }
                else if (scheme == "ws")
                {
                    port_us = 80;
                }
                else if (scheme == "wss")
                {
                    port_us = 443;
                    secure  = true;
                }
                else if (scheme == "mqtt")
                {
                    port_us = 1883;
                }
                else if (scheme == "mqtts")
                {
                    port_us = 8883;
                    secure  = true;
                }
            }

            return new CURLParsedResult(scheme, host, port_us, target, secure);
        }
    }

    return nullptr;
}

static std::string ToLowerCase(const char* str)
{
    std::string result(str);
    for (auto& c : result)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return result;
}

static bool IsHTTPURL(const IURLParsedResult* result)
{
    const std::string_view scheme(result->GetScheme());
    return scheme == "http" || scheme == "https";
}

class CUtilHTTPPayload : public IUtilHTTPPayload
{
private:
    std::string              m_payload;
    SteamBridgeContextHandle m_Bridge;

public:
    explicit CUtilHTTPPayload(SteamBridgeContextHandle bridge) : m_Bridge(bridge) {}

    const char* GetBytes() const
    {
        return m_payload.data();
    }

    size_t GetLength() const
    {
        return m_payload.size();
    }

public:
    bool ReadPayloadFromRequestHandle(HTTPRequestHandle handle)
    {
        m_payload.clear();
        uint32_t responseSize = 0;
        if (SteamBridge_HTTPGetBodySize(m_Bridge, handle, &responseSize) == SB_OK)
        {
            std::string payload(responseSize, '\0');
            if (SteamBridge_HTTPGetBody(m_Bridge, handle, payload.data(), responseSize) == SB_OK)
            {
                m_payload.swap(payload);
                return true;
            }
        }

        return false;
    }
};

class CUtilHTTPResponse : public IUtilHTTPResponse
{
private:
    SteamBridgeContextHandle m_Bridge;
    bool                     m_bResponseCompleted{};
    bool                     m_bResponseError{};
    bool                     m_bIsStreamPayload{};
    int                      m_iResponseStatusCode{};
    std::string              m_ResponseErrorMessage;

    //cached headers
    std::unordered_map<std::string, std::shared_ptr<std::string>> m_headers;

    CUtilHTTPPayload* m_pResponsePayload{};

    HTTPRequestHandle m_RequestHandle{INVALID_HTTPREQUEST_HANDLE};

public:
    void SetRequestHandle(HTTPRequestHandle handle) { m_RequestHandle = handle; }

    void SetStreamPayload(bool bIsStreamPayload)
    {
        m_bIsStreamPayload = bIsStreamPayload;
    }

    bool OnSteamHTTPDataReceived(HTTPRequestDataReceived_t* pResult, bool bHasError, std::string& buf)
    {
        m_RequestHandle = pResult->m_hRequest;

        if (m_bIsStreamPayload && !bHasError)
        {
            buf.resize(pResult->m_cBytesReceived, '\0');

            if (SteamBridge_HTTPGetStreamData(m_Bridge, pResult->m_hRequest, pResult->m_cOffset, buf.data(), pResult->m_cBytesReceived) == SB_OK)
            {
                return true;
            }
        }

        return false;
    }

    void OnSteamHTTPCompleted(HTTPRequestCompleted_t* pResult, bool bHasError)
    {
        m_RequestHandle = pResult->m_hRequest;

        m_bResponseCompleted  = true;
        m_bResponseError      = bHasError || !pResult->m_bRequestSuccessful;
        m_iResponseStatusCode = (int)pResult->m_eStatusCode;

        if (pResult->m_bRequestSuccessful && !bHasError && !m_bIsStreamPayload)
        {
            m_bResponseError = !m_pResponsePayload->ReadPayloadFromRequestHandle(pResult->m_hRequest);
        }
    }

public:
    explicit CUtilHTTPResponse(SteamBridgeContextHandle bridge) : m_Bridge(bridge), m_pResponsePayload(new CUtilHTTPPayload(bridge))
    {
    }

    ~CUtilHTTPResponse()
    {
        if (m_pResponsePayload)
        {
            delete m_pResponsePayload;
            m_pResponsePayload = nullptr;
        }
    }

    bool GetHeaderSize(const char* name, size_t* buflen) override
    {
        return SteamBridge_HTTPGetHeaderSize(m_Bridge, m_RequestHandle, name, buflen) == SB_OK;
    }

    bool GetHeader(const char* name, char* buf, size_t buflen) override
    {
        return SteamBridge_HTTPGetHeader(m_Bridge, m_RequestHandle, name, buf, static_cast<uint32_t>(buflen)) == SB_OK;
    }

    const char* GetHeaderValue(const char* name) override
    {
        auto lowerName = ToLowerCase(name);

        auto it = m_headers.find(lowerName);
        if (it != m_headers.end())
        {
            return it->second->c_str();
        }

        size_t valueLength{0};
        if (GetHeaderSize(name, &valueLength) && valueLength > 0)
        {
            std::string value(valueLength, '\0');
            if (GetHeader(name, value.data(), valueLength))
            {
                m_headers[lowerName] = std::make_shared<std::string>(value);

                return m_headers[lowerName]->c_str();
            }
        }

        return nullptr;
    }

    bool IsResponseCompleted() const override
    {
        return m_bResponseCompleted;
    }

    bool IsResponseError() const override
    {
        return m_bResponseError;
    }

    const char* GetResponseErrorMessage() const override
    {
        return m_ResponseErrorMessage.c_str();
    }

    int GetStatusCode() const override
    {
        return m_iResponseStatusCode;
    }

    IUtilHTTPPayload* GetPayload() const override
    {
        return m_pResponsePayload;
    }
};

struct RequestPool
{
    std::mutex                                                 mutex;
    std::unordered_map<UtilHTTPRequestId_t, IUtilHTTPRequest*> requests;
};

class CUtilHTTPRequest : public IUtilHTTPRequest
{
protected:
    BridgeContext              m_Bridge;
    std::weak_ptr<RequestPool> m_Pool;
    UtilHTTPRequestId_t        m_PoolId{};
    bool                       m_bSetupFailed{};
    unsigned                   m_DispatchDepth{};
    bool                       m_DestroyPending{};
    struct DispatchScope
    {
        CUtilHTTPRequest* request;
        explicit DispatchScope(CUtilHTTPRequest* value) : request(value) { ++request->m_DispatchDepth; }
        ~DispatchScope()
        {
            if (!--request->m_DispatchDepth && request->m_DestroyPending) delete request;
        }
    };
    UtilHTTPRequestId_t m_RequestId{UTILHTTP_REQUEST_INVALID_ID};
    HTTPRequestHandle   m_RequestHandle{};
    bool                m_bRequesting{};
    bool                m_bResponding{};
    bool                m_bRequestSuccessful{};
    bool                m_bFinished{};
    bool                m_bAutoDestroyOnFinish{};
    IUtilHTTPCallbacks* m_Callbacks{};
    CUtilHTTPResponse*  m_pResponse{};

public:
    CUtilHTTPRequest(
        const UtilHTTPMethod      method,
        const std::string&        host,
        unsigned short            port,
        bool                      secure,
        const std::string&        target,
        IUtilHTTPCallbacks*       callbacks,
        HTTPCookieContainerHandle hCookieHandle,
        BridgeContext             bridge,
        bool                      cookieFailed) :
        m_Bridge(std::move(bridge)),
        m_bSetupFailed(cookieFailed),
        m_Callbacks(callbacks),
        m_pResponse(new CUtilHTTPResponse(m_Bridge.get()))
    {
        std::string field_host = host;

        if (secure && port != 443)
        {
            field_host = std::format("{0}:{1}", host, port);
        }
        else if (!secure && port != 80)
        {
            field_host = std::format("{0}:{1}", host, port);
        }

        std::string url = std::format("{0}://{1}{2}", secure ? "https" : "http", field_host, target.empty() ? "/" : target);

        if (SteamBridge_HTTPCreate(m_Bridge.get(), UTIL_ConvertUtilHTTPMethodToSteamHTTPMethod(method), url.c_str(), &m_RequestHandle) != SB_OK)
            m_bSetupFailed = true;

        if (hCookieHandle != INVALID_HTTPCOOKIE_HANDLE)
            if (SteamBridge_HTTPSetCookies(m_Bridge.get(), m_RequestHandle, hCookieHandle) != SB_OK)
                m_bSetupFailed = true;

        SetField("Host", field_host.c_str());
    }

    virtual ~CUtilHTTPRequest()
    {
        if (m_RequestHandle != INVALID_HTTPREQUEST_HANDLE)
        {
            SteamBridge_HTTPRelease(m_Bridge.get(), m_RequestHandle);
            m_RequestHandle = INVALID_HTTPREQUEST_HANDLE;
        }

        if (m_Callbacks)
        {
            m_Callbacks->Destroy();
            m_Callbacks = nullptr;
        }
        delete m_pResponse;
    }

    virtual void OnRespondStart()
    {
        if (!m_bResponding)
        {
            m_bRequesting = false;
            m_bResponding = true;

            if (m_Callbacks)
            {
                m_Callbacks->OnUpdateState(this, m_pResponse, UtilHTTPRequestState::Responding);
            }
        }
    }

    virtual void OnRespondFinish()
    {
        if (!m_bFinished)
        {
            m_bFinished   = true;
            m_bRequesting = false;
            m_bResponding = false;

            if (m_Callbacks)
            {
                m_Callbacks->OnUpdateState(this, m_pResponse, UtilHTTPRequestState::Finished);
            }
        }
    }

    virtual void OnSteamHTTPHeaderReceived(HTTPRequestHeadersReceived_t* pResult, bool bHasError)
    {
        m_pResponse->SetRequestHandle(pResult->m_hRequest);
        OnRespondStart();
    }

    virtual void OnSteamHTTPCompleted(HTTPRequestCompleted_t* pResult, bool bHasError)
    {
        if (m_bFinished)
            return;
        m_bRequestSuccessful = pResult->m_bRequestSuccessful && !bHasError;

        m_pResponse->OnSteamHTTPCompleted(pResult, bHasError);
        m_bRequestSuccessful = m_bRequestSuccessful && !m_pResponse->IsResponseError();

        OnRespondFinish();
        if (m_Callbacks)
        {
            m_Callbacks->OnResponseComplete(this, m_pResponse);
        }
    }

    virtual void OnSteamHTTPDataReceived(HTTPRequestDataReceived_t*, bool) {}

    static void __cdecl OnBridgeEvent(void* opaque, const SteamBridgeHTTPEvent* event)
    {
        auto          self = static_cast<CUtilHTTPRequest*>(opaque);
        DispatchScope dispatch(self);
        if (self->m_bFinished || self->m_DestroyPending)
            return;
        if (event->kind == SB_HTTP_HEADERS)
        {
            HTTPRequestHeadersReceived_t value{};
            value.m_hRequest = event->request;
            self->OnSteamHTTPHeaderReceived(&value, event->ioFailure != 0);
        }
        else if (event->kind == SB_HTTP_DATA)
        {
            HTTPRequestDataReceived_t value{};
            value.m_hRequest       = event->request;
            value.m_cOffset        = event->offset;
            value.m_cBytesReceived = event->bytes;
            self->OnSteamHTTPDataReceived(&value, event->ioFailure != 0);
        }
        else if (event->kind == SB_HTTP_COMPLETE)
        {
            HTTPRequestCompleted_t value{};
            value.m_hRequest           = event->request;
            value.m_bRequestSuccessful = event->successful != 0;
            value.m_eStatusCode        = static_cast<EHTTPStatusCode>(event->statusCode);
            self->OnSteamHTTPCompleted(&value, event->ioFailure != 0);
        }
    }

public:
    bool AttachToPool(const std::shared_ptr<RequestPool>& pool, UtilHTTPRequestId_t id)
    {
        if (!m_Pool.expired() || m_DestroyPending)
            return false;
        m_Pool   = pool;
        m_PoolId = id;
        return true;
    }

    void Destroy() override
    {
        if (m_DestroyPending)
            return;
        m_DestroyPending = true;
        if (auto pool = m_Pool.lock())
        {
            std::lock_guard<std::mutex> lock(pool->mutex);
            auto                        it = pool->requests.find(m_PoolId);
            if (it != pool->requests.end() && it->second == this)
                pool->requests.erase(it);
            m_Pool.reset();
        }
        if (!m_DispatchDepth)
            delete this;
    }

    void Send() override
    {
        DispatchScope dispatch(this);
        if (m_bRequesting || m_bResponding || m_bFinished)
            return;
        m_bRequesting = true;

        if (m_Callbacks)
        {
            m_Callbacks->OnUpdateState(this, m_pResponse, UtilHTTPRequestState::Requesting);
        }
        if (m_DestroyPending)
            return;
        if (m_bSetupFailed || SteamBridge_HTTPSend(m_Bridge.get(), m_RequestHandle, IsStream(), OnBridgeEvent, this) != SB_OK)
        {
            HTTPRequestCompleted_t failure{};
            failure.m_hRequest = m_RequestHandle;
            OnSteamHTTPCompleted(&failure, true);
        }
    }

    bool IsRequesting() const override
    {
        return m_bRequesting;
    }

    bool IsResponding() const override
    {
        return m_bResponding;
    }

    bool IsRequestSuccessful() const override
    {
        return m_bRequestSuccessful;
    }

    bool IsFinished() const override
    {
        return m_bFinished;
    }

    bool IsAutoDestroyOnFinish() const override
    {
        return m_bAutoDestroyOnFinish;
    }

    UtilHTTPRequestState GetRequestState() const override
    {
        if (IsFinished())
            return UtilHTTPRequestState::Finished;

        if (IsRequesting())
            return UtilHTTPRequestState::Requesting;

        if (IsResponding())
            return UtilHTTPRequestState::Responding;

        return UtilHTTPRequestState::Idle;
    }

    void SetRequestId(UtilHTTPRequestId_t id) override
    {
        m_RequestId = id;
    }

    UtilHTTPRequestId_t GetRequestId() const override
    {
        return m_RequestId;
    }

    void SetAutoDestroyOnFinish(bool b) override
    {
        m_bAutoDestroyOnFinish = b;
    }

    void SetTimeout(int secs) override
    {
        if (secs <= 0 || SteamBridge_HTTPSetTimeout(m_Bridge.get(), m_RequestHandle, secs) != SB_OK)
            m_bSetupFailed = true;
    }

    void SetPostBody(const char* contentType, const char* payload, size_t payloadSize) override
    {
        if (!contentType)
            contentType = "application/octet-stream";

        if (SteamBridge_HTTPSetBody(m_Bridge.get(), m_RequestHandle, contentType, payload, static_cast<uint32_t>(payloadSize)) != SB_OK)
            m_bSetupFailed = true;
    }

    void SetField(const char* field, const char* value) override
    {
        if (SteamBridge_HTTPSetHeader(m_Bridge.get(), m_RequestHandle, field, value) != SB_OK)
            m_bSetupFailed = true;
    }

    void SetRequireCertVerification(bool b) override
    {
        if (SteamBridge_HTTPSetCertificateVerification(m_Bridge.get(), m_RequestHandle, b) != SB_OK)
            m_bSetupFailed = true;
    }

    void SetFollowLocation(bool b) override
    {
        //not supported
    }
};

class CUtilHTTPSyncRequest : public CUtilHTTPRequest
{
private:
    std::mutex              m_mutex;
    std::condition_variable m_cv;
    bool                    m_isComplete{false};

public:
    CUtilHTTPSyncRequest(
        const UtilHTTPMethod      method,
        const std::string&        host,
        unsigned short            port,
        bool                      secure,
        const std::string&        target,
        IUtilHTTPCallbacks*       callbacks,
        HTTPCookieContainerHandle hCookieHandle,
        BridgeContext             bridge,
        bool                      cookieFailed) :
        CUtilHTTPRequest(method, host, port, secure, target, callbacks, hCookieHandle, std::move(bridge), cookieFailed)
    {
    }

    bool IsAsync() const override
    {
        return false;
    }

    bool IsStream() const override
    {
        return false;
    }

    void WaitForComplete() override
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this]() { return m_isComplete; });
    }

    bool WaitForCompleteTimeout(int timeout_ms) override
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this]() { return m_isComplete; });
    }

    IUtilHTTPResponse* GetResponse() override
    {
        return m_pResponse;
    }

    void OnSteamHTTPCompleted(HTTPRequestCompleted_t* pResult, bool bHasError) override
    {
        CUtilHTTPRequest::OnSteamHTTPCompleted(pResult, bHasError);


        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_isComplete = true;
        }
        m_cv.notify_one();
    }
};

class CUtilHTTPAsyncRequest : public CUtilHTTPRequest
{
private:
public:
    CUtilHTTPAsyncRequest(
        const UtilHTTPMethod      method,
        const std::string&        host,
        unsigned short            port,
        bool                      secure,
        const std::string&        target,
        IUtilHTTPCallbacks*       callbacks,
        HTTPCookieContainerHandle hCookieHandle,
        BridgeContext             bridge,
        bool                      cookieFailed) :
        CUtilHTTPRequest(method, host, port, secure, target, callbacks, hCookieHandle, std::move(bridge), cookieFailed)
    {
        m_bAutoDestroyOnFinish = true;
    }

    bool IsAsync() const override
    {
        return true;
    }

    bool IsStream() const override
    {
        return false;
    }

    void WaitForComplete() override
    {
    }

    bool WaitForCompleteTimeout(int timeout_ms) override
    {
        return false;
    }

    IUtilHTTPResponse* GetResponse() override
    {
        return nullptr;
    }
};

class CUtilHTTPAsyncStreamRequest : public CUtilHTTPAsyncRequest
{
public:
    CUtilHTTPAsyncStreamRequest(
        const UtilHTTPMethod      method,
        const std::string&        host,
        unsigned short            port,
        bool                      secure,
        const std::string&        target,
        IUtilHTTPCallbacks*       StreamCallbacks,
        HTTPCookieContainerHandle hCookieHandle,
        BridgeContext             bridge,
        bool                      cookieFailed) :
        CUtilHTTPAsyncRequest(method, host, port, secure, target, StreamCallbacks, hCookieHandle, std::move(bridge), cookieFailed)
    {
        m_pResponse->SetStreamPayload(true);
    }

    bool IsStream() const override
    {
        return true;
    }

    void OnSteamHTTPHeaderReceived(HTTPRequestHeadersReceived_t* pResult, bool bHasError) override
    {
        CUtilHTTPRequest::OnSteamHTTPHeaderReceived(pResult, bHasError);
    }

    void OnSteamHTTPDataReceived(HTTPRequestDataReceived_t* pResult, bool bHasError) override
    {
        std::string buf;
        if (!m_pResponse->OnSteamHTTPDataReceived(pResult, bHasError, buf))
        {
            HTTPRequestCompleted_t failure{};
            failure.m_hRequest = m_RequestHandle;
            OnSteamHTTPCompleted(&failure, true);
            return;
        }

        if (m_Callbacks)
        {
            m_Callbacks->OnReceiveData(
                this,
                m_pResponse,
                buf.data(),
                buf.size());
        }
    }
};

class CUtilHTTPClient : public IUtilHTTPClient
{
private:
    BridgeContext                m_Bridge{SteamBridge_CreateContext(), SteamBridge_DestroyContext};
    bool                         m_bCookieFailed{};
    std::shared_ptr<RequestPool> m_Pool{std::make_shared<RequestPool>()};
    UtilHTTPRequestId_t          m_RequestUsedId{UTILHTTP_REQUEST_START_ID};
    HTTPCookieContainerHandle    m_CookieHandle{INVALID_HTTPCOOKIE_HANDLE};

public:
    ~CUtilHTTPClient()
    {
        Shutdown();
        if (m_CookieHandle)
        {
            SteamBridge_HTTPReleaseCookies(m_Bridge.get(), m_CookieHandle);
            m_CookieHandle = INVALID_HTTPCOOKIE_HANDLE;
        }
    }

    void Destroy() override
    {
        delete this;
    }

    void Init(const CUtilHTTPClientCreationContext* context) override
    {
        if (m_CookieHandle)
            SteamBridge_HTTPReleaseCookies(m_Bridge.get(), m_CookieHandle);
        m_CookieHandle  = INVALID_HTTPCOOKIE_HANDLE;
        m_bCookieFailed = false;
        if (context->m_bUseCookieContainer)
        {
            m_bCookieFailed = SteamBridge_HTTPCreateCookies(m_Bridge.get(), context->m_bAllowResponseToModifyCookie, &m_CookieHandle) != SB_OK;
        }
    }

    void Shutdown() override
    {
        auto pool = m_Pool;
        for (;;)
        {
            IUtilHTTPRequest* request{};
            {
                std::lock_guard<std::mutex> lock(pool->mutex);
                if (pool->requests.empty())
                    break;
                auto it = pool->requests.begin();
                request = it->second;
                pool->requests.erase(it);
            }
            request->Destroy();
        }
    }

    void RunFrame() override
    {
        // Remove one request before destruction. A consumer destructor may reenter
        // the client and destroy other requests, so do not retain a pointer batch.
        auto pool = m_Pool;
        for (;;)
        {
            IUtilHTTPRequest* finished{};
            {
                std::lock_guard<std::mutex> lock(pool->mutex);
                for (auto it = pool->requests.begin(); it != pool->requests.end(); ++it)
                {
                    if (it->second->IsFinished() && it->second->IsAutoDestroyOnFinish())
                    {
                        finished = it->second;
                        pool->requests.erase(it);
                        break;
                    }
                }
            }
            if (!finished)
                break;
            finished->Destroy();
        }
    }

    IURLParsedResult* ParseUrl(const char* url) override
    {
        return ParseUrlInternal(url);
    }

    IUtilHTTPRequest* CreateSyncRequestEx(const char* host, unsigned short port_us, const char* target, bool secure, const UtilHTTPMethod method, IUtilHTTPCallbacks* callback)
    {
        return new CUtilHTTPSyncRequest(method, host, port_us, secure, target, callback, m_CookieHandle, m_Bridge, m_bCookieFailed);
    }

    IUtilHTTPRequest* CreateAsyncRequestEx(const char* host, unsigned short port_us, const char* target, bool secure, const UtilHTTPMethod method, IUtilHTTPCallbacks* callback)
    {
        return new CUtilHTTPAsyncRequest(method, host, port_us, secure, target, callback, m_CookieHandle, m_Bridge, m_bCookieFailed);
    }

    IUtilHTTPRequest* CreateSyncRequest(const char* url, const UtilHTTPMethod method, IUtilHTTPCallbacks* callbacks) override
    {
        auto result = ParseUrlInternal(url);

        if (!result)
            return nullptr;

        SCOPE_EXIT { result->Destroy(); };

        if (!IsHTTPURL(result))
            return nullptr;
        return CreateSyncRequestEx(result->GetHost(), result->GetPort(), result->GetTarget(), result->IsSecure(), method, callbacks);
    }

    IUtilHTTPRequest* CreateAsyncRequest(const char* url, const UtilHTTPMethod method, IUtilHTTPCallbacks* callbacks) override
    {
        auto result = ParseUrlInternal(url);

        if (!result)
            return nullptr;

        SCOPE_EXIT { result->Destroy(); };

        if (!IsHTTPURL(result))
            return nullptr;
        return CreateAsyncRequestEx(result->GetHost(), result->GetPort(), result->GetTarget(), result->IsSecure(), method, callbacks);
    }

    IUtilHTTPRequest* CreateAsyncStreamRequestEx(const char* host, unsigned short port_us, const char* target, bool secure, const UtilHTTPMethod method, IUtilHTTPCallbacks* callback)
    {
        return new CUtilHTTPAsyncStreamRequest(method, host, port_us, secure, target, callback, m_CookieHandle, m_Bridge, m_bCookieFailed);
    }

    IUtilHTTPRequest* CreateAsyncStreamRequest(const char* url, const UtilHTTPMethod method, IUtilHTTPCallbacks* callbacks) override
    {
        auto result = ParseUrl(url);

        if (!result)
            return nullptr;

        SCOPE_EXIT { result->Destroy(); };

        if (!IsHTTPURL(result))
            return nullptr;
        return CreateAsyncStreamRequestEx(result->GetHost(), result->GetPort(), result->GetTarget(), result->IsSecure(), method, callbacks);
    }

    void AddToRequestPool(IUtilHTTPRequest* RequestInstance) override
    {
        auto request = dynamic_cast<CUtilHTTPRequest*>(RequestInstance);
        if (!request)
            return;
        std::lock_guard<std::mutex> lock(m_Pool->mutex);

        if (m_RequestUsedId == UTILHTTP_REQUEST_MAX_ID)
            m_RequestUsedId = UTILHTTP_REQUEST_START_ID;

        while (m_Pool->requests.contains(m_RequestUsedId))
        {
            if (++m_RequestUsedId == UTILHTTP_REQUEST_MAX_ID)
                m_RequestUsedId = UTILHTTP_REQUEST_START_ID;
        }
        auto RequestId = m_RequestUsedId;
        if (!request->AttachToPool(m_Pool, RequestId))
            return;

        RequestInstance->SetRequestId(RequestId);

        m_Pool->requests[RequestId] = RequestInstance;

        m_RequestUsedId++;
    }

    IUtilHTTPRequest* GetRequestById(UtilHTTPRequestId_t id) override
    {
        std::lock_guard<std::mutex> lock(m_Pool->mutex);

        auto itor = m_Pool->requests.find(id);

        if (itor != m_Pool->requests.end())
        {
            return itor->second;
        }

        return NULL;
    }

    bool DestroyRequestById(UtilHTTPRequestId_t id) override
    {
        IUtilHTTPRequest* request{};
        {
            std::lock_guard<std::mutex> lock(m_Pool->mutex);
            auto                        it = m_Pool->requests.find(id);
            if (it == m_Pool->requests.end())
                return false;
            request = it->second;
            m_Pool->requests.erase(it);
        }
        request->Destroy();
        return true;
    }

    bool SetCookie(const char* host, const char* url, const char* cookie) override
    {
        if (m_CookieHandle != INVALID_HTTPCOOKIE_HANDLE)
        {
            return SteamBridge_HTTPSetCookie(m_Bridge.get(), m_CookieHandle, host, url, cookie) == SB_OK;
        }

        return false;
    }
};

EXPOSE_INTERFACE(CUtilHTTPClient, IUtilHTTPClient, UTIL_HTTPCLIENT_STEAMAPI_INTERFACE_VERSION);

class CUtilHTTPClientFactory : public IUtilHTTPClientFactory
{
public:
    IUtilHTTPClient* CreateUtilHTTPClient() override
    {
        return new (std::nothrow) CUtilHTTPClient();
    }

    IURLParsedResult* ParseUrl(const char* url) override
    {
        return ParseUrlInternal(url);
    }
};

EXPOSE_SINGLE_INTERFACE(CUtilHTTPClientFactory, IUtilHTTPClientFactory, UTIL_HTTPCLIENT_FACTORY_STEAMAPI_INTERFACE_VERSION);
