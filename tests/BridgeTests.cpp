#include <Windows.h>
#include <IUtilHTTPClient.h>
#include <cassert>
#include <string>

struct Counts { int complete{}, data{}, destroyed{}; bool destroyRequest{}; IUtilHTTPClient* reenter{}; bool readHeaders{}; };
class Callbacks : public IUtilHTTPCallbacks {
    Counts& counts;
public:
    explicit Callbacks(Counts& value): counts(value) {}
    void Destroy() override { ++counts.destroyed; if(counts.reenter) { counts.reenter->GetRequestById(1); counts.reenter->RunFrame(); } delete this; }
    void OnResponseComplete(IUtilHTTPRequest* request,IUtilHTTPResponse*) override { ++counts.complete; if(counts.destroyRequest) request->Destroy(); }
    void OnUpdateState(IUtilHTTPRequest*,IUtilHTTPResponse* response,UtilHTTPRequestState state) override {
        if(counts.readHeaders && state==UtilHTTPRequestState::Responding) {
            auto header=response->GetHeaderValue("Content-Type"); assert(header && std::string(header)=="ok");
        }
    }
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
    assert(nullptr==client->CreateSyncRequest("wss://example.invalid/",UtilHTTPMethod::Get,nullptr));
    assert(nullptr==client->CreateAsyncRequest("mqtt://example.invalid/",UtilHTTPMethod::Get,nullptr));
    assert(nullptr==client->CreateAsyncStreamRequest("ws://example.invalid/",UtilHTTPMethod::Get,nullptr));
    assert(nullptr==client->CreateSyncRequest(nullptr,UtilHTTPMethod::Get,nullptr));
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
    for(int flags : {39,71}) {
        mode(flags); Counts failed;
        request=client->CreateSyncRequest("https://example.invalid/",UtilHTTPMethod::Get,new Callbacks(failed));
        request->Send(); emit(3,last());
        assert(request->WaitForCompleteTimeout(100)); assert(request->IsFinished());
        assert(!request->IsRequestSuccessful() && request->GetResponse()->IsResponseError());
        assert(0==request->GetResponse()->GetPayload()->GetLength()); assert(1==failed.complete); request->Destroy();
    }
    mode(7);
    auto lastURL=(const char*(__cdecl*)())GetProcAddress(runtime,"MockLastURL"); assert(lastURL);
    request=client->CreateSyncRequest("HTTPS://example.invalid:8443?x=1#private",UtilHTTPMethod::Get,nullptr);
    assert(std::string(lastURL())=="https://example.invalid:8443/?x=1"); request->Destroy();
    Counts stream; stream.readHeaders=true;
    request=client->CreateAsyncStreamRequest("https://example.invalid/",UtilHTTPMethod::Get,new Callbacks(stream));
    request->Send(); emit(1,last()); emit(2,last()); emit(2,last()); emit(3,last());
    assert(request->IsFinished()); assert(2==stream.data && 1==stream.complete); request->Destroy();
    Counts reentrant; reentrant.destroyRequest=true;
    request=client->CreateAsyncRequest("https://example.invalid/",UtilHTTPMethod::Get,new Callbacks(reentrant));
    request->Send(); emit(3,last()); assert(1==reentrant.complete && 1==reentrant.destroyed);
    Counts pooled; pooled.destroyRequest=true;
    request=client->CreateAsyncRequest("https://example.invalid/",UtilHTTPMethod::Get,new Callbacks(pooled));
    client->AddToRequestPool(request); auto id=request->GetRequestId();
    client->AddToRequestPool(request); assert(id==request->GetRequestId());
    request->Send(); emit(3,last()); assert(1==pooled.destroyed);
    assert(nullptr==client->GetRequestById(id)); client->RunFrame();
    for(int cleanupPath : {0,1,2}) {
        Counts cleanup; cleanup.reenter=client;
        request=client->CreateAsyncRequest("https://example.invalid/",UtilHTTPMethod::Get,new Callbacks(cleanup));
        client->AddToRequestPool(request); id=request->GetRequestId();
        if(cleanupPath==1) {
            request->Send(); emit(3,last());
            auto second=client->CreateAsyncRequest("https://example.invalid/",UtilHTTPMethod::Get,new Callbacks(cleanup));
            client->AddToRequestPool(second); second->Send(); emit(3,last());
            client->RunFrame();
        }
        else if(cleanupPath==2) client->Shutdown();
        else assert(client->DestroyRequestById(id));
        assert((cleanupPath==1 ? 2 : 1)==cleanup.destroyed && nullptr==client->GetRequestById(id));
    }
    client->Shutdown(); client->Destroy();
    FreeLibrary(module); FreeLibrary(bridge); FreeLibrary(runtime);
}
