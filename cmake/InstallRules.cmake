#==============================================================================
# Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
# Licensed under the Apache License, Version 2.0 (the "License")
#
# cmake/InstallRules.cmake
# Install rules for vnesc library targets and public headers.
# Included from the bottom of the top-level CMakeLists.txt.
#==============================================================================

include(GNUInstallDirs)

set(_vnesc_install_targets
    vnesc
    vnesc_interface
    vnesc_spirvcross
    vnesc_validator
    VnescWarnings
    VnescBuildSettings)

if(VNE_SC_GLSLANG AND TARGET vnesc_glslang)
    list(APPEND _vnesc_install_targets vnesc_glslang)
endif()
if(VNE_SC_TINT AND TARGET vnesc_tint)
    list(APPEND _vnesc_install_targets vnesc_tint)
endif()

install(TARGETS ${_vnesc_install_targets}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    COMPONENT vnesc)

install(DIRECTORY include/vertexnova/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/vertexnova
    COMPONENT vnesc
    FILES_MATCHING PATTERN "*.h")

# CLI tool
if(VNE_SC_TOOLS AND TARGET vnesc_shader_compiler)
    install(TARGETS vnesc_shader_compiler
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT vnesc_tools)
endif()

# Python batch wrapper
if(VNE_SC_TOOLS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/compile_shaders.py")
    install(PROGRAMS "${CMAKE_CURRENT_SOURCE_DIR}/tools/compile_shaders.py"
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT vnesc_tools)
endif()
