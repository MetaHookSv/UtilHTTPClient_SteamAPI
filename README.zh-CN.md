# UtilHTTPClient_SteamAPI

[English](README.md)

独立的 Steamworks HTTP 客户端，在 Steamworks 之上实现 `IUtilHTTPClient` API，
为宿主提供同步、异步和流式请求。

* 仅支持 Windows x86，使用 Visual Studio 2022、C++20 和 VC-LTL 5.3.1 构建。
* 导出 `CreateInterface`，以及 `UtilHTTPClient_SteamAPI_007` 和
  `UtilHTTPClientFactory_SteamAPI_007` 两个接口。
* 此后端不支持 `SetFollowLocation()`。

## 快速开始

从 [GitHub Releases](https://github.com/MetaHookSv/UtilHTTPClient_SteamAPI/releases)
下载 `UtilHTTPClient_SteamAPI-windows-x86.7z`（由 `v*` 标签推送构建）。

包内根目录为 `steam_api.dll`，并含客户端 DLL、PDB、公共头文件和许可证文件。
加载客户端 DLL 的 `CreateInterface`，取得
`UTIL_HTTPCLIENT_FACTORY_STEAMAPI_INTERFACE_VERSION` 对应工厂，再调用
`CreateUtilHTTPClient()`。此工具库由消费者加载，无需添加 `plugins.lst` 条目。
宿主需初始化 Steamworks 并持续分发 Steam 回调；宿主未提供兼容运行库时，将随包的
`steam_api.dll` 放到游戏可执行文件目录。

## 构建

需要 Windows、Visual Studio 2022 C++ x86 工具和 Windows SDK、CMake 3.21 以上及 Git。
Debug、Release 均使用 C++20、静态 MSVC 运行库及 VC-LTL 5.3.1。

```bat
git clone --recurse-submodules https://github.com/MetaHookSv/UtilHTTPClient_SteamAPI
cd UtilHTTPClient_SteamAPI
scripts\build-UtilHTTPClient_SteamAPI-x86-Release.bat
```

每个构建脚本依次配置、构建、执行 CTest 并安装到 `install/x86/<Configuration>`，
失败立即退出。`BUILD_TESTING` 默认开启，`-DBUILD_TESTING=OFF` 可只构建库。
除 `thirdparty/SteamSDK` 子模块外，首次配置还会按固定提交获取 MetaHook SDK 和
ScopeExit；可通过 `-DMETAHOOK_SOURCE_PATH=... -DSCOPEEXIT_SOURCE_PATH=... -DVC_LTL_Root=...`
复用本地副本进行离线构建。

## 许可证

客户端代码采用 MIT；各依赖保留自身许可证，见 [LICENSE](LICENSE) 和
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md)。
