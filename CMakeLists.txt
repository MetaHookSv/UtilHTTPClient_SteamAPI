cmake_minimum_required(VERSION 3.21)
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(UtilHTTPClient_SteamAPI LANGUAGES CXX)

if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "UtilHTTPClient_SteamAPI requires Windows MSVC x86. Configure with -A Win32.")
endif()
get_property(multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(multi_config)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Supported configurations" FORCE)
elseif(NOT CMAKE_BUILD_TYPE MATCHES "^(Debug|Release)$")
    message(FATAL_ERROR "Select Debug or Release.")
endif()

include(cmake/Dependencies.cmake)
utilhttpclientsteamapi_prepare_dependencies()
include("${VC_LTL_Root}/VC-LTL helper for cmake.cmake")

add_library(SteamSDK::SteamAPI SHARED IMPORTED)
set_target_properties(SteamSDK::SteamAPI PROPERTIES
    IMPORTED_IMPLIB "${STEAMSDK_SOURCE_PATH}/lib/steam_api.lib"
    IMPORTED_LOCATION "${STEAMSDK_SOURCE_PATH}/bin/steam_api.dll"
    INTERFACE_INCLUDE_DIRECTORIES "${STEAMSDK_SOURCE_PATH}")

add_library(utilhttpclientsteamapi_settings INTERFACE)
target_compile_features(utilhttpclientsteamapi_settings INTERFACE cxx_std_20)
target_include_directories(utilhttpclientsteamapi_settings INTERFACE
    "${PROJECT_SOURCE_DIR}/include/Interface"
    "${METAHOOK_SOURCE_PATH}/include/HLSDK/common")
target_compile_definitions(utilhttpclientsteamapi_settings INTERFACE
    WIN32 _WINDOWS UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS
    $<$<CONFIG:Debug>:_DEBUG> $<$<CONFIG:Release>:NDEBUG>)
target_compile_options(utilhttpclientsteamapi_settings INTERFACE /W3 /permissive- /wd4819 /Zi)

add_library(UtilHTTPClient_SteamAPI SHARED
    src/UtilHTTPClient_SteamAPI.cpp src/dllmain.cpp
    "${METAHOOK_SOURCE_PATH}/include/HLSDK/common/interface.cpp")
target_link_libraries(UtilHTTPClient_SteamAPI PRIVATE utilhttpclientsteamapi_settings SteamSDK::SteamAPI)
target_include_directories(UtilHTTPClient_SteamAPI PRIVATE "${SCOPEEXIT_SOURCE_PATH}/include")
target_compile_definitions(UtilHTTPClient_SteamAPI PRIVATE UTILHTTPCLIENT_STEAMAPI_EXPORTS _USRDLL)
target_compile_options(UtilHTTPClient_SteamAPI PRIVATE /MP /sdl $<$<CONFIG:Release>:/Gy;/Oi>)
target_link_options(UtilHTTPClient_SteamAPI PRIVATE /DEBUG /SUBSYSTEM:WINDOWS /MANIFESTUAC:NO
    $<$<CONFIG:Debug>:/INCREMENTAL>
    $<$<CONFIG:Release>:/INCREMENTAL:NO;/OPT:REF;/OPT:ICF>)
set_target_properties(UtilHTTPClient_SteamAPI PROPERTIES
    DEBUG_POSTFIX "" INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
add_custom_command(TARGET UtilHTTPClient_SteamAPI POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:SteamSDK::SteamAPI>" "$<TARGET_FILE_DIR:UtilHTTPClient_SteamAPI>"
    VERBATIM)

include(CTest)
if(BUILD_TESTING)
    add_executable(UtilHTTPClientSteamAPITests tests/SmokeTests.cpp)
    target_link_libraries(UtilHTTPClientSteamAPITests PRIVATE utilhttpclientsteamapi_settings)
    target_compile_definitions(UtilHTTPClientSteamAPITests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    set_target_properties(UtilHTTPClientSteamAPITests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests")
    add_dependencies(UtilHTTPClientSteamAPITests UtilHTTPClient_SteamAPI)
    foreach(scenario Factory Client Url)
        add_test(NAME ${scenario} COMMAND UtilHTTPClientSteamAPITests
            "$<TARGET_FILE:UtilHTTPClient_SteamAPI>" "$<TARGET_FILE:SteamSDK::SteamAPI>" ${scenario})
        set_tests_properties(${scenario} PROPERTIES TIMEOUT 30)
    endforeach()
endif()

install(TARGETS UtilHTTPClient_SteamAPI RUNTIME DESTINATION svencoop/metahook/dlls)
install(FILES "$<TARGET_PDB_FILE:UtilHTTPClient_SteamAPI>" DESTINATION svencoop/metahook/dlls)
install(IMPORTED_RUNTIME_ARTIFACTS SteamSDK::SteamAPI RUNTIME DESTINATION .)
