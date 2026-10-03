#include <Windows.h>
#include <IUtilHTTPClient.h>

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
template<typename Expected, typename Actual>
void ExpectEqual(const Expected& expected, const Actual& actual, const char* message)
{
    if (!(expected == actual))
        throw std::runtime_error(message);
}

class Module
{
public:
    explicit Module(const wchar_t* path) : m_handle(LoadLibraryExW(path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH))
    {
        if (!m_handle)
            throw std::runtime_error("LoadLibraryExW failed: " + std::to_string(GetLastError()));
    }

    ~Module() { FreeLibrary(m_handle); }
    Module(const Module&) = delete;
    Module& operator=(const Module&) = delete;

    CreateInterfaceFn GetFactory() const
    {
        auto factory = reinterpret_cast<CreateInterfaceFn>(GetProcAddress(m_handle, CREATEINTERFACE_PROCNAME));
        if (!factory)
            throw std::runtime_error("CreateInterface export is missing");
        return factory;
    }

private:
    HMODULE m_handle;
};

struct DestroyObject
{
    template<typename T>
    void operator()(T* object) const { object->Destroy(); }
};

template<typename T>
using Owned = std::unique_ptr<T, DestroyObject>;

IUtilHTTPClientFactory* GetClientFactory(CreateInterfaceFn createInterface)
{
    int result = IFACE_FAILED;
    auto factory = static_cast<IUtilHTTPClientFactory*>(
        createInterface(UTIL_HTTPCLIENT_FACTORY_STEAMAPI_INTERFACE_VERSION, &result));
    ExpectEqual(IFACE_OK, result, "Factory interface lookup failed");
    ExpectEqual(true, factory != nullptr, "Factory is null");
    return factory;
}

void TestFactory(CreateInterfaceFn createInterface)
{
    auto factory = GetClientFactory(createInterface);
    ExpectEqual(factory, GetClientFactory(createInterface), "Factory must be a singleton");
    ExpectEqual(factory, createInterface(UTIL_HTTPCLIENT_FACTORY_STEAMAPI_INTERFACE_VERSION, nullptr),
        "Factory lookup without a return code failed");

    int result = IFACE_OK;
    ExpectEqual(nullptr, createInterface("UnknownInterface_999", &result), "Unknown interface must be rejected");
    ExpectEqual(IFACE_FAILED, result, "Unknown interface must report IFACE_FAILED");
    ExpectEqual(nullptr, createInterface("UtilHTTPClient_SteamAPI_006", nullptr), "Unsupported version must be rejected");

    result = IFACE_FAILED;
    Owned<IUtilHTTPClient> client(static_cast<IUtilHTTPClient*>(
        createInterface(UTIL_HTTPCLIENT_STEAMAPI_INTERFACE_VERSION, &result)));
    ExpectEqual(IFACE_OK, result, "Client interface lookup failed");
    ExpectEqual(true, client != nullptr, "Client interface is null");
}

void TestClient(CreateInterfaceFn createInterface)
{
    auto factory = GetClientFactory(createInterface);
    Owned<IUtilHTTPClient> first(factory->CreateUtilHTTPClient());
    Owned<IUtilHTTPClient> second(factory->CreateUtilHTTPClient());
    ExpectEqual(true, first != nullptr && second != nullptr, "Client creation failed");
    ExpectEqual(false, first.get() == second.get(), "Factory must create independent clients");

    CUtilHTTPClientCreationContext context;
    first->Init(&context);
    first->RunFrame();
    ExpectEqual(nullptr, first->GetRequestById(UTILHTTP_REQUEST_INVALID_ID), "Empty request pool lookup failed");
    ExpectEqual(false, first->DestroyRequestById(UTILHTTP_REQUEST_START_ID), "Empty request pool removal failed");
    ExpectEqual(false, first->SetCookie("example.test", "/", "name=value"), "Cookies must be disabled by default");
    first->Shutdown();
    first->Shutdown();

    Owned<IURLParsedResult> url(first->ParseUrl("https://example.test/client"));
    ExpectEqual(true, url != nullptr, "Legacy client URL parser failed");
    ExpectEqual(std::string_view("/client"), std::string_view(url->GetTarget()), "Legacy URL target mismatch");
}

void TestUrl(CreateInterfaceFn createInterface)
{
    auto factory = GetClientFactory(createInterface);
    struct UrlCase
    {
        const char* url;
        const char* scheme;
        const char* host;
        unsigned short port;
        const char* target;
        bool secure;
    };
    const UrlCase cases[] = {
        {"http://example.test", "http", "example.test", 80, "", false},
        {"https://example.test/path?key=value", "https", "example.test", 443, "/path?key=value", true},
        {"http://example.test:8080/path", "http", "example.test", 8080, "/path", false},
        {"http://example.test:65535/", "http", "example.test", 65535, "/", false},
        {"ws://example.test/socket", "ws", "example.test", 80, "/socket", false},
        {"wss://example.test/socket", "wss", "example.test", 443, "/socket", true},
        {"mqtt://example.test/topic", "mqtt", "example.test", 1883, "/topic", false},
        {"mqtts://example.test/topic", "mqtts", "example.test", 8883, "/topic", true}
    };
    for (const auto& test : cases)
    {
        Owned<IURLParsedResult> result(factory->ParseUrl(test.url));
        ExpectEqual(true, result != nullptr, "Valid URL was rejected");
        ExpectEqual(std::string_view(test.scheme), std::string_view(result->GetScheme()), "Scheme mismatch");
        ExpectEqual(std::string_view(test.host), std::string_view(result->GetHost()), "Host mismatch");
        ExpectEqual(test.port, result->GetPort(), "Port mismatch");
        ExpectEqual(std::to_string(test.port), std::string(result->GetPortString()), "Port string mismatch");
        ExpectEqual(std::string_view(test.target), std::string_view(result->GetTarget()), "Target mismatch");
        ExpectEqual(test.secure, result->IsSecure(), "Secure flag mismatch");
    }
    for (const char* url : {"", "example.test", "ftp://example.test/", "http:///path",
        "http://example.test:65536/", "http://example.test:999999999999999999999/", "http://example.test:abc/"})
    {
        Owned<IURLParsedResult> result(factory->ParseUrl(url));
        ExpectEqual(nullptr, result.get(), "Invalid URL was accepted");
    }
}
}

int wmain(int argc, wchar_t* argv[])
{
    try
    {
        if (argc != 4)
            throw std::runtime_error("Usage: SmokeTests <client DLL> <Steam DLL> <Factory|Client|Url>");
        Module steam(argv[2]);
        Module client(argv[1]);
        auto createInterface = client.GetFactory();
        const std::wstring_view scenario(argv[3]);
        if (scenario == L"Factory") TestFactory(createInterface);
        else if (scenario == L"Client") TestClient(createInterface);
        else if (scenario == L"Url") TestUrl(createInterface);
        else throw std::runtime_error("Unknown test scenario");
        std::cout << "PASS\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
