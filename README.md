# UtilHTTPClient_SteamAPI

A standalone Steamworks HTTP client extracted from
[MetaHookSv](https://github.com/hzqst/MetaHookSv). It provides synchronous,
asynchronous and streaming requests through the existing `IUtilHTTPClient` API.

The port preserves `UtilHTTPClient_SteamAPI.dll`, the `CreateInterface` export,
`UtilHTTPClient_SteamAPI_007` and `UtilHTTPClientFactory_SteamAPI_007`.
Source baseline: `hzqst/MetaHookSv@fe80b6d60bfb487b52aed7ea7ec0492e7b27a5d2`,
`PluginLibs/UtilHTTPClient_SteamAPI` and `include/Interface/IUtilHTTPClient.h`.

[简体中文](README.zh-CN.md)

## Build and test

Requirements: Windows, Visual Studio 2022 with C++ x86 tools and Windows SDK,
CMake 3.21 or newer, and Git. Builds use C++20, static MSVC runtime and VC-LTL 5.3.1.

```bat
git clone --recurse-submodules https://github.com/MetaHookSv/UtilHTTPClient_SteamAPI
cd UtilHTTPClient_SteamAPI
scripts\build-UtilHTTPClient_SteamAPI-x86-Release.bat
scripts\build-UtilHTTPClient_SteamAPI-x86-Debug.bat
```

For an existing checkout, initialize the SDK with:

```bat
git submodule update --init --recursive
```

Each build script configures, builds, runs CTest and installs, stopping on failure.
Equivalent Release commands:

```bat
cmake -S . -B build/x86/Release -G "Visual Studio 17 2022" -A Win32 -DCMAKE_INSTALL_PREFIX=install/x86/Release
cmake --build build/x86/Release --config Release --parallel
ctest --test-dir build/x86/Release -C Release --output-on-failure
cmake --install build/x86/Release --config Release
```

`BUILD_TESTING` defaults to `ON`; use `-DBUILD_TESTING=OFF` for a library-only build.
Other generators must select MSVC x86. Single-config generators require
`-DCMAKE_BUILD_TYPE=Debug` or `Release`.

## Dependencies

- [SteamSDK](https://github.com/MetaHookSv/SteamSDK) is the `thirdparty/SteamSDK`
  Git submodule pinned to `3c1abaf6277f9f99fd16ef40557d6852820b848f`.
  The build uses its headers, x86 import library and runtime.
- The first configure fetches the MetaHook SDK at
  `4d23b6fecd79dc949aabc2e145480cd1328d4a35` and ScopeExit at
  `bd345da594a4675d04de663d93d00cb81b6678b2`, without configuring upstream projects.
- VC-LTL 5.3.1 is downloaded into `thirdparty/cache` and verified against SHA-256
  `7a18799ed3aa84a225610a5447a56bc534c5c98ccb8dec05caba0e3f633431ad`.

Use existing dependencies for local or offline builds after initializing SteamSDK:

```bat
scripts\build-UtilHTTPClient_SteamAPI-x86-Release.bat -DMETAHOOK_SOURCE_PATH=D:\MetaHook -DSCOPEEXIT_SOURCE_PATH=D:\ScopeExit -DVC_LTL_Root=D:\VC-LTL-5.3.1
```

These names also accept environment variables when initializing the CMake cache;
explicit cache values take precedence. External directories are read-only.
`UTILHTTPCLIENT_STEAMAPI_DEPENDENCY_CACHE_DIR` overrides the binary download cache.

## Deployment and lifecycle

The install tree is `install/x86/<Configuration>`:

- DLL and PDB: `svencoop/metahook/dlls`.
- SDK runtime: `steam_api.dll` at the install root.
- Consumer headers: `include/Interface` and `include/HLSDK/common`; add both include paths.
- README files, licenses and third-party notices are included.

Load the client DLL through `CreateInterface`, request
`UTIL_HTTPCLIENT_FACTORY_STEAMAPI_INTERFACE_VERSION`, then call `CreateUtilHTTPClient()`.
This is a consumer-loaded utility DLL; it requires no `plugins.lst` entry.
Deploy the bundled `steam_api.dll` to the game executable directory when the host
does not already supply a compatible runtime.

The host must initialize Steamworks and regularly dispatch Steam callbacks.
`RunFrame()` only cleans up completed requests; it does not run Steam callbacks.
Synchronous waits require callback dispatch to continue on another thread and
must not block the thread responsible for that dispatch.

Call `Init()` with a valid creation context. Requests own their callback objects
and call `Destroy()` on them when released; use callbacks whose `Destroy()` frees
them in their allocating module. Sync requests remain caller-owned. Async requests
default to automatic cleanup when added to the request pool; otherwise callers
manage their lifetime. Pooled requests should be accessed through their IDs.
Use `Shutdown()` to release the pool, then `Destroy()` the client before Steam
shutdown or DLL unload. URL results must also be released with `Destroy()`.

`SetFollowLocation()` is unsupported by this backend. Async responses are consumed
through callbacks. The existing parser and HTTP behavior are retained.

## CI and verification

Main pushes, pull requests and manual runs build, test and package Windows x86
Release on `windows-2022`. The complete install tree is uploaded as
`UtilHTTPClient_SteamAPI-windows-x86.7z`. A `v*` tag publishes the same archive in a
GitHub Release.

Three CTest smoke scenarios load the actual DLL and SDK runtime and exercise
interface lookup, client creation/destruction, empty pool operations and URL parsing.
Checks remain active in Release and each scenario has a 30-second timeout.
They require no Steam login, game installation or external HTTP requests; live
Steam HTTP requests are outside these smoke tests.

## License

MIT for the client code; see [LICENSE](LICENSE) and
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md) for dependency terms.
