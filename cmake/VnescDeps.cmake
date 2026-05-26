#==============================================================================
# Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
# Licensed under the Apache License, Version 2.0 (the "License")
#
# VnescDeps.cmake
# ───────────────
# Wires external compiler dependencies into the vnesc build:
#   - SPIRV-Cross  (always, for MSL/WGSL cross-compilation and reflection)
#   - glslang      (when VNE_SC_GLSLANG=ON, for GLSL→SPIR-V)
#   - SPIRV-Tools  (when VNE_SC_SPIRVTOOLS=ON, for validation)
#
# Source resolution order for each dep (mirrors vnerhi/cmake/SpirvReflection.cmake):
#   1. -DVNE_SC_<DEP>_DIR=<path>           explicit developer override
#   2. deps/external/<dep>                  git submodule (preferred)
#   3. FetchContent at the pinned tag       ad-hoc clone fallback
#
# All deps are pinned to vulkan-sdk-1.3.296.0 for consistency with vnerhi.
#==============================================================================

cmake_minimum_required(VERSION 3.20)

set(_vne_sc_sdk_tag "vulkan-sdk-1.3.296.0")
get_filename_component(_vne_sc_repo_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

# ──────────────────────────────────────────────────────────────────────────────
# Helper: resolve dep root (vendored / override / FetchContent)
# ──────────────────────────────────────────────────────────────────────────────
function(_vne_sc_resolve_dep _name _vendored _override_cache _out_src _out_origin)
    set(_src "")
    set(_origin "")
    if(DEFINED ${_override_cache} AND NOT "${${_override_cache}}" STREQUAL "")
        get_filename_component(_override_path "${${_override_cache}}" REALPATH)
        if(EXISTS "${_override_path}/CMakeLists.txt")
            set(_src "${_override_path}")
            set(_origin "local:${_override_path}")
        else()
            message(WARNING
                "vnesc: ${_override_cache}='${${_override_cache}}' has no CMakeLists.txt; "
                "ignoring override for ${_name}")
        endif()
    endif()
    if(NOT _src AND EXISTS "${_vendored}/CMakeLists.txt")
        set(_src "${_vendored}")
        set(_origin "vendored:${_vendored}")
    endif()
    set(${_out_src}    "${_src}"    PARENT_SCOPE)
    set(${_out_origin} "${_origin}" PARENT_SCOPE)
endfunction()

include(FetchContent)

# Keep PIC state so static archives can be linked into shared libs on Linux.
set(_vne_sc_prev_pic "${CMAKE_POSITION_INDEPENDENT_CODE}")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ══════════════════════════════════════════════════════════════════════════════
# SPIRV-Cross (always required — cross-compilation + reflection)
# ══════════════════════════════════════════════════════════════════════════════
set(_vne_sc_spirvcross_vendored "${_vne_sc_repo_root}/deps/external/SPIRV-Cross")
if(EXISTS "${_vne_sc_spirvcross_vendored}/CMakeLists.txt")
    set(_vne_sc_spirvcross_dir_init "${_vne_sc_spirvcross_vendored}")
else()
    set(_vne_sc_spirvcross_dir_init "")
endif()
set(VNE_SC_SPIRV_CROSS_DIR "${_vne_sc_spirvcross_dir_init}" CACHE PATH
    "SPIRV-Cross source root. Empty: FetchContent. Default: deps/external/SPIRV-Cross if present." FORCE)

set(SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_TESTS             OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_GLSL              ON  CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_HLSL              OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_MSL               ON  CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_CPP               OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_REFLECT           OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_C_API             OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_SKIP_INSTALL             ON  CACHE BOOL "" FORCE)
set(SPIRV_CROSS_SHARED                   OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_STATIC                   ON  CACHE BOOL "" FORCE)
set(SPIRV_CROSS_CLI                      OFF CACHE BOOL "" FORCE)

_vne_sc_resolve_dep(SPIRV-Cross
    "${_vne_sc_spirvcross_vendored}" VNE_SC_SPIRV_CROSS_DIR
    _vne_sc_spirvcross_src _vne_sc_spirvcross_origin)

if(_vne_sc_spirvcross_src)
    add_subdirectory("${_vne_sc_spirvcross_src}"
                     "${CMAKE_BINARY_DIR}/deps/external/spirv-cross" EXCLUDE_FROM_ALL)
else()
    FetchContent_Declare(spirv_cross
        GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Cross.git
        GIT_TAG        ${_vne_sc_sdk_tag}
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(spirv_cross)
    FetchContent_GetProperties(spirv_cross SOURCE_DIR _vne_sc_spirvcross_src)
    set(_vne_sc_spirvcross_origin "fetch:${_vne_sc_sdk_tag}")
endif()

set(VNE_SC_SPIRV_CROSS_INCLUDE_DIR "${_vne_sc_spirvcross_src}" CACHE INTERNAL
    "SPIRV-Cross header root (bare-name includes)")

foreach(_t spirv-cross-core spirv-cross-glsl spirv-cross-msl)
    if(TARGET ${_t})
        get_target_property(_inc ${_t} INTERFACE_INCLUDE_DIRECTORIES)
        if(_inc AND NOT _inc STREQUAL "_inc-NOTFOUND")
            set_target_properties(${_t} PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_inc}")
        endif()
        set_target_properties(${_t} PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
    endif()
endforeach()

message(STATUS "vnesc: SPIRV-Cross (${_vne_sc_spirvcross_origin})")

# ══════════════════════════════════════════════════════════════════════════════
# glslang  (when VNE_SC_GLSLANG=ON)
# ══════════════════════════════════════════════════════════════════════════════
if(VNE_SC_GLSLANG)
    # SPIRV-Headers — required by glslang (not by SPIRV-Cross / vnesc_spirvcross)
    set(_vne_sc_spirvhdr_vendored "${_vne_sc_repo_root}/deps/external/SPIRV-Headers")
    if(EXISTS "${_vne_sc_spirvhdr_vendored}/CMakeLists.txt")
        set(_vne_sc_spirvhdr_dir_init "${_vne_sc_spirvhdr_vendored}")
    else()
        set(_vne_sc_spirvhdr_dir_init "")
    endif()
    set(VNE_SC_SPIRV_HEADERS_DIR "${_vne_sc_spirvhdr_dir_init}" CACHE PATH
        "SPIRV-Headers source root. Empty: FetchContent. Default: deps/external/SPIRV-Headers if present." FORCE)
    _vne_sc_resolve_dep(SPIRV-Headers
        "${_vne_sc_spirvhdr_vendored}" VNE_SC_SPIRV_HEADERS_DIR
        _vne_sc_spirvhdr_src _vne_sc_spirvhdr_origin)
    if(_vne_sc_spirvhdr_src)
        add_subdirectory("${_vne_sc_spirvhdr_src}"
                         "${CMAKE_BINARY_DIR}/deps/external/spirv-headers" EXCLUDE_FROM_ALL)
    else()
        FetchContent_Declare(spirv_headers
            GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Headers.git
            GIT_TAG        ${_vne_sc_sdk_tag} GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(spirv_headers)
        FetchContent_GetProperties(spirv_headers SOURCE_DIR _vne_sc_spirvhdr_src)
        set(_vne_sc_spirvhdr_origin "fetch:${_vne_sc_sdk_tag}")
    endif()
    set(SPIRV-Headers_SOURCE_DIR "${_vne_sc_spirvhdr_src}" CACHE PATH "" FORCE)

    # glslang
    set(_vne_sc_glslang_vendored "${_vne_sc_repo_root}/deps/external/glslang")
    if(EXISTS "${_vne_sc_glslang_vendored}/CMakeLists.txt")
        set(_vne_sc_glslang_dir_init "${_vne_sc_glslang_vendored}")
    else()
        set(_vne_sc_glslang_dir_init "")
    endif()
    set(VNE_SC_GLSLANG_DIR "${_vne_sc_glslang_dir_init}" CACHE PATH
        "glslang source root. Empty: FetchContent. Default: deps/external/glslang if present." FORCE)
    _vne_sc_resolve_dep(glslang
        "${_vne_sc_glslang_vendored}" VNE_SC_GLSLANG_DIR
        _vne_sc_glslang_src _vne_sc_glslang_origin)

    set(BUILD_EXTERNAL          OFF CACHE BOOL "" FORCE)
    set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
    set(ENABLE_GLSLANG_INSTALL  OFF CACHE BOOL "" FORCE)
    set(ENABLE_HLSL             ON  CACHE BOOL "" FORCE)
    set(ENABLE_RTTI             OFF CACHE BOOL "" FORCE)
    set(ENABLE_OPT              OFF CACHE BOOL "" FORCE)
    set(ENABLE_CTEST            OFF CACHE BOOL "" FORCE)
    set(ENABLE_SPVREMAPPER      OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTING           OFF CACHE BOOL "" FORCE)
    set(SKIP_GLSLANG_INSTALL    ON  CACHE BOOL "" FORCE)

    if(_vne_sc_glslang_src)
        add_subdirectory("${_vne_sc_glslang_src}"
                         "${CMAKE_BINARY_DIR}/deps/external/glslang" EXCLUDE_FROM_ALL)
    else()
        FetchContent_Declare(glslang
            GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
            GIT_TAG        ${_vne_sc_sdk_tag} GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(glslang)
        set(_vne_sc_glslang_origin "fetch:${_vne_sc_sdk_tag}")
    endif()

    foreach(_t glslang SPIRV glslang-default-resource-limits
            MachineIndependent OSDependent GenericCodeGen)
        if(TARGET ${_t})
            set_target_properties(${_t} PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
        endif()
    endforeach()

    message(STATUS "vnesc: glslang (${_vne_sc_glslang_origin})")
endif()

# ══════════════════════════════════════════════════════════════════════════════
# SPIRV-Tools  (when VNE_SC_SPIRVTOOLS=ON)
# ══════════════════════════════════════════════════════════════════════════════
if(VNE_SC_SPIRVTOOLS)
    set(_vne_sc_spirvtools_vendored "${_vne_sc_repo_root}/deps/external/SPIRV-Tools")
    if(EXISTS "${_vne_sc_spirvtools_vendored}/CMakeLists.txt")
        set(_vne_sc_spirvtools_dir_init "${_vne_sc_spirvtools_vendored}")
    else()
        set(_vne_sc_spirvtools_dir_init "")
    endif()
    set(VNE_SC_SPIRV_TOOLS_DIR "${_vne_sc_spirvtools_dir_init}" CACHE PATH
        "SPIRV-Tools source root. Empty: FetchContent. Default: deps/external/SPIRV-Tools if present." FORCE)
    _vne_sc_resolve_dep(SPIRV-Tools
        "${_vne_sc_spirvtools_vendored}" VNE_SC_SPIRV_TOOLS_DIR
        _vne_sc_spirvtools_src _vne_sc_spirvtools_origin)

    set(SPIRV_SKIP_EXECUTABLES  ON  CACHE BOOL "" FORCE)
    set(SPIRV_SKIP_TESTS        ON  CACHE BOOL "" FORCE)
    set(SPIRV_WERROR            OFF CACHE BOOL "" FORCE)
    set(SPIRV_BUILD_FUZZER      OFF CACHE BOOL "" FORCE)
    set(SKIP_SPIRV_TOOLS_INSTALL ON CACHE BOOL "" FORCE)

    if(_vne_sc_spirvtools_src)
        add_subdirectory("${_vne_sc_spirvtools_src}"
                         "${CMAKE_BINARY_DIR}/deps/external/spirv-tools" EXCLUDE_FROM_ALL)
    else()
        FetchContent_Declare(spirv_tools
            GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Tools.git
            GIT_TAG        ${_vne_sc_sdk_tag} GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(spirv_tools)
        set(_vne_sc_spirvtools_origin "fetch:${_vne_sc_sdk_tag}")
    endif()

    foreach(_t SPIRV-Tools-static SPIRV-Tools-opt)
        if(TARGET ${_t})
            set_target_properties(${_t} PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
        endif()
    endforeach()

    message(STATUS "vnesc: SPIRV-Tools (${_vne_sc_spirvtools_origin})")
endif()

# ══════════════════════════════════════════════════════════════════════════════
# nlohmann/json (offline manifests / bundle manifest.json)
# ══════════════════════════════════════════════════════════════════════════════
if(VNE_SC_JSON)
    set(_vne_sc_json_vendored "${_vne_sc_repo_root}/deps/external/nlohmann_json")
    if(EXISTS "${_vne_sc_json_vendored}/CMakeLists.txt")
        set(_vne_sc_json_dir_init "${_vne_sc_json_vendored}")
    else()
        set(_vne_sc_json_dir_init "")
    endif()
    set(VNE_SC_JSON_DIR "${_vne_sc_json_dir_init}" CACHE PATH
        "nlohmann/json source root. Empty: FetchContent. Default: deps/external/nlohmann_json if present." FORCE)

    _vne_sc_resolve_dep(nlohmann_json
        "${_vne_sc_json_vendored}" VNE_SC_JSON_DIR
        _vne_sc_json_src _vne_sc_json_origin)

    set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
    set(JSON_Install OFF CACHE BOOL "" FORCE)
    if(_vne_sc_json_src)
        add_subdirectory("${_vne_sc_json_src}"
                         "${CMAKE_BINARY_DIR}/deps/external/nlohmann_json" EXCLUDE_FROM_ALL)
    else()
        FetchContent_Declare(nlohmann_json
            GIT_REPOSITORY https://github.com/nlohmann/json.git
            GIT_TAG        v3.11.3
            GIT_SHALLOW    TRUE)
        FetchContent_MakeAvailable(nlohmann_json)
        set(_vne_sc_json_origin "fetch:v3.11.3")
    endif()
    message(STATUS "vnesc: nlohmann/json (${_vne_sc_json_origin})")
endif()

# ══════════════════════════════════════════════════════════════════════════════
# Dawn / Tint (SPIR-V → WGSL, when VNE_SC_TINT=ON)
# ══════════════════════════════════════════════════════════════════════════════
if(VNE_SC_TINT)
    set(DAWN_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
    set(DAWN_ENABLE_VULKAN OFF CACHE BOOL "" FORCE)
    set(DAWN_ENABLE_METAL OFF CACHE BOOL "" FORCE)
    set(DAWN_ENABLE_D3D12 OFF CACHE BOOL "" FORCE)
    set(DAWN_ENABLE_OPENGLES OFF CACHE BOOL "" FORCE)
    set(DAWN_USE_GLFW OFF CACHE BOOL "" FORCE)
    set(DAWN_USE_X11 OFF CACHE BOOL "" FORCE)
    set(DAWN_USE_WAYLAND OFF CACHE BOOL "" FORCE)
    set(TINT_BUILD_SPV_READER ON CACHE BOOL "" FORCE)
    set(TINT_BUILD_WGSL_WRITER ON CACHE BOOL "" FORCE)
    set(TINT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(TINT_BUILD_CMD_TOOLS OFF CACHE BOOL "" FORCE)
    set(TINT_BUILD_IR_BINARY OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(dawn
        GIT_REPOSITORY https://dawn.googlesource.com/dawn
        GIT_TAG        chromium/6723
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(dawn)
    message(STATUS "vnesc: Dawn/Tint (FetchContent chromium/6723)")
endif()

function(vne_sc_link_tint target)
    if(NOT VNE_SC_TINT)
        return()
    endif()
    if(TARGET libtint)
        target_link_libraries(${target} PRIVATE libtint)
    elseif(TARGET tint)
        target_link_libraries(${target} PRIVATE tint)
    else()
        message(FATAL_ERROR "vne_sc_link_tint: no libtint or tint target from Dawn")
    endif()
    target_compile_definitions(${target} PRIVATE VNE_SC_TINT_ENABLED)
endfunction()

# Restore PIC
if(DEFINED _vne_sc_prev_pic AND NOT _vne_sc_prev_pic STREQUAL "")
    set(CMAKE_POSITION_INDEPENDENT_CODE "${_vne_sc_prev_pic}")
else()
    unset(CMAKE_POSITION_INDEPENDENT_CODE)
endif()

# Helper function consumed by src/CMakeLists.txt
function(vne_sc_link_spirvcross target)
    if(NOT VNE_SC_SPIRV_CROSS_INCLUDE_DIR OR
       NOT EXISTS "${VNE_SC_SPIRV_CROSS_INCLUDE_DIR}/spirv_cross.hpp")
        message(FATAL_ERROR "vne_sc_link_spirvcross: invalid VNE_SC_SPIRV_CROSS_INCLUDE_DIR")
    endif()
    target_include_directories(${target} SYSTEM PRIVATE "${VNE_SC_SPIRV_CROSS_INCLUDE_DIR}")
    target_link_libraries(${target} PRIVATE
        $<LINK_ONLY:spirv-cross-core>
        $<LINK_ONLY:spirv-cross-glsl>
        $<LINK_ONLY:spirv-cross-msl>)
    target_compile_definitions(${target} PRIVATE VNE_SC_SPIRV_CROSS_AVAILABLE)
endfunction()
