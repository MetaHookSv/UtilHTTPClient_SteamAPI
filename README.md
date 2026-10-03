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

The archive holds `steam_api.dll` at its root, the client DLL and PDB under
`svencoop/metahook/dlls`, and the consumer headers under `include`. Load the client
DLL through `CreateInterface`, request
`UTIL_HTTPCLIENT_FACTORY_STEAMAPI_INTERFACE_VERSION`, then call `CreateUtilHTTPClient()`.
This is a consumer-loaded utility DLL and needs no `plugins.lst` entry. The host must
initialize Steamworks and keep dispatching Steam callbacks; deploy the bundled
`steam_api.dll` to the game executable directory when the host does not already supply
a compatible runtime.

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
use `-DBUILD_TESTING=OFF` for a library-only build. Besides the `thirdparty/SteamSDK`
submodule, the first configure fetches the MetaHook SDK and ScopeExit at pinned
commits; pass `-DMETAHOOK_SOURCE_PATH=... -DSCOPEEXIT_SOURCE_PATH=... -DVC_LTL_Root=...`
to reuse local copies for offline builds.

## License

MIT for the client code; each dependency keeps its own license. See [LICENSE](LICENSE);
dependency terms are documented in the upstream repositories.
