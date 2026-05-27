# 01_hello_compiler

Minimal vnesc example: inline GLSL vertex + fragment shaders compiled to MSL with no external shader files.

- Uses `ShaderCompilerFactory::createPipelineBuilder` and `PipelineBuildDesc`
- Prints generated MSL to stdout
- Writes a bundle under the system temp directory (`hello_compiler_bundle`)

```bash
cmake --build build --target vnesc_example_01_hello_compiler
./build/bin/examples/glsl/01_hello_compiler/vnesc_example_01_hello_compiler
```
