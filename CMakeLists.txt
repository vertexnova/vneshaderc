#==============================================================================
# Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
# Licensed under the Apache License, Version 2.0 (the "License")
#
# Author:    Ajeet Singh Yadav
# Created:   February 2026
#
# Autodoc:   yes
#==============================================================================

cmake_minimum_required(VERSION 3.19)

# Single source of truth for version (updated by release-please on release)
set(VERSION_FILE "${CMAKE_CURRENT_LIST_DIR}/VERSION")
if(EXISTS "${VERSION_FILE}")
    file(READ "${VERSION_FILE}" VERSION_FROM_FILE)
    string(STRIP "${VERSION_FROM_FILE}" VERSION_FROM_FILE)
    if(VERSION_FROM_FILE)
        set(PROJECT_VERSION_INIT "${VERSION_FROM_FILE}")
    endif()
endif()
if(NOT DEFINED PROJECT_VERSION_INIT)
    set(PROJECT_VERSION_INIT "1.0.0")
endif()

project(vnesc VERSION ${PROJECT_VERSION_INIT} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# CMake modules from vnecmake submodule (cmake/vnecmake)
set(VNE_DEPS_INTERNAL_DIR ${PROJECT_SOURCE_DIR}/deps/internal)
set(VNE_DEPS_EXTERNAL_DIR ${PROJECT_SOURCE_DIR}/deps/external)
set(VNECMAKE_MODULES "${PROJECT_SOURCE_DIR}/cmake/vnecmake/modules")
if(NOT EXISTS "${VNECMAKE_MODULES}/VneUseDep.cmake")
    message(FATAL_ERROR "vnecmake not found at cmake/vnecmake. Run: git submodule update --init --recursive")
endif()
list(APPEND CMAKE_MODULE_PATH "${VNECMAKE_MODULES}" "${PROJECT_SOURCE_DIR}/cmake")

include(ProjectSetup)
include(CompilerOptions)

#==============================================================================
# Internal Libraries (deps/internal) - use if already in build, else add
#==============================================================================
include(VneUseDep)

function(_vnesc_configure_vnecommon_dep)
    get_property(_vt_build_examples_ty CACHE BUILD_EXAMPLES PROPERTY TYPE)
    if(NOT _vt_build_examples_ty STREQUAL "NOTFOUND" AND _vt_build_examples_ty)
        set(_vt_build_examples_saved "${BUILD_EXAMPLES}")
    endif()
    vne_use_dep(TARGET vne::common SUBDIR "${VNE_DEPS_INTERNAL_DIR}/vnecommon" BINARY_DIR "${CMAKE_BINARY_DIR}/deps/internal/vnecommon"
        CACHE_VARS BUILD_EXAMPLES OFF)
    if(DEFINED _vt_build_examples_saved)
        set(BUILD_EXAMPLES "${_vt_build_examples_saved}" CACHE BOOL "" FORCE)
    else()
        unset(BUILD_EXAMPLES CACHE)
    endif()
endfunction()
_vnesc_configure_vnecommon_dep()

function(_vnesc_configure_vnelogging_dep)
    get_property(_vt_build_tests_ty CACHE BUILD_TESTS PROPERTY TYPE)
    if(NOT _vt_build_tests_ty STREQUAL "NOTFOUND" AND _vt_build_tests_ty)
        set(_vt_build_tests_saved "${BUILD_TESTS}")
    endif()
    get_property(_vt_build_examples_ty CACHE BUILD_EXAMPLES PROPERTY TYPE)
    if(NOT _vt_build_examples_ty STREQUAL "NOTFOUND" AND _vt_build_examples_ty)
        set(_vt_build_examples_saved "${BUILD_EXAMPLES}")
    endif()
    set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    vne_use_dep(TARGET vne::logging SUBDIR "${VNE_DEPS_INTERNAL_DIR}/vnelogging" BINARY_DIR "${CMAKE_BINARY_DIR}/deps/internal/vnelogging"
        CACHE_VARS BUILD_TESTS OFF BUILD_EXAMPLES OFF)
    if(DEFINED _vt_build_tests_saved)
        set(BUILD_TESTS "${_vt_build_tests_saved}" CACHE BOOL "" FORCE)
    else()
        unset(BUILD_TESTS CACHE)
    endif()
    if(DEFINED _vt_build_examples_saved)
        set(BUILD_EXAMPLES "${_vt_build_examples_saved}" CACHE BOOL "" FORCE)
    else()
        unset(BUILD_EXAMPLES CACHE)
    endif()
endfunction()
_vnesc_configure_vnelogging_dep()

# Include dirs and config
set(VNE_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)
set(VNE_SRC_DIR ${PROJECT_SOURCE_DIR}/src)
configure_file(${PROJECT_SOURCE_DIR}/configs/config.h.in ${CMAKE_BINARY_DIR}/config.h)
include_directories(${CMAKE_BINARY_DIR})

#==============================================================================
# Build Options
#==============================================================================
# Option naming and deps:
# - Use VNE_TEMPLATE_* for vnetemplate-only options so there is no conflict when
#   this project is used as a submodule (parent can set VNE_TEMPLATE_TESTS=OFF
#   without affecting BUILD_TESTS used by other deps).
# - BUILD_TESTS / BUILD_EXAMPLES are global; we force them (via VneUseDep
#   CACHE_VARS) before add_subdirectory for each dep so deps do not build their
#   own tests/examples when used as internal libs. We restore after each dep.
# - ENABLE_COVERAGE, ENABLE_DOXYGEN, WARNINGS_AS_ERRORS are global: if multiple
#   libs in deps/internal define the same option name, the first to run wins the
#   default; -D from the command line applies to all. No duplicate option()
#   definition (CMake uses cache), but behavior is "one value for the whole tree."
#==============================================================================
if(CMAKE_PROJECT_NAME STREQUAL "vnesc")
    set(VNE_SC_DEV_DEFAULT ON)
else()
    set(VNE_SC_DEV_DEFAULT OFF)
endif()
option(VNE_SC_DEV "Dev build: enable tests (local development)" ${VNE_SC_DEV_DEFAULT})
option(VNE_SC_CI "CI build: enable tests (e.g. -DVNE_SC_CI=ON in pipelines)" OFF)

if(VNE_SC_CI)
    set(VNE_SC_DEV OFF CACHE BOOL "" FORCE)
endif()

option(VNE_SC_TESTS    "Build vnesc test suite (turn OFF when used as submodule)" ON)
option(VNE_SC_EXAMPLES "Build vnesc example programs (turn OFF when used as submodule)" OFF)
option(VNE_SC_TOOLS    "Build vnesc CLI tools (e.g. vnesc_shader_compiler)" ON)

# ── Shader compiler backend options ──────────────────────────────────────────
option(VNE_SC_GLSLANG    "Enable glslang GLSL→SPIR-V front-end (Phase 1, primary)" ON)
option(VNE_SC_DXC        "Enable DXC HLSL→SPIR-V front-end (Phase 2 — stub only)" OFF)
option(VNE_SC_SLANG      "Enable Slang front-end (Phase 3 — stub only)" OFF)
option(VNE_SC_SPIRVTOOLS "Enable SPIRV-Tools SPIR-V validator (OFF = no-op validator)" OFF)
option(VNE_SC_TINT       "Enable Dawn Tint SPIR-V→WGSL cross-compiler (slow first configure)" OFF)
option(VNE_SC_JSON       "Enable nlohmann/json for manifests and bundle metadata" ON)

option(ENABLE_DOXYGEN "Enable Doxygen documentation builds" OFF)
option(ENABLE_COVERAGE "Enable code coverage reporting" OFF)
option(ENABLE_ASAN "Enable AddressSanitizer + UndefinedBehaviorSanitizer (Clang/GCC, Linux/macOS only)" OFF)
option(WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

#==============================================================================
# Incompatible options: coverage and ASan/UBSan must not both be enabled
# (--coverage + -fsanitize=... together produce noisy/incorrect results).
#==============================================================================
if(ENABLE_COVERAGE AND ENABLE_ASAN)
    message(FATAL_ERROR
        "ENABLE_COVERAGE and ENABLE_ASAN cannot both be ON. "
        "Choose one: coverage (gcov/lcov) or ASan/UBSan. "
        "Reconfigure with -DENABLE_COVERAGE=OFF or -DENABLE_ASAN=OFF.")
endif()

if(VNE_SC_CI)
    if(NOT DEFINED VNE_SC_TESTS OR VNE_SC_TESTS)
        set(VNE_SC_TESTS ON CACHE BOOL "" FORCE)
    endif()
    set(VNE_SC_EXAMPLES OFF CACHE BOOL "" FORCE)
    message(STATUS "vnesc: CI build -> VNE_SC_TESTS=${VNE_SC_TESTS}, VNE_SC_EXAMPLES=OFF")
elseif(VNE_SC_DEV)
    set(VNE_SC_TESTS ON CACHE BOOL "" FORCE)
    set(VNE_SC_EXAMPLES ON CACHE BOOL "" FORCE)
    message(STATUS "vnesc: DEV build -> VNE_SC_TESTS=ON, VNE_SC_EXAMPLES=ON")
endif()

#==============================================================================
# Compiler Warnings
#==============================================================================
add_library(VnescWarnings INTERFACE)
include(ProjectWarnings)
set_project_warnings(VnescWarnings)
add_library(vne::sc::Warnings ALIAS VnescWarnings)

#==============================================================================
# Build Settings Interface Library
#==============================================================================
add_library(VnescBuildSettings INTERFACE)
target_compile_features(VnescBuildSettings INTERFACE cxx_std_20)
add_library(vne::sc::BuildSettings ALIAS VnescBuildSettings)

if(CMAKE_BUILD_TYPE MATCHES Debug)
    target_compile_definitions(VnescBuildSettings INTERFACE VNE_BUILD_TYPE_DEBUG VNE_DEVELOPER_BUILD)
elseif(CMAKE_BUILD_TYPE MATCHES Release)
    target_compile_definitions(VnescBuildSettings INTERFACE VNE_BUILD_TYPE_RELEASE)
elseif(CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)
    target_compile_definitions(VnescBuildSettings INTERFACE VNE_BUILD_TYPE_RELWITHDEBINFO)
elseif(CMAKE_BUILD_TYPE MATCHES MinSizeRel)
    target_compile_definitions(VnescBuildSettings INTERFACE VNE_BUILD_TYPE_MINSIZEREL)
endif()

if(VNE_TARGET_PLATFORM STREQUAL "Windows")
    target_compile_definitions(VnescBuildSettings INTERFACE VNE_PLATFORM_WIN)
elseif(VNE_TARGET_PLATFORM STREQUAL "Linux")
    target_compile_definitions(VnescBuildSettings INTERFACE VNE_PLATFORM_LINUX)
elseif(VNE_TARGET_PLATFORM STREQUAL "macOS")
    target_compile_definitions(VnescBuildSettings INTERFACE VNE_PLATFORM_MACOS)
elseif(VNE_TARGET_PLATFORM STREQUAL "iOS")
    target_compile_definitions(VnescBuildSettings INTERFACE VNE_PLATFORM_IOS)
elseif(VNE_TARGET_PLATFORM STREQUAL "visionOS")
    target_compile_definitions(VnescBuildSettings INTERFACE VNE_PLATFORM_VISIONOS)
elseif(VNE_TARGET_PLATFORM STREQUAL "Android")
    target_compile_definitions(VnescBuildSettings INTERFACE VNE_PLATFORM_ANDROID)
elseif(VNE_TARGET_PLATFORM STREQUAL "Web")
    target_compile_definitions(VnescBuildSettings INTERFACE VNE_PLATFORM_WEB)
endif()

# External shader compiler dependencies (FetchContent)
include(Dependencies)

#==============================================================================
# Add Library
#==============================================================================
add_subdirectory(src)

#==============================================================================
# Tests
#==============================================================================
if(VNE_SC_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

#==============================================================================
# Examples
#==============================================================================
if(VNE_SC_EXAMPLES)
    add_subdirectory(examples)
endif()

#==============================================================================
# Tools
#==============================================================================
if(VNE_SC_TOOLS)
    add_subdirectory(tools/shader_compiler_cli)
endif()

#==============================================================================
# Documentation
#==============================================================================
if(ENABLE_DOXYGEN)
    find_package(Doxygen QUIET OPTIONAL_COMPONENTS dot)
    if(Doxygen_FOUND)
        set(DOXYGEN_IN ${PROJECT_SOURCE_DIR}/docs/doxyfile.in)
        set(DOXYGEN_OUT ${CMAKE_BINARY_DIR}/doxyfile)
        set(DOXYGEN_HAVE_DOT ${DOXYGEN_DOT_FOUND})
        configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
        add_custom_target(vnesc_doc_doxygen ALL
            COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM
        )
        message(STATUS "Doxygen target: vnesc_doc_doxygen")
    else()
        message(STATUS "Doxygen not found. Install doxygen to build documentation.")
    endif()
endif()

#==============================================================================
# CPack (ZIP/TGZ release archives)
#==============================================================================
set(CPACK_PACKAGE_NAME         "vnesc")
set(CPACK_PACKAGE_VERSION      "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR       "VertexNova")
set(CPACK_GENERATOR            "ZIP;TGZ")
set(CPACK_COMPONENTS_ALL       vnesc vnesc_tools)
set(CPACK_COMPONENT_VNESC_DISPLAY_NAME       "vnesc library + headers")
set(CPACK_COMPONENT_VNESC_TOOLS_DISPLAY_NAME "vnesc CLI tools")
include(CPack)

#==============================================================================
# Installation
#==============================================================================
include(InstallRules)
