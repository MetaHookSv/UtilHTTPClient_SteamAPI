# UtilHTTPClient_SteamAPI

从 [MetaHookSv](https://github.com/hzqst/MetaHookSv) 提取的独立 Steamworks HTTP
客户端，通过现有 `IUtilHTTPClient` API 提供同步、异步及流式请求。

保留 `UtilHTTPClient_SteamAPI.dll`、`CreateInterface` 导出和两个 `_007` 接口。
源基线为 `fe80b6d60bfb487b52aed7ea7ec0492e7b27a5d2`，迁入
`PluginLibs/UtilHTTPClient_SteamAPI` 实现和 `IUtilHTTPClient.h`。

## 构建与测试

需要 Windows、Visual Studio 2022 C++ x86 工具、Windows SDK、CMake 3.21 以上和 Git。
Debug、Release 均使用 C++20、静态 MSVC 运行库及 VC-LTL 5.3.1。

```bat
git clone --recurse-submodules https://github.com/MetaHookSv/UtilHTTPClient_SteamAPI
cd UtilHTTPClient_SteamAPI
scripts\build-UtilHTTPClient_SteamAPI-x86-Release.bat
scripts\build-UtilHTTPClient_SteamAPI-x86-Debug.bat
```

已有检出目录先运行 `git submodule update --init --recursive`。
脚本依次配置、构建、执行 CTest 和安装，失败立即退出。
直接使用 CMake 的等效命令见 [英文文档](README.md#build-and-test)。
`BUILD_TESTING` 默认开启；`-DBUILD_TESTING=OFF` 可只构建库。
其他生成器也需选择 MSVC x86；单配置生成器必须设置 Debug 或 Release。

## 依赖

- SteamSDK 使用 `thirdparty/SteamSDK` 子模块，固定提交
  `3c1abaf6277f9f99fd16ef40557d6852820b848f`，提供头文件、x86 导入库和运行库。
- 首次配置自动获取 MetaHook SDK
  `4d23b6fecd79dc949aabc2e145480cd1328d4a35`、ScopeExit
  `bd345da594a4675d04de663d93d00cb81b6678b2`，仅使用所需源码。
- VC-LTL 5.3.1 下载至 `thirdparty/cache`，校验 SHA-256
  `7a18799ed3aa84a225610a5447a56bc534c5c98ccb8dec05caba0e3f633431ad`。

初始化子模块后可指定本地依赖进行离线构建：

```bat
scripts\build-UtilHTTPClient_SteamAPI-x86-Release.bat -DMETAHOOK_SOURCE_PATH=D:\MetaHook -DSCOPEEXIT_SOURCE_PATH=D:\ScopeExit -DVC_LTL_Root=D:\VC-LTL-5.3.1
```

同名环境变量可初始化 CMake cache，显式 cache 参数优先；外部依赖目录只读。
`UTILHTTPCLIENT_STEAMAPI_DEPENDENCY_CACHE_DIR` 可覆盖二进制依赖缓存路径。

## 部署与生命周期

安装目录为 `install/x86/<Configuration>`。DLL、PDB 位于
`svencoop/metahook/dlls`，`steam_api.dll` 位于安装根目录，公共头文件位于
`include/Interface` 和 `include/HLSDK/common`；消费者需添加两个 include 路径。
包内包含 README、许可证和第三方声明。

加载 DLL 的 `CreateInterface`，取得
`UTIL_HTTPCLIENT_FACTORY_STEAMAPI_INTERFACE_VERSION` 对应工厂，再调用
`CreateUtilHTTPClient()`。此工具库由消费者加载，无需添加 `plugins.lst` 条目。
宿主未提供兼容运行库时，将随包附带的 `steam_api.dll` 放到游戏可执行文件目录。

宿主负责初始化 Steamworks 并持续分发 Steam 回调；`RunFrame()` 只回收请求，
不驱动 Steam 回调。同步等待期间必须由另一线程继续驱动回调，不可阻塞负责
回调分发的线程。

使用有效创建上下文调用 `Init()`。请求拥有回调对象，并在释放时调用其
`Destroy()`；回调应在分配它的模块中释放自身。同步请求由调用者销毁。
异步请求默认在完成后自动回收，但需加入请求池；未入池时由调用者管理。
跨帧访问池内请求应使用 ID。卸载 DLL 或关闭 Steam 前，先 `Shutdown()`
释放请求池，再 `Destroy()` 客户端。URL 解析结果也需调用 `Destroy()`。

保留现有 HTTP 和 URL 解析行为：此后端不支持 `SetFollowLocation()`，
异步响应通过回调消费。

## CI 与验证

main push、PR 和手动运行在 `windows-2022` 构建、测试并打包 x86 Release。
完整安装目录发布为 `UtilHTTPClient_SteamAPI-windows-x86.7z` artifact；
`v*` 标签通过相同流程创建 GitHub Release。

三个 CTest 冒烟场景加载真实 DLL 和 SDK 运行库，检查接口、客户端生命周期、
空请求池和 URL 解析。Release 下断言仍有效，每个场景超时 30 秒。
测试无需 Steam 登录、游戏安装或外网请求；不包含真实 Steam HTTP 请求验证。

## 许可证

客户端代码采用 MIT；依赖条款见 [LICENSE](LICENSE) 和
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md)。
