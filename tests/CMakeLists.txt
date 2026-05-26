#==============================================================================
# Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
# Licensed under the Apache License, Version 2.0 (the "License")
#
# vnesc test suite
#==============================================================================

if(NOT TARGET vnesc)
    return()
endif()

# ── Google Test ───────────────────────────────────────────────────────────────
if(EXISTS "${PROJECT_SOURCE_DIR}/deps/external/googletest/CMakeLists.txt")
    message(STATUS "vnesc tests: using vendored googletest")
    add_subdirectory(
        ${PROJECT_SOURCE_DIR}/deps/external/googletest
        ${CMAKE_BINARY_DIR}/deps/external/googletest
        EXCLUDE_FROM_ALL)
    set(GTEST_LIBS gtest_main gmock)
else()
    find_package(GTest QUIET)
    if(GTest_FOUND)
        message(STATUS "vnesc tests: using system googletest")
        set(GTEST_LIBS GTest::gtest_main GTest::gmock)
    else()
        message(STATUS "vnesc tests: fetching googletest v1.17.0")
        include(FetchContent)
        FetchContent_Declare(googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG        v1.17.0
            GIT_SHALLOW    TRUE)
        FetchContent_MakeAvailable(googletest)
        set(GTEST_LIBS gtest_main gmock)
    endif()
endif()

# ── Test executable ───────────────────────────────────────────────────────────
add_executable(vnesc_tests vnesc_tests.cpp)
target_include_directories(vnesc_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/src)   # internal headers (e.g. GlslangFrontEnd)
target_link_libraries(vnesc_tests PRIVATE
    ${GTEST_LIBS}
    vnesc)
if(VNE_SC_GLSLANG)
    target_link_libraries(vnesc_tests PRIVATE vnesc_glslang)
    target_compile_definitions(vnesc_tests PRIVATE VNE_SC_GLSLANG_ENABLED)
endif()

include(GoogleTest)
gtest_discover_tests(vnesc_tests)
