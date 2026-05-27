#==============================================================================
# Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
# Licensed under the Apache License, Version 2.0 (the "License")
#
# vnesc library targets
#
#   vnesc_interface   — public headers; no compiled code
#   vnesc_glslang     — glslang GLSL → SPIR-V front-end (conditional)
#   vnesc_spirvcross  — SPIRV-Cross cross-compiler + reflector (always built)
#   vnesc_validator   — SPIR-V validator (SPIRV-Tools or no-op)
#   vnesc             — core: cache, factory, pipeline builder
#==============================================================================

# ── Interface library (public headers only) ───────────────────────────────────
add_library(vnesc_interface INTERFACE)
target_include_directories(vnesc_interface INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(vnesc_interface INTERFACE
    vne::sc::Warnings
    vne::sc::BuildSettings
    vne::logging)
target_compile_features(vnesc_interface INTERFACE cxx_std_20)
add_library(vne::sc::interface ALIAS vnesc_interface)

# ── glslang front-end (GLSL → SPIR-V) ────────────────────────────────────────
if(VNE_SC_GLSLANG)
    add_library(vnesc_glslang STATIC
        vertexnova/sc/frontend/glslang_frontend.cpp)
    target_include_directories(vnesc_glslang PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(vnesc_glslang PUBLIC
        vnesc_interface
        glslang
        SPIRV
        glslang-default-resource-limits)
    target_compile_definitions(vnesc_glslang PUBLIC VNE_SC_GLSLANG_ENABLED)
    set_target_properties(vnesc_glslang PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
    add_library(vne::sc::glslang ALIAS vnesc_glslang)
endif()

# ── SPIRV-Cross cross-compiler + reflector (SPIRV-Cross only; not SPIRV-Headers) ─
if(NOT TARGET spirv-cross-core)
    message(FATAL_ERROR
        "vnesc_spirvcross requires SPIRV-Cross (target spirv-cross-core). "
        "Set VNE_SC_SPIRV_CROSS_DIR or allow FetchContent in cmake/Dependencies.cmake.")
endif()
add_library(vnesc_spirvcross STATIC
    vertexnova/sc/crosscompiler/spirvcross_cross_compiler.cpp
    vertexnova/sc/reflector/spirvcross_reflector.cpp)
target_include_directories(vnesc_spirvcross PRIVATE ${PROJECT_SOURCE_DIR}/src)
vne_sc_link_spirvcross(vnesc_spirvcross)
target_link_libraries(vnesc_spirvcross PUBLIC vnesc_interface)
set_target_properties(vnesc_spirvcross PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
add_library(vne::sc::spirvcross ALIAS vnesc_spirvcross)

# ── Validator (SPIRV-Tools when available, no-op otherwise) ───────────────────
if(VNE_SC_SPIRVTOOLS)
    add_library(vnesc_validator STATIC
        vertexnova/sc/validator/spirvtools_validator.cpp)
    target_link_libraries(vnesc_validator PUBLIC vnesc_interface SPIRV-Tools-static)
    target_compile_definitions(vnesc_validator PUBLIC VNE_SC_SPIRVTOOLS_ENABLED)
else()
    add_library(vnesc_validator STATIC
        vertexnova/sc/validator/noop_validator.cpp)
    target_link_libraries(vnesc_validator PUBLIC vnesc_interface)
endif()
target_include_directories(vnesc_validator PRIVATE ${PROJECT_SOURCE_DIR}/src)
set_target_properties(vnesc_validator PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
add_library(vne::sc::validator ALIAS vnesc_validator)

# ── Tint WGSL cross-compiler (optional) ───────────────────────────────────────
if(VNE_SC_TINT)
    add_library(vnesc_tint STATIC
        vertexnova/sc/crosscompiler/tint_cross_compiler.cpp)
    target_include_directories(vnesc_tint PRIVATE ${PROJECT_SOURCE_DIR}/src)
    vne_sc_link_tint(vnesc_tint)
    target_link_libraries(vnesc_tint PUBLIC vnesc_interface)
    set_target_properties(vnesc_tint PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
    add_library(vne::sc::tint ALIAS vnesc_tint)
endif()

# ── Core library (cache, factory, pipeline builder, bundle) ─────────────────
add_library(vnesc STATIC
    vertexnova/sc/shader_artifact_cache.cpp
    vertexnova/sc/shader_reflection_binary.cpp
    vertexnova/sc/shader_bundle.cpp
    vertexnova/sc/shader_pipeline_spec.cpp
    vertexnova/sc/shader_compiler_factory.cpp
    vertexnova/sc/shader_pipeline_builder.cpp
    vertexnova/sc/crosscompiler/shader_cross_compiler.cpp)
target_include_directories(vnesc PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(vnesc PUBLIC
    vnesc_interface
    vnesc_spirvcross
    vnesc_validator)
if(VNE_SC_GLSLANG)
    target_link_libraries(vnesc PRIVATE vnesc_glslang)
    target_compile_definitions(vnesc PUBLIC VNE_SC_GLSLANG_ENABLED)
endif()
if(VNE_SC_TINT)
    target_link_libraries(vnesc PRIVATE vnesc_tint)
    target_compile_definitions(vnesc PUBLIC VNE_SC_TINT_ENABLED)
endif()
if(VNE_SC_JSON)
    target_link_libraries(vnesc PUBLIC nlohmann_json::nlohmann_json)
    target_compile_definitions(vnesc PUBLIC VNE_SC_JSON_ENABLED)
endif()
set_target_properties(vnesc PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
add_library(vne::sc ALIAS vnesc)
