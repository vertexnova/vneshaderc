#==============================================================================
# Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
# Licensed under the Apache License, Version 2.0 (the "License")
#
# Author:    Ajeet Singh Yadav
# Created:   February 2026
#
# Autodoc:   yes
#==============================================================================

# 03_textured_pipeline_spec - JSON pipeline spec -> MSL bundle

if(NOT VNE_SC_JSON)
    message(STATUS "03_textured_pipeline_spec requires VNE_SC_JSON=ON - skipping")
    return()
endif()

#==============================================================================
#                              Build Executable                                #
#==============================================================================

add_executable(vnesc_example_03_textured_pipeline_spec main.cpp)

target_include_directories(vnesc_example_03_textured_pipeline_spec PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/examples/common)

#==============================================================================
#                                Dependencies                                  #
#==============================================================================

target_link_libraries(vnesc_example_03_textured_pipeline_spec PRIVATE vnesc vnesc_glslang)

set_target_properties(vnesc_example_03_textured_pipeline_spec PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/examples/glsl/03_textured_pipeline_spec")
