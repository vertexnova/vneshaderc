#==============================================================================
# Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
# Licensed under the Apache License, Version 2.0 (the "License")
#
# cmake/CompilerOptions.cmake
# Platform detection, compile definitions, and optional sanitizer/coverage flags.
# Included from the top-level CMakeLists.txt after project() and ProjectSetup.
#==============================================================================

# Platform Detection
if(DEFINED VNE_TARGET_PLATFORM)
    message(STATUS "[vnesc] Using manually specified target platform: ${VNE_TARGET_PLATFORM}")
else()
    if(EMSCRIPTEN)
        set(VNE_TARGET_PLATFORM "Web")
    elseif(WIN32)
        set(VNE_TARGET_PLATFORM "Windows")
    elseif(APPLE)
        if(VISIONOS OR "${CMAKE_SYSTEM_NAME}" STREQUAL "visionOS")
            set(VNE_TARGET_PLATFORM "visionOS")
        elseif(IOS OR "${CMAKE_SYSTEM_NAME}" STREQUAL "iOS")
            set(VNE_TARGET_PLATFORM "iOS")
        else()
            set(VNE_TARGET_PLATFORM "macOS")
        endif()
    elseif(UNIX)
        if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
            set(VNE_TARGET_PLATFORM "Linux")
        elseif("${CMAKE_SYSTEM_NAME}" STREQUAL "Android")
            set(VNE_TARGET_PLATFORM "Android")
        else()
            message(FATAL_ERROR "Unsupported UNIX platform: ${CMAKE_SYSTEM_NAME}")
        endif()
    else()
        message(FATAL_ERROR "Unsupported platform")
    endif()
endif()
message(STATUS "[vnesc] Platform: ${VNE_TARGET_PLATFORM}")

# Global Compile Definitions
if(VNE_TARGET_PLATFORM     STREQUAL "macOS")
    add_compile_definitions(VNE_PLATFORM_MACOS)
elseif(VNE_TARGET_PLATFORM STREQUAL "iOS")
    add_compile_definitions(VNE_PLATFORM_IOS)
elseif(VNE_TARGET_PLATFORM STREQUAL "visionOS")
    add_compile_definitions(VNE_PLATFORM_VISIONOS)
elseif(VNE_TARGET_PLATFORM STREQUAL "Windows")
    add_compile_definitions(VNE_PLATFORM_WIN)
elseif(VNE_TARGET_PLATFORM STREQUAL "Linux")
    add_compile_definitions(VNE_PLATFORM_LINUX)
elseif(VNE_TARGET_PLATFORM STREQUAL "Android")
    add_compile_definitions(VNE_PLATFORM_ANDROID)
elseif(VNE_TARGET_PLATFORM STREQUAL "Web")
    add_compile_definitions(VNE_PLATFORM_WEB)
endif()

# Code Coverage (gcov/lcov)
if(ENABLE_COVERAGE)
    include(FindCoverage OPTIONAL)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(--coverage)
        add_link_options(--coverage)
    endif()
endif()

# AddressSanitizer + UndefinedBehaviorSanitizer
if(ENABLE_ASAN)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT WIN32)
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    else()
        message(WARNING "ENABLE_ASAN is only supported with GCC/Clang on Linux/macOS — ignoring.")
    endif()
endif()
