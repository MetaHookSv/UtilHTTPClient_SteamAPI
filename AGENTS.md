# AGENTS.md

This file provides guidance and important rules working with code in this repository.

## When coding / building plan

- Use a progressive disclosure approach for agent coding in this repository: start from high-level
  information in the Basic Memory knowledge base first, and only locate/read specific files or
  symbols when necessary, instead of expanding a large amount of context at once.

#### Basic Memory knowledge base (project-scoped, `memory/`)

- Notes live in `memory/` (markdown with YAML frontmatter: `title`/`type`/`permalink`), tracked in git.
- This repository contains the standalone UtilHTTPClient_SteamAPI library, extracted from MetaHookSv
  `PluginLibs/UtilHTTPClient_SteamAPI` (the migrated note is titled "UtilHTTPClient_Steam"). See
  `memory/project_overview.md` for scope and provenance.
- Basic Memory is registered as MCP server `basic-memory`, pinned to the `utilhttpclient-steamapi`
  project (project-level `.mcp.json`, mirrored by `.codex/config.toml`). The `metahooksv` project
  belongs to the source repository.
- Prefer Basic Memory MCP tools (`search_notes` / `read_note` / `write_note` / `edit_note`) only when
  their project resolves to this repository's `memory/` directory. Verify the project binding before
  writing; when no matching project is available, read and edit the local markdown files directly.
- Notes use the `utilhttpclient-steamapi/` permalink prefix to distinguish them from the source
  repository.
- Historical records are not current evidence: the migrated note retains MetaHookSv paths
  (`PluginLibs/UtilHTTPClient_SteamAPI/`, `.vcxproj` property names), while the current sources are
  `src/<file>` and the build is CMake. Do not extend an old statement to a new change without
  checking the code.

#### High-level information in this repository (read corresponding notes first)

- Project overview, provenance, Steam request lifecycle, host requirements and backend limitation:
  `project_overview`

#### When notes are insufficient: source entry points (query and read on demand)

- Build: `CMakeLists.txt` (including the `SteamSDK::SteamAPI` imported target), `cmake/`,
  `scripts/build-UtilHTTPClient_SteamAPI-x86-{Debug,Release}.bat`
- Library sources: `src/UtilHTTPClient_SteamAPI.cpp` (client, request family, response family, URL
  parsing and the exports at the end of the file), `src/dllmain.cpp`
- Public API / interface: `include/Interface/IUtilHTTPClient.h` — shared with the libcurl backend,
  so a change here affects both repositories
- Steamworks: `thirdparty/SteamSDK` (git submodule; clone with `--recurse-submodules`), consumed as
  `steam_api.h` plus `steam_api.lib`
- Tests: `tests/SmokeTests.cpp`, run by CTest (`BUILD_TESTING` defaults to `ON` in the scripts)
- Docs: `README.md`, `README.zh-CN.md`; dependency terms live in the upstream repositories
- External sources, all read-only inputs: `METAHOOK_SOURCE_PATH`, `SCOPEEXIT_SOURCE_PATH` and
  `VC_LTL_Root`; pins are fetched only when the corresponding override is empty. The build uses
  C++20, a static CRT and VC-LTL 5.3.1, for Windows x86 only
- Build output: `build/x86/<configuration>/`; install output: `install/x86/<configuration>/`. Neither
  is tracked, and nothing is deployed into a game

#### Progressive disclosure key points

- Read notes first, then locate a single file/symbol; do not read the whole repository at once.
- Prefer correctly scoped Basic Memory MCP tools for knowledge retrieval; otherwise use the local
  notes before reading source.
- Prefer Context7 for external dependency/library usage (query on demand).

## Repository rules

- Preserve the exported interface versions (`UtilHTTPClient_SteamAPI_007` and
  `UtilHTTPClientFactory_SteamAPI_007`), the `IUtilHTTPClient` vtable layout and the shared header's
  ABI unless an interface change is explicitly requested; the libcurl backend and its consumers must
  keep compiling against the same header.
- Keep the Steam-side contracts intact: `RunFrame()` is a pool janitor only (Steam drives transfers
  through callbacks), the request destroys its `IUtilHTTPCallbacks`, and a synchronous wait depends
  on the host pumping `SteamAPI_RunCallbacks()`.
- Known gaps are documented behavior, not bugs to silently paper over: `SetFollowLocation()` is
  unsupported by the backend, `GetResponseErrorMessage()` is always empty, and the async wait/response
  methods are stubs. If one changes, update `memory/project_overview.md` and both READMEs together.
- Do not modify external dependency trees or fetched vendor sources, including the `thirdparty/SteamSDK`
  submodule and the Steamworks import library. Explicit source trees are read-only inputs.
- Run both x86 Debug and Release build scripts for relevant changes; they configure, build, run CTest
  and install. Keep tests focused on public behavior and DLL integration.
- Build, install and dependency-cache directories are ignored. Do not commit, push or publish
  without authorization.

## Explore SKILLs

- Project-level skills, when present, live in `.claude/skills` no matter what harness tool is being
  used.
