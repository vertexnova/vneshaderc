#==============================================================================
# Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
# Licensed under the Apache License, Version 2.0 (the "License")
#
# Author:    Ajeet Singh Yadav
# Created:   February 2026
#
# Autodoc:   yes
#==============================================================================

# cmake/Dependencies.cmake
# External dependencies for vnesc via pure FetchContent.
#
# Each dep can be overridden with a local path:
#   -DVNE_SC_SPIRV_CROSS_DIR=/path/to/SPIRV-Cross
#   -DVNE_SC_GLSLANG_DIR=/path/to/glslang
#   -DVNE_SC_SPIRV_HEADERS_DIR=/path/to/SPIRV-Headers
#   -DVNE_SC_SPIRV_TOOLS_DIR=/path/to/SPIRV-Tools
#   -DVNE_SC_JSON_DIR=/path/to/nlohmann_json
#
# All KhronosGroup deps are pinned to vulkan-sdk-1.3.296.0.

include(FetchContent)

set(_vne_sc_sdk_tag "vulkan-sdk-1.3.296.0")

# Preserve PIC so static archives link into shared libs on Linux.
set(_vne_sc_prev_pic "${CMAKE_POSITION_INDEPENDENT_CODE}")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

#==============================================================================
#                         FetchContent Path Overrides                          #
#==============================================================================

# Map VNE_SC_<DEP>_DIR → FETCHCONTENT_SOURCE_DIR_<DEP>.
# FetchContent skips the network when FETCHCONTENT_SOURCE_DIR_<upper> is set.
macro(_vne_sc_apply_override _fc_name _cache_var)
    if(DEFINED ${_cache_var} AND NOT "${${_cache_var}}" STREQUAL "")
        if(EXISTS "${${_cache_var}}/CMakeLists.txt")
            string(TOUPPER "${_fc_name}" _fc_upper)
            string(REPLACE "-" "_" _fc_upper "${_fc_upper}")
            set("FETCHCONTENT_SOURCE_DIR_${_fc_upper}" "${${_cache_var}}" CACHE PATH "" FORCE)
            message(STATUS "[vnesc] ${_fc_name}: local override → ${${_cache_var}}")
        else()
            message(WARNING "[vnesc] ${_cache_var}='${${_cache_var}}' has no CMakeLists.txt; ignored.")
        endif()
    endif()
endmacro()

#==============================================================================
#                                 SPIRV-Cross                                  #
#==============================================================================

# Always required (cross-compilation + reflection).
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

_vne_sc_apply_override(SPIRV-Cross VNE_SC_SPIRV_CROSS_DIR)
FetchContent_Declare(SPIRV-Cross
    GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Cross.git
    GIT_TAG        ${_vne_sc_sdk_tag}
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(SPIRV-Cross)

FetchContent_GetProperties(SPIRV-Cross SOURCE_DIR _vne_sc_spirvcross_src)
set(VNE_SC_SPIRV_CROSS_INCLUDE_DIR "${_vne_sc_spirvcross_src}" CACHE INTERNAL
    "SPIRV-Cross header root (for bare-name includes like spirv_cross.hpp)")

foreach(_t spirv-cross-core spirv-cross-glsl spirv-cross-msl)
    if(TARGET ${_t})
        get_target_property(_inc ${_t} INTERFACE_INCLUDE_DIRECTORIES)
        if(_inc AND NOT _inc STREQUAL "_inc-NOTFOUND")
            set_target_properties(${_t} PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_inc}")
        endif()
        set_target_properties(${_t} PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
    endif()
endforeach()
message(STATUS "[vnesc] SPIRV-Cross → ${_vne_sc_spirvcross_src}")

#==============================================================================
#                          glslang + SPIRV-Headers                             #
#==============================================================================

if(VNE_SC_GLSLANG)
    # SPIRV-Headers must be available before glslang configures.
    set(SPIRV_HEADERS_SKIP_INSTALL ON CACHE BOOL "" FORCE)
    _vne_sc_apply_override(SPIRV-Headers VNE_SC_SPIRV_HEADERS_DIR)
    FetchContent_Declare(SPIRV-Headers
        GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Headers.git
        GIT_TAG        ${_vne_sc_sdk_tag}
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(SPIRV-Headers)
    FetchContent_GetProperties(SPIRV-Headers SOURCE_DIR _vne_sc_spirvhdr_src)
    # glslang locates SPIRV-Headers via this cache variable.
    set(SPIRV-Headers_SOURCE_DIR "${_vne_sc_spirvhdr_src}" CACHE PATH "" FORCE)
    message(STATUS "[vnesc] SPIRV-Headers → ${_vne_sc_spirvhdr_src}")

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

    _vne_sc_apply_override(glslang VNE_SC_GLSLANG_DIR)
    FetchContent_Declare(glslang
        GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
        GIT_TAG        ${_vne_sc_sdk_tag}
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(glslang)

    foreach(_t glslang SPIRV glslang-default-resource-limits
            MachineIndependent OSDependent GenericCodeGen)
        if(TARGET ${_t})
            set_target_properties(${_t} PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
        endif()
    endforeach()
    FetchContent_GetProperties(glslang SOURCE_DIR _vne_sc_glslang_src)
    message(STATUS "[vnesc] glslang → ${_vne_sc_glslang_src}")
endif()

#==============================================================================
#                                 SPIRV-Tools                                  #
#==============================================================================

if(VNE_SC_SPIRVTOOLS)
    set(SPIRV_SKIP_EXECUTABLES   ON  CACHE BOOL "" FORCE)
    set(SPIRV_SKIP_TESTS         ON  CACHE BOOL "" FORCE)
    set(SPIRV_WERROR             OFF CACHE BOOL "" FORCE)
    set(SPIRV_BUILD_FUZZER       OFF CACHE BOOL "" FORCE)
    set(SKIP_SPIRV_TOOLS_INSTALL ON  CACHE BOOL "" FORCE)

    _vne_sc_apply_override(SPIRV-Tools VNE_SC_SPIRV_TOOLS_DIR)
    FetchContent_Declare(SPIRV-Tools
        GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Tools.git
        GIT_TAG        ${_vne_sc_sdk_tag}
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(SPIRV-Tools)

    foreach(_t SPIRV-Tools-static SPIRV-Tools-opt)
        if(TARGET ${_t})
            set_target_properties(${_t} PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
        endif()
    endforeach()
    FetchContent_GetProperties(SPIRV-Tools SOURCE_DIR _vne_sc_spirvtools_src)
    message(STATUS "[vnesc] SPIRV-Tools → ${_vne_sc_spirvtools_src}")
endif()

#==============================================================================
#                               nlohmann/json                                  #
#==============================================================================

if(VNE_SC_JSON)
    set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
    set(JSON_Install    OFF CACHE BOOL "" FORCE)

    _vne_sc_apply_override(nlohmann_json VNE_SC_JSON_DIR)
    FetchContent_Declare(nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(nlohmann_json)
    FetchContent_GetProperties(nlohmann_json SOURCE_DIR _vne_sc_json_src)
    message(STATUS "[vnesc] nlohmann/json → ${_vne_sc_json_src}")
endif()

#==============================================================================
#                          Dawn / Tint Apple Workarounds                       #
#==============================================================================

# Fix abseil-cpp PR #1710 on Apple: CMAKE deduplicates -Xarch_* compile options,
# so x86-only flags like -msse4.1 leak into arm64 builds. Patch targets only
# (do not edit dawn-src; that dirties FetchContent's git checkout on reconfigure).
function(_vne_sc_fix_abseil_randen_target_copts _target)
    get_target_property(_copts ${_target} COMPILE_OPTIONS)
    if(NOT _copts OR _copts STREQUAL "_copts-NOTFOUND")
        set(_copts "")
    endif()
    list(FILTER _copts EXCLUDE REGEX "^-Xarch_")
    list(FILTER _copts EXCLUDE REGEX "^-m(aes|sse4\\.1|arch=)")
    list(FILTER _copts EXCLUDE REGEX "^-Wno-unused-command-line-argument$")
    list(APPEND _copts
        "SHELL:-Xarch_x86_64 -maes"
        "SHELL:-Xarch_x86_64 -msse4.1"
        "SHELL:-Xarch_arm64 -march=armv8-a+crypto"
        "-Wno-unused-command-line-argument")
    set_property(TARGET ${_target} PROPERTY COMPILE_OPTIONS "${_copts}")
endfunction()

function(_vne_sc_fix_dawn_abseil_randen_copts)
    if(NOT APPLE)
        return()
    endif()
    set(_fixed 0)
    foreach(_target IN ITEMS
            absl_random_internal_randen_hwaes
            absl_random_internal_randen_hwaes_impl)
        if(TARGET ${_target})
            _vne_sc_fix_abseil_randen_target_copts(${_target})
            math(EXPR _fixed "${_fixed} + 1")
        endif()
    endforeach()
    if(_fixed GREATER 0)
        message(STATUS "[vnesc] Fixed ${_fixed} Dawn abseil randen HWAES target(s) for Apple ARM64")
    endif()
endfunction()

#==============================================================================
#                              Dawn / Tint (optional)                          #
#==============================================================================

# FetchContent only — no submodule. When VNE_SC_TINT=ON.
if(VNE_SC_TINT)
    set(DAWN_BUILD_SAMPLES           OFF CACHE BOOL "" FORCE)
    # Dawn can either bootstrap dependencies via Chromium's depot_tools/gclient
    # flow or via its lightweight `tools/fetch_dawn_dependencies.py` script.
    # In headless environments this often avoids interactive credential prompts.
    set(DAWN_FETCH_DEPENDENCIES     ON CACHE BOOL "" FORCE)
    set(DAWN_ENABLE_VULKAN           OFF CACHE BOOL "" FORCE)
    set(DAWN_ENABLE_METAL            OFF CACHE BOOL "" FORCE)
    set(DAWN_ENABLE_D3D11            OFF CACHE BOOL "" FORCE)
    set(DAWN_ENABLE_D3D12            OFF CACHE BOOL "" FORCE)
    set(DAWN_ENABLE_OPENGLES         OFF CACHE BOOL "" FORCE)
    set(DAWN_ENABLE_DESKTOP_GL       OFF CACHE BOOL "" FORCE)
    set(DAWN_ENABLE_NULL             OFF CACHE BOOL "" FORCE)
    set(DAWN_ENABLE_SPIRV_VALIDATION OFF CACHE BOOL "" FORCE)
    set(DAWN_USE_GLFW                OFF CACHE BOOL "" FORCE)
    set(DAWN_USE_X11                 OFF CACHE BOOL "" FORCE)
    set(DAWN_USE_WAYLAND             OFF CACHE BOOL "" FORCE)
    set(DAWN_BUILD_MONOLITHIC_LIBRARY OFF CACHE BOOL "" FORCE)
    set(TINT_BUILD_SPV_READER        ON  CACHE BOOL "" FORCE)
    set(TINT_BUILD_WGSL_WRITER       ON  CACHE BOOL "" FORCE)
    set(TINT_BUILD_WGSL_READER       OFF CACHE BOOL "" FORCE)
    set(TINT_BUILD_GLSL_WRITER       OFF CACHE BOOL "" FORCE)
    set(TINT_BUILD_GLSL_VALIDATOR    OFF CACHE BOOL "" FORCE)
    set(TINT_BUILD_HLSL_WRITER       OFF CACHE BOOL "" FORCE)
    set(TINT_BUILD_MSL_WRITER        OFF CACHE BOOL "" FORCE)
    set(TINT_BUILD_SPV_WRITER        OFF CACHE BOOL "" FORCE)
    set(TINT_BUILD_TESTS             OFF CACHE BOOL "" FORCE)
    set(TINT_BUILD_CMD_TOOLS         OFF CACHE BOOL "" FORCE)
    set(TINT_BUILD_IR_BINARY         OFF CACHE BOOL "" FORCE)
    set(TINT_BUILD_BENCHMARKS        OFF CACHE BOOL "" FORCE)
    set(TINT_BUILD_FUZZERS           OFF CACHE BOOL "" FORCE)
    set(TINT_BUILD_IR_FUZZER         OFF CACHE BOOL "" FORCE)

    # Build Dawn/Tint in Release even for Debug projects — it's a large compiler
    # library that has negligible benefit from debug symbols.
    set(_vne_sc_saved_build_type "${CMAKE_BUILD_TYPE}")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
    FetchContent_Declare(dawn
        GIT_REPOSITORY       https://dawn.googlesource.com/dawn
        GIT_TAG              chromium/6723
        GIT_SHALLOW          TRUE
        GIT_SUBMODULES       ""
        GIT_SUBMODULES_RECURSE FALSE)
    FetchContent_MakeAvailable(dawn)
    _vne_sc_fix_dawn_abseil_randen_copts()
    set(CMAKE_BUILD_TYPE "${_vne_sc_saved_build_type}" CACHE STRING "" FORCE)
    message(STATUS "[vnesc] Dawn/Tint → FetchContent chromium/6723 (Release build)")
endif()

#==============================================================================
#                         Link Helpers (for src/CMakeLists)                    #
#==============================================================================

function(vne_sc_link_spirvcross target)
    if(NOT VNE_SC_SPIRV_CROSS_INCLUDE_DIR OR
       NOT EXISTS "${VNE_SC_SPIRV_CROSS_INCLUDE_DIR}/spirv_cross.hpp")
        message(FATAL_ERROR
            "vne_sc_link_spirvcross: invalid VNE_SC_SPIRV_CROSS_INCLUDE_DIR"
            " ('${VNE_SC_SPIRV_CROSS_INCLUDE_DIR}')")
    endif()
    target_include_directories(${target} SYSTEM PRIVATE "${VNE_SC_SPIRV_CROSS_INCLUDE_DIR}")
    target_link_libraries(${target} PRIVATE
        $<LINK_ONLY:spirv-cross-core>
        $<LINK_ONLY:spirv-cross-glsl>
        $<LINK_ONLY:spirv-cross-msl>)
    target_compile_definitions(${target} PRIVATE VNE_SC_SPIRV_CROSS_AVAILABLE)
endfunction()

function(vne_sc_link_tint target)
    if(NOT VNE_SC_TINT)
        return()
    endif()
    if(TARGET libtint)
        target_link_libraries(${target} PRIVATE libtint)
    elseif(TARGET tint)
        target_link_libraries(${target} PRIVATE tint)
    else()
        message(FATAL_ERROR "vne_sc_link_tint: no libtint or tint target found from Dawn")
    endif()
    target_compile_definitions(${target} PRIVATE VNE_SC_TINT_ENABLED)
endfunction()

#==============================================================================
#                               Restore PIC State                              #
#==============================================================================

if(DEFINED _vne_sc_prev_pic AND NOT _vne_sc_prev_pic STREQUAL "")
    set(CMAKE_POSITION_INDEPENDENT_CODE "${_vne_sc_prev_pic}")
else()
    unset(CMAKE_POSITION_INDEPENDENT_CODE)
endif()
