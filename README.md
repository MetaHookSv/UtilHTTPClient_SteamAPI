# UtilHTTPClient_SteamAPI

[简体中文](README.zh-CN.md)

UtilHTTPClient_SteamAPI is a standalone Steamworks HTTP client. It implements the
`IUtilHTTPClient` API on top of Steamworks and provides synchronous, asynchronous and
streaming requests to the host application.

* Windows x86 only, built with Visual Studio 2022, C++20 and VC-LTL 5.3.1.
* Exports `CreateInterface`, plus the `UtilHTTPClient_SteamAPI_007` and
  `UtilHTTPClientFactory_SteamAPI_007` interfaces.
* `SetFollowLocation()` is unsupported by this backend.

## Quick start

Download `UtilHTTPClient_SteamAPI-windows-x86.7z` from
[GitHub Releases](https://github.com/MetaHookSv/UtilHTTPClient_SteamAPI/releases) (built on `v*` tag pushes).

The install tree holds the client and SteamAPIBridge DLL/PDB pairs under
`svencoop/metahook/dlls`. Load the client
DLL through `CreateInterface`, request
`UTIL_HTTPCLIENT_FACTORY_STEAMAPI_INTERFACE_VERSION`, then call `CreateUtilHTTPClient()`.
This is a consumer-loaded utility DLL and needs no `plugins.lst` entry. The host must
initialize Steamworks and keep dispatching Steam callbacks. Its existing
`steam_api.dll` is used without replacement. SteamAPIBridge requests HTTP003 on
both modern runtimes and legacy SteamClient012 runtimes; unavailable HTTP or failed
request setup/send completes the request with an error instead of waiting forever.

Requests accept HTTP/HTTPS URLs, preserve TLS with explicit ports, and omit URL
fragments. Steam supplies the default User-Agent; setting `User-Agent` through
`SetField` is rejected by Steam and fails request setup. Body extraction failures
also finish with an error. Streaming response headers are readable in the
`Responding` notification.

An added request belongs to one pool; repeated additions are ignored. Calling
`Destroy()` (including from a request callback) removes it from its pool before
reclamation. Pool cleanup invokes consumer destructors without holding the pool
lock. Use client/request operations and the Steam callback pump on the same owner
thread; a separate thread may wait for a synchronous result while its request is
kept alive. The pool mutex does not make arbitrary concurrent request use safe.

## Build

Windows with Visual Studio 2022 C++ x86 tools and the Windows SDK, CMake 3.21 or newer
and Git. Debug and Release both use C++20, the static MSVC runtime and VC-LTL 5.3.1.

```bat
git clone --recurse-submodules https://github.com/MetaHookSv/UtilHTTPClient_SteamAPI
cd UtilHTTPClient_SteamAPI
scripts\build-UtilHTTPClient_SteamAPI-x86-Release.bat
```

Each build script configures, builds, runs CTest and installs into
`install/x86/<Configuration>`, stopping on failure. `BUILD_TESTING` defaults to `ON`;
use `-DBUILD_TESTING=OFF` for a library-only build. The first configure fetches
SteamSDK headers, SteamAPIBridge, MetaHook SDK and ScopeExit at pinned commits;
pass `-DSTEAMAPIBRIDGE_SOURCE_PATH=... -DSTEAMSDK_SOURCE_PATH=...`
and `-DMETAHOOK_SOURCE_PATH=... -DSCOPEEXIT_SOURCE_PATH=... -DVC_LTL_Root=...`
to reuse local copies for offline builds.

## License

MIT for the client code; each dependency keeps its own license. See [LICENSE](LICENSE);
dependency terms are documented in the upstream repositories.
