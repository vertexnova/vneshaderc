#==============================================================================
# Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
# Licensed under the Apache License, Version 2.0 (the "License")
#
# Author:    Ajeet Singh Yadav
# Created:   February 2026
#
# Autodoc:   yes
#==============================================================================

# 01_hello_compiler — inline GLSL → MSL via PipelineBuildDesc

#==============================================================================
#                              Build Executable                                #
#==============================================================================

add_executable(vnesc_example_01_hello_compiler main.cpp)

target_include_directories(vnesc_example_01_hello_compiler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/examples/common)

#==============================================================================
#                                Dependencies                                  #
#==============================================================================

target_link_libraries(vnesc_example_01_hello_compiler PRIVATE vnesc vnesc_glslang)

set_target_properties(vnesc_example_01_hello_compiler PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/examples/glsl/01_hello_compiler")
