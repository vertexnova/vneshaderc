# vnesc offline shader pipeline

## Build options

| CMake option | Default | Purpose |
|--------------|---------|---------|
| `VNE_SC_GLSLANG` | ON | GLSL -> SPIR-V (glslang) |
| `VNE_SC_TINT` | OFF | SPIR-V -> WGSL (Dawn Tint; slow first configure) |
| `VNE_SC_JSON` | ON | `manifest.json` + pipeline build JSON |
| `VNE_SC_SPIRVTOOLS` | OFF | SPIR-V validation |

## Typical flow

```cpp
#include "vertexnova/sc/vnesc.h"

auto builder = vne::sc::ShaderCompilerFactory::createPipelineBuilder(vne::sc::SourceLang::eGLSL);
vne::sc::PipelineBuildDesc desc;
desc.name = "scene";
desc.targets = {vne::sc::CrossTarget::eMSL, vne::sc::CrossTarget::eWGSL};
// ... add CompileRequest per stage ...
auto result = builder->build(desc);
vne::sc::writeShaderBundle(result.artifact, "shaders/gen/scene.vneshader");
```

## vnerhi consumption

```cpp
#include "vertexnova/rhi/shader_bundle_loader.h"

auto loaded = vne::rhi::loadShaderBundle("shaders/gen/scene.vneshader");
shader_library->initialize(loaded.package);  // uses baked_reflection
```

See [separate_samplers.md](separate_samplers.md) for WGSL GLSL constraints.
