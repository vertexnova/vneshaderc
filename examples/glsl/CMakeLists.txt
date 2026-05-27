#==============================================================================
# Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
# Licensed under the Apache License, Version 2.0 (the "License")
#==============================================================================

if(NOT VNE_SC_GLSLANG)
    message(STATUS "vnesc GLSL examples require VNE_SC_GLSLANG=ON — skipping")
    return()
endif()

add_subdirectory(01_hello_compiler)
add_subdirectory(02_triangle_bundle)
add_subdirectory(03_textured_pipeline_spec)
