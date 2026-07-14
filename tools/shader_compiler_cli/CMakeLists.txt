#==============================================================================
# Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
# Licensed under the Apache License, Version 2.0 (the "License")
#
# Author:    Ajeet Singh Yadav
# Created:   February 2026
#
# Autodoc:   yes
#==============================================================================

# vnesc_shader_compiler - offline shader compilation CLI tool

#==============================================================================
#                              Build Executable                                #
#==============================================================================

add_executable(vnesc_shader_compiler main.cpp)

#==============================================================================
#                                Dependencies                                  #
#==============================================================================

target_link_libraries(vnesc_shader_compiler PRIVATE vnesc)
target_compile_features(vnesc_shader_compiler PRIVATE cxx_std_20)

set_target_properties(vnesc_shader_compiler PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

#==============================================================================
#                                 Installation                                 #
#==============================================================================

install(TARGETS vnesc_shader_compiler
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT vnesc_tools
)
