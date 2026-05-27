# vnesc Examples

Examples demonstrating the vnesc offline shader compiler API. GLSL scenarios live under `glsl/`; future trees may add `hlsl/` or `slang/`.

## Building

From the project root:

```bash
cmake -B build -DVNE_SC_GLSLANG=ON -DVNE_SC_EXAMPLES=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target \
    vnesc_example_01_hello_compiler \
    vnesc_example_02_triangle_bundle \
    vnesc_example_03_textured_pipeline_spec
```

Executables are under `build/bin/examples/glsl/<example>/`.

## Examples

| Example | Focus |
|---------|--------|
| [glsl/01_hello_compiler](glsl/01_hello_compiler/) | Inline GLSL → MSL, `PipelineBuildDesc` |
| [glsl/02_triangle_bundle](glsl/02_triangle_bundle/) | File-based shaders, reflection dump, bundle I/O |
| [glsl/03_textured_pipeline_spec](glsl/03_textured_pipeline_spec/) | `.pipeline.json` spec, `MetalBindingLayout` |

## Shared utilities

- `common/shader_loader.h` — load a shader file into a `std::string`
- `common/logging_guard.h` — optional vnelogging RAII helper (requires `vnelogging` submodule)
