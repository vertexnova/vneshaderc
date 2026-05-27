#==============================================================================
# 02_triangle_bundle — file-based GLSL → MSL bundle + reflection dump
#==============================================================================

add_executable(vnesc_example_02_triangle_bundle main.cpp)

target_include_directories(vnesc_example_02_triangle_bundle PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/examples/common)

target_link_libraries(vnesc_example_02_triangle_bundle PRIVATE vnesc vnesc_glslang)

set_target_properties(vnesc_example_02_triangle_bundle PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/examples/glsl/02_triangle_bundle")
