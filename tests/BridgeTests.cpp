#include <Windows.h>
#include <IUtilHTTPClient.h>
#include <cassert>
#include <string>

struct Counts { int complete{}, data{}, destroyed{}; bool destroyRequest{}; };
class Callbacks : public IUtilHTTPCallbacks {
    Counts& counts;
public:
    explicit Callbacks(Counts& value): counts(value) {}
    void Destroy() override { ++counts.destroyed; delete this; }
    void OnResponseComplete(IUtilHTTPRequest* request,IUtilHTTPResponse*) override { ++counts.complete; if(counts.destroyRequest) request->Destroy(); }
    void OnUpdateState(IUtilHTTPRequest*,IUtilHTTPResponse*,UtilHTTPRequestState) override {}
    void OnReceiveData(IUtilHTTPRequest*,IUtilHTTPResponse*,const void* data,size_t size) override { assert(std::string(static_cast<const char*>(data),size)=="ok"); ++counts.data; }
};
int wmain(int argc,wchar_t** argv) {
    assert(argc==4);
    auto bridge=LoadLibraryW(argv[2]); assert(bridge);
    auto module=LoadLibraryW(argv[1]); assert(module);
    auto factoryFn=(CreateInterfaceFn)GetProcAddress(module,"CreateInterface"); assert(factoryFn);
    auto factory=(IUtilHTTPClientFactory*)factoryFn(UTIL_HTTPCLIENT_FACTORY_STEAMAPI_INTERFACE_VERSION,nullptr); assert(factory);
    auto client=factory->CreateUtilHTTPClient(); assert(client);
    CUtilHTTPClientCreationContext options; client->Init(&options);
    Counts unavailable;
    auto request=client->CreateSyncRequest("https://example.invalid/",UtilHTTPMethod::Get,new Callbacks(unavailable)); assert(request);
    request->Send(); assert(request->WaitForCompleteTimeout(100)); assert(request->IsFinished()); assert(!request->IsRequestSuccessful());
    assert(request->GetResponse()->IsResponseError()); assert(1==unavailable.complete);
    request->Destroy(); assert(1==unavailable.destroyed);
    auto runtime=LoadLibraryW(argv[3]); assert(runtime);
    auto mode=(void(__cdecl*)(int))GetProcAddress(runtime,"MockMode");
    auto emit=(void(__cdecl*)(int,unsigned))GetProcAddress(runtime,"MockEmit");
    auto last=(unsigned(__cdecl*)())GetProcAddress(runtime,"MockLastRequest"); assert(mode && emit && last);
    // Immediate send failure, then setup failure; both must wake synchronous waiters.
    for(int flags : {15,23}) {
        mode(flags); Counts failed;
        request=client->CreateSyncRequest("https://example.invalid/",UtilHTTPMethod::Get,new Callbacks(failed));
        request->Send(); assert(request->WaitForCompleteTimeout(100)); assert(request->IsFinished()); assert(!request->IsRequestSuccessful());
        request->Send(); assert(1==failed.complete); request->Destroy(); assert(1==failed.destroyed);
    }
    mode(7); Counts success;
    request=client->CreateSyncRequest("https://example.invalid/",UtilHTTPMethod::Get,new Callbacks(success));
    request->Send(); emit(3,last()); // No header callback is required for completion.
    assert(request->WaitForCompleteTimeout(100)); assert(request->IsFinished()); assert(request->IsRequestSuccessful());
    assert(1==success.complete); assert(2==request->GetResponse()->GetPayload()->GetLength()); request->Destroy();
    Counts stream;
    request=client->CreateAsyncStreamRequest("https://example.invalid/",UtilHTTPMethod::Get,new Callbacks(stream));
    request->Send(); emit(1,last()); emit(2,last()); emit(2,last()); emit(3,last());
    assert(request->IsFinished()); assert(2==stream.data && 1==stream.complete); request->Destroy();
    Counts reentrant; reentrant.destroyRequest=true;
    request=client->CreateAsyncRequest("https://example.invalid/",UtilHTTPMethod::Get,new Callbacks(reentrant));
    request->Send(); emit(3,last()); assert(1==reentrant.complete && 1==reentrant.destroyed);
    client->Shutdown(); client->Destroy();
    FreeLibrary(module); FreeLibrary(bridge); FreeLibrary(runtime);
}
