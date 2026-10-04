set(UTILHTTPCLIENT_STEAMAPI_DEPENDENCY_CACHE_DIR "${PROJECT_SOURCE_DIR}/thirdparty/cache" CACHE PATH "Downloaded binary dependency cache")
set(METAHOOK_SOURCE_PATH "$ENV{METAHOOK_SOURCE_PATH}" CACHE PATH "MetaHook source tree; empty fetches the pinned SDK")
set(SCOPEEXIT_SOURCE_PATH "$ENV{SCOPEEXIT_SOURCE_PATH}" CACHE PATH "ScopeExit source tree; empty fetches the pinned commit")
set(STEAMSDK_SOURCE_PATH "$ENV{STEAMSDK_SOURCE_PATH}" CACHE PATH "SteamSDK header source tree; empty fetches the pinned commit")
set(VC_LTL_Root "$ENV{VC_LTL_Root}" CACHE PATH "Existing VC-LTL binary package; empty downloads the verified package")

function(utilhttpclientsteamapi_require_files name source)
    foreach(required IN LISTS ARGN)
        if(NOT EXISTS "${source}/${required}" OR IS_DIRECTORY "${source}/${required}")
            message(FATAL_ERROR "${name} is missing ${required}: ${source}")
        endif()
    endforeach()
endfunction()

function(utilhttpclientsteamapi_fetch_source name url commit out_var)
    include(FetchContent)
    FetchContent_Declare(${name}
        GIT_REPOSITORY "${url}" GIT_TAG "${commit}"
        GIT_SUBMODULES "" GIT_SUBMODULES_RECURSE FALSE
        # Populate the source without configuring upstream projects.
        SOURCE_SUBDIR _utilhttpclientsteamapi_source_only)
    FetchContent_MakeAvailable(${name})
    set(${out_var} "${${name}_SOURCE_DIR}" PARENT_SCOPE)
endfunction()

function(utilhttpclientsteamapi_prepare_dependencies)
    # SteamSDK may come from a shared external tree (STEAMSDK_SOURCE_PATH).
    if(STEAMSDK_SOURCE_PATH)
        get_filename_component(steam_sdk "${STEAMSDK_SOURCE_PATH}" ABSOLUTE BASE_DIR "${PROJECT_SOURCE_DIR}")
    else()
        utilhttpclientsteamapi_fetch_source(utilhttpclientsteamapi_steamsdk
            https://github.com/MetaHookSv/SteamSDK
            3c1abaf6277f9f99fd16ef40557d6852820b848f steam_sdk)
    endif()
    foreach(required steam/steam_api.h STEAM-SDK-NOTICE.md)
        if(NOT EXISTS "${steam_sdk}/${required}")
            message(FATAL_ERROR "SteamSDK is missing ${required}: ${steam_sdk}. Set STEAMSDK_SOURCE_PATH or check the pinned commit.")
        endif()
    endforeach()
    set(STEAMSDK_SOURCE_PATH "${steam_sdk}" PARENT_SCOPE)
    message(STATUS "STEAMSDK_SOURCE_PATH: ${steam_sdk}")
    set(metahook_files include/HLSDK/common/interface.h include/HLSDK/common/interface.cpp LICENSE)
    set(scopeexit_files include/ScopeExit/ScopeExit.h LICENSE)
    set(vcltl_files "VC-LTL helper for cmake.cmake" config/config.cmake
        TargetPlatform/6.0.6000.0/lib/Win32/libucrt.lib Readme.md)

    # Validate explicit paths before downloads. External directories are read-only.
    foreach(dependency METAHOOK SCOPEEXIT)
        if(${dependency}_SOURCE_PATH)
            get_filename_component(source "${${dependency}_SOURCE_PATH}" ABSOLUTE BASE_DIR "${PROJECT_SOURCE_DIR}")
            string(TOLOWER "${dependency}" lower_name)
            utilhttpclientsteamapi_require_files("${dependency}_SOURCE_PATH" "${source}" ${${lower_name}_files})
            set(${dependency}_SOURCE_PATH "${source}")
        endif()
    endforeach()
    if(VC_LTL_Root)
        get_filename_component(VC_LTL_Root "${VC_LTL_Root}" ABSOLUTE BASE_DIR "${PROJECT_SOURCE_DIR}")
        utilhttpclientsteamapi_require_files(VC_LTL_Root "${VC_LTL_Root}" ${vcltl_files})
    endif()

    if(NOT METAHOOK_SOURCE_PATH)
        utilhttpclientsteamapi_fetch_source(utilhttpclientsteamapi_metahook
            https://github.com/MetaHookSv/MetaHook
            4d23b6fecd79dc949aabc2e145480cd1328d4a35 METAHOOK_SOURCE_PATH)
    endif()
    if(NOT SCOPEEXIT_SOURCE_PATH)
        utilhttpclientsteamapi_fetch_source(utilhttpclientsteamapi_scopeexit
            https://github.com/SergiusTheBest/ScopeExit
            bd345da594a4675d04de663d93d00cb81b6678b2 SCOPEEXIT_SOURCE_PATH)
    endif()
    foreach(dependency METAHOOK SCOPEEXIT)
        string(TOLOWER "${dependency}" lower_name)
        utilhttpclientsteamapi_require_files("${dependency}_SOURCE_PATH" "${${dependency}_SOURCE_PATH}" ${${lower_name}_files})
        set(${dependency}_SOURCE_PATH "${${dependency}_SOURCE_PATH}" PARENT_SCOPE)
        message(STATUS "${dependency}_SOURCE_PATH: ${${dependency}_SOURCE_PATH}")
    endforeach()

    if(NOT VC_LTL_Root)
        include("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/VCLTL.cmake")
        set(VC_LTL_Root "${UTILHTTPCLIENT_STEAMAPI_DEPENDENCY_CACHE_DIR}/VC-LTL-5.3.1")
        utilhttpclientsteamapi_prepare_vcltl()
    endif()
    utilhttpclientsteamapi_require_files(VC_LTL_Root "${VC_LTL_Root}" ${vcltl_files})
    set(VC_LTL_Root "${VC_LTL_Root}" PARENT_SCOPE)
    message(STATUS "VC_LTL_Root: ${VC_LTL_Root}")
endfunction()
