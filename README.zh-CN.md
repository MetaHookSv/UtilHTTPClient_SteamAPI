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

安装目录的 `svencoop/metahook/dlls` 下包含客户端和 SteamAPIBridge 的 DLL/PDB。
加载客户端 DLL 的 `CreateInterface`，取得
`UTIL_HTTPCLIENT_FACTORY_STEAMAPI_INTERFACE_VERSION` 对应工厂，再调用
`CreateUtilHTTPClient()`。此工具库由消费者加载，无需添加 `plugins.lst` 条目。
宿主需初始化 Steamworks 并持续分发 Steam 回调；Bridge 使用游戏已有的
`steam_api.dll`，不替换该文件。新版运行库和旧 SteamClient012 路径都请求 HTTP003；
HTTP 不可用或请求设置、发送失败时，请求以错误结束，不会永久等待。

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
首次配置按固定提交获取 SteamSDK 头文件、SteamAPIBridge、MetaHook SDK 和
ScopeExit；可通过 `-DSTEAMAPIBRIDGE_SOURCE_PATH=... -DSTEAMSDK_SOURCE_PATH=...`，以及
`-DMETAHOOK_SOURCE_PATH=... -DSCOPEEXIT_SOURCE_PATH=... -DVC_LTL_Root=...`
复用本地副本进行离线构建。

## 许可证

客户端代码采用 MIT；各依赖保留自身许可证，见 [LICENSE](LICENSE)，依赖条款请查阅
各自上游仓库。
