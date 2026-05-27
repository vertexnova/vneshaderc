#==============================================================================
# Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
# Licensed under the Apache License, Version 2.0 (the "License")
#
# vnesc test suite — one translation unit per class under test (ClassNameTest).
#==============================================================================

if(NOT TARGET vnesc)
    return()
endif()

# ── Google Test ───────────────────────────────────────────────────────────────
# Always build from source via FetchContent to guarantee static linkage.
message(STATUS "vnesc tests: fetching googletest v1.17.0")
include(FetchContent)
if(DEFINED VNE_SC_GOOGLETEST_DIR)
    set(FETCHCONTENT_SOURCE_DIR_GOOGLETEST "${VNE_SC_GOOGLETEST_DIR}" CACHE PATH "" FORCE)
endif()
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.17.0
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(googletest)
set(GTEST_LIBS gtest gmock)

set(VNESC_TEST_SOURCES
    main.cpp
    result_code_test.cpp
    shader_artifact_test.cpp
    shader_artifact_cache_test.cpp
    shader_compiler_factory_test.cpp
    shader_pipeline_spec_test.cpp
)

if(VNE_SC_GLSLANG)
    list(APPEND VNESC_TEST_SOURCES
        glslang_frontend_test.cpp
        spirvcross_reflector_test.cpp
        shader_cross_compiler_test.cpp
        shader_pipeline_builder_test.cpp
        shader_bundle_test.cpp
        shader_pipeline_spec_includes_test.cpp)
endif()

if(VNE_SC_TINT AND VNE_SC_GLSLANG)
    list(APPEND VNESC_TEST_SOURCES tint_cross_compiler_test.cpp)
endif()

add_executable(vnesc_tests ${VNESC_TEST_SOURCES})
target_include_directories(vnesc_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(vnesc_tests PRIVATE
    ${GTEST_LIBS}
    vnesc)

if(VNE_SC_GLSLANG)
    target_link_libraries(vnesc_tests PRIVATE vnesc_glslang)
    target_compile_definitions(vnesc_tests PRIVATE VNE_SC_GLSLANG_ENABLED)
endif()
if(VNE_SC_TINT)
    target_compile_definitions(vnesc_tests PRIVATE VNE_SC_TINT_ENABLED)
endif()
if(VNE_SC_JSON)
    target_compile_definitions(vnesc_tests PRIVATE VNE_SC_JSON_ENABLED)
endif()

include(GoogleTest)
gtest_discover_tests(vnesc_tests)
