#==============================================================================
# Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
# Licensed under the Apache License, Version 2.0 (the "License")
#
# Author:    Ajeet Singh Yadav
# Created:   February 2026
#
# Autodoc:   yes
#==============================================================================

# 02_triangle_bundle — file-based GLSL → MSL bundle + reflection dump

#==============================================================================
#                              Build Executable                                #
#==============================================================================

add_executable(vnesc_example_02_triangle_bundle main.cpp)

target_include_directories(vnesc_example_02_triangle_bundle PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/examples/common)

#==============================================================================
#                                Dependencies                                  #
#==============================================================================

target_link_libraries(vnesc_example_02_triangle_bundle PRIVATE vnesc vnesc_glslang)

set_target_properties(vnesc_example_02_triangle_bundle PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/examples/glsl/02_triangle_bundle")
