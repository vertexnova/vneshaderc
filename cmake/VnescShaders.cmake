#==============================================================================
# Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
# Licensed under the Apache License, Version 2.0 (the "License")
#
# Author:    Ajeet Singh Yadav
# Created:   February 2026
#
# Autodoc:   yes
#==============================================================================

# VnescShaders.cmake - CMake helper for build-time shader bundle compilation
#
# Usage:
#   include(VnescShaders)
#
#   vne_compile_shaders(<target>
#       OUTPUT_DIR  <dir>        # Where .vneshader directories are written
#       MANIFESTS   <m1> <m2>...  # Paths to .manifest.json files
#       [CACHE_DIR  <dir>]       # Optional artifact cache directory
#   )
#
# Each manifest produces a <name>.vneshader directory under OUTPUT_DIR.
# The bundle's bundle.header file is added as a source dependency so the
# target rebuilds whenever a manifest or the vnesc_shader_compiler binary
# changes.

#==============================================================================
#                            vne_compile_shaders()                             #
#==============================================================================

function(vne_compile_shaders TARGET)
    cmake_parse_arguments(ARGS "" "OUTPUT_DIR;CACHE_DIR" "MANIFESTS" ${ARGN})

    if(NOT ARGS_OUTPUT_DIR)
        message(FATAL_ERROR "vne_compile_shaders: OUTPUT_DIR is required")
    endif()
    if(NOT ARGS_MANIFESTS)
        message(FATAL_ERROR "vne_compile_shaders: at least one MANIFEST is required")
    endif()

    if(NOT TARGET vnesc_shader_compiler)
        message(FATAL_ERROR
            "vne_compile_shaders: vnesc_shader_compiler target not found. "
            "Build vnesc with VNE_SC_TOOLS=ON or add vnesc as a subdirectory.")
    endif()

    foreach(manifest_path IN LISTS ARGS_MANIFESTS)
        get_filename_component(manifest_name "${manifest_path}" NAME_WE)
        set(bundle_dir "${ARGS_OUTPUT_DIR}/${manifest_name}.vneshader")
        set(stamp_file "${bundle_dir}/bundle.header")

        set(_cmd_args
            --manifest "${manifest_path}"
            --output   "${bundle_dir}")

        if(ARGS_CACHE_DIR)
            list(APPEND _cmd_args --cache "${ARGS_CACHE_DIR}")
        endif()

        add_custom_command(
            OUTPUT  "${stamp_file}"
            COMMAND vnesc_shader_compiler ${_cmd_args}
            DEPENDS "${manifest_path}" vnesc_shader_compiler
            COMMENT "Compiling shader bundle: ${manifest_name}"
            VERBATIM
        )

        # Add the stamp as a source so CMake tracks the dependency.
        target_sources("${TARGET}" PRIVATE "${stamp_file}")
    endforeach()
endfunction()
