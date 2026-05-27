<p align="center">
  <img src="icons/vertexnova_logo_medallion_with_text.svg" alt="VertexNova Shader Compiler" width="320"/>
</p>

<p align="center">
  <strong>Offline shader compiler library for the VertexNova ecosystem</strong>
</p>

<p align="center">
  <a href="https://github.com/vertexnova/vnesc/actions/workflows/ci.yml">
    <img src="https://github.com/vertexnova/vnesc/actions/workflows/ci.yml/badge.svg?branch=main" alt="CI"/>
  </a>
  <a href="https://codecov.io/gh/vertexnova/vnesc">
    <img src="https://codecov.io/gh/vertexnova/vnesc/branch/main/graph/badge.svg" alt="Coverage"/>
  </a>
  <img src="https://img.shields.io/badge/C%2B%2B-20-blue.svg" alt="C++ Standard"/>
  <img src="https://img.shields.io/badge/license-Apache%202.0-green.svg" alt="License"/>
</p>

---

## About

**vnesc** is an offline shader compiler library that transforms GLSL source into a portable `.vneshader` bundle: SPIR-V bytecode, cross-compiled MSL/WGSL/HLSL output, and typed resource-binding reflection — all ready for direct consumption by [vnerhi](https://github.com/vertexnova/vnerhi).

It is a core component of [VertexNova](https://github.com/vertexnova), a multi-backend graphics engine targeting Vulkan, Metal, WebGPU, and OpenGL/ES from a single source.

## Features

- **Full offline pipeline**: GLSL → SPIR-V → validate → reflect → cross-compile in one call
- **Multi-target output**: MSL (Metal), WGSL (WebGPU), HLSL, GLSL ES from a single GLSL source
- **Typed reflection**: per-binding resource metadata with typed optional backend slots (`MetalResourceSlot`, `WebGpuResourceSlot`)
- **Artifact cache**: content-addressed file cache skips recompiling unchanged shaders
- **`.vneshader` bundle**: self-contained directory — SPIR-V + cross-compiled sources + binary reflection
- **JSON pipeline spec**: declare a full multi-stage pipeline in a `.pipeline.json` file; `include_paths` resolve relative to the spec file
- **CLI tool**: `vnesc_shader_compiler` drives the full pipeline from the command line
- **Python batch tool**: `tools/compile_shaders.py` — parallel glob-aware wrapper around the CLI
- **Extensible interfaces**: `IShaderFrontEnd`, `IShaderCrossCompiler`, `IShaderReflector`, `IShaderValidator`
- **C++20, header-only public API**: single umbrella include `<vertexnova/sc/vnesc.h>`

## Installation

### As a git submodule (recommended for VertexNova projects)

```bash
git submodule add https://github.com/vertexnova/vnesc.git deps/internal/vnesc
git submodule update --init --recursive
```

```cmake
add_subdirectory(deps/internal/vnesc)
target_link_libraries(your_target PRIVATE vnesc vnesc_glslang)
```

### FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(vnesc
    GIT_REPOSITORY https://github.com/vertexnova/vnesc.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(vnesc)
target_link_libraries(your_target PRIVATE vnesc vnesc_glslang)
```

> When used as a dependency, tests and examples are off by default (`VNE_SC_TESTS=OFF`, `VNE_SC_EXAMPLES=OFF`).

## Using vnesc as a library

### CMake integration

```cmake
target_link_libraries(your_target PRIVATE
    vnesc          # core: cache, factory, pipeline builder, bundle I/O
    vnesc_glslang) # glslang GLSL → SPIR-V front-end (required for GLSL source)
```

`vnesc` carries the public headers, SPIRV-Cross cross-compiler, reflector, and bundle writer.
`vnesc_glslang` adds the GLSL → SPIR-V front-end; omit it only if you supply SPIR-V directly.

### Single umbrella include

```cpp
#include <vertexnova/sc/vnesc.h>
```

### Key types at a glance

| Header | Key types |
|--------|-----------|
| `sc_types.h` | `ShaderStage`, `CrossTarget`, `SourceLang`, `CompileRequest`, `CrossCompileRequest`, `MetalBindingLayout` |
| `shader_pipeline_builder.h` | `PipelineBuildDesc`, `PipelineBuildResult` |
| `shader_pipeline_spec.h` | `ShaderPipelineSpec`, `loadShaderPipelineSpec()` |
| `shader_compiler_factory.h` | `ShaderCompilerFactory` |
| `shader_bundle.h` | `writeShaderBundle()`, `readShaderBundle()` |
| `shader_artifact.h` | `ShaderArtifact`, `StageArtifact`, `CrossCompiledSource` |

Tooling details: [tools/README.md](tools/README.md). Examples: [examples/](examples/).

## Building

```bash
git clone --recursive https://github.com/vertexnova/vnesc.git
cd vnesc

# Configure (glslang + JSON enabled by default)
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Build library + CLI tool
cmake --build build --target vnesc vnesc_shader_compiler -j$(nproc)

# Run tests
ctest --test-dir build --output-on-failure
```

### Platform scripts

```bash
# macOS
./scripts/build_macos.sh -a configure_and_build
./scripts/build_macos.sh -a test -clean

# Linux
./scripts/build_linux.sh -t Release -a configure_and_build
./scripts/build_linux.sh -c clang -a test

# Windows (PowerShell)
.\scripts\build_windows.ps1 -BuildType Release -Action configure_and_build
.\scripts\build_windows.ps1 -Action test
```

Options: `-t` / `-BuildType` build type, `-a` / `-Action` action (`configure_and_build` | `build` | `test`), `-clean`, `-j N` jobs. macOS also accepts `-xcode` for an Xcode project.

### CMake options

| Option | Default | Description |
|--------|---------|-------------|
| `VNE_SC_GLSLANG` | `ON` | Enable glslang GLSL → SPIR-V front-end |
| `VNE_SC_JSON` | `ON` | Enable nlohmann/json for `.pipeline.json` specs and bundle manifests |
| `VNE_SC_SPIRVTOOLS` | `OFF` | Enable SPIRV-Tools SPIR-V validator |
| `VNE_SC_TINT` | `OFF` | Enable Dawn Tint SPIR-V → WGSL cross-compiler |
| `VNE_SC_DXC` | `OFF` | Enable DXC HLSL → SPIR-V front-end (stub) |
| `VNE_SC_SLANG` | `OFF` | Enable Slang front-end (stub) |
| `VNE_SC_TESTS` | `ON` | Build the test suite |
| `VNE_SC_EXAMPLES` | `OFF` | Build example programs |
| `VNE_SC_TOOLS` | `ON` | Build `vnesc_shader_compiler` CLI |
| `ENABLE_DOXYGEN` | `OFF` | Generate API docs with Doxygen |
| `ENABLE_COVERAGE` | `OFF` | Enable gcov/lcov code coverage |
| `ENABLE_ASAN` | `OFF` | Enable AddressSanitizer + UBSan |
| `WARNINGS_AS_ERRORS` | `OFF` | Treat compiler warnings as errors |

## Quick start

### Compile a single shader (low-level)

```cpp
#include <vertexnova/sc/vnesc.h>

auto factory = vne::sc::ShaderCompilerFactory::createFrontEnd(vne::sc::SourceLang::eGLSL);

vne::sc::CompileRequest req;
req.source    = glsl_source_string;
req.stage     = vne::sc::ShaderStage::eVertex;
req.entry_point = "main";

auto result = factory->compile(req);
if (!result.ok()) {
    // result.errors contains per-error messages
}
// result.spirv — compiled SPIR-V words
```

### Build a full pipeline (recommended)

```cpp
#include <vertexnova/sc/vnesc.h>

// Factory creates and wires all components for you
auto builder = vne::sc::ShaderCompilerFactory::createPipelineBuilder(vne::sc::SourceLang::eGLSL);

vne::sc::PipelineBuildDesc desc;
desc.name     = "mesh_phong";
desc.targets  = { vne::sc::CrossTarget::eMSL, vne::sc::CrossTarget::eWGSL };
desc.validate = true;

vne::sc::CompileRequest vert, frag;
vert.source = vert_glsl;  vert.stage = vne::sc::ShaderStage::eVertex;
frag.source = frag_glsl;  frag.stage = vne::sc::ShaderStage::eFragment;
desc.stages  = { vert, frag };

auto result = builder->build(desc);
if (result.ok()) {
    vne::sc::writeShaderBundle(result.artifact, "output/mesh_phong.vneshader");
}
```

### Load a pipeline spec from JSON

```cpp
#include <vertexnova/sc/vnesc.h>

auto spec = vne::sc::loadShaderPipelineSpec("shaders/src/mesh_phong.pipeline.json");
if (spec) {
    auto desc    = spec->toBuildDesc(std::filesystem::path("shaders/src"));
    auto builder = vne::sc::ShaderCompilerFactory::createPipelineBuilder(spec->source_lang);
    auto result  = builder->build(desc);
}
```

## Pipeline spec format (`.pipeline.json`)

```json
{
  "name": "mesh_phong",
  "source_lang": "glsl",
  "include_paths": ["../common/glsl"],
  "targets": ["msl", "wgsl"],
  "stages": [
    { "stage": "vertex",   "file": "mesh_phong.vert.glsl" },
    { "stage": "fragment", "file": "mesh_phong.frag.glsl" }
  ]
}
```

`include_paths` entries are resolved relative to the spec file's directory and forwarded as `#include` search paths to glslang.

Optional Metal binding layout (defaults match vnerhi):

```json
"metal_layout": {
  "flatten_stride": 32,
  "buffer_base": 16
}
```

## CLI tool

See [tools/README.md](tools/README.md) for full flag documentation.

```bash
vnesc_shader_compiler \
    --manifest shaders/src/mesh_phong.pipeline.json \
    --output   output/bundles/mesh_phong \
    --cache    .shader_cache

python3 tools/compile_shaders.py \
    --manifest "shaders/src/**/*.pipeline.json" \
    --output   output/bundles \
    --cache    .shader_cache \
    --parallel 8
```

## Bundle format (`.vneshader`)

A compiled `.vneshader` is a directory:

| File | Contents |
|------|----------|
| `bundle.header` | Binary index: stage list, entry points, file names |
| `reflection.bin` | Binary `ProgramReflection` (v2 format) |
| `manifest.json` | Human-readable index (when `VNE_SC_JSON` is enabled) |
| `<stage>.spv` | SPIR-V bytecode |
| `<stage>.msl` | MSL source (when `msl` target requested) |
| `<stage>.wgsl` | WGSL source (when `wgsl` target requested) |

The binary reflection (`reflection.bin`) stores typed per-backend resource slots:

```
metal:  { buffer, texture, sampler }   — present only when MSL was compiled
webgpu: { group, binding }             — present only when WGSL was compiled
```

## Platform support

| Platform | Status | Compiler |
|----------|--------|---------|
| macOS | Tested | Apple Clang 15+, Clang 17+ |
| Linux | Tested | GCC 10+, Clang 10+ |
| Windows | Tested | MSVC 2019+ |

## Documentation

- **Architecture doc**: [docs/vertexnova/sc/](docs/vertexnova/sc/) — pipeline design, reflection format, bundle layout
- **API docs**: Build with Doxygen:

  ```bash
  cmake -B build -DENABLE_DOXYGEN=ON
  cmake --build build --target vnesc_doc_doxygen
  # Output: build/docs/html/index.html
  ```

- **Doxygen script**:

  ```bash
  ./scripts/generate-docs.sh
  ```

## Requirements

- C++20 or later
- CMake 3.19+
- Compiler: GCC 10+, Clang 10+, MSVC 2019+
- Python 3.10+ (for `tools/compile_shaders.py`)

## Releases

The **VERSION** file at the repo root is the single source of truth; CMake reads it at configure time. Releases are cut via [release-please](https://github.com/googleapis/release-please) on merge to `main`. See [CHANGELOG.md](CHANGELOG.md) for the full history.

## License

Apache License 2.0 — See [LICENSE](LICENSE) for details.

---

<p align="center">
  Part of the <a href="https://github.com/vertexnova">VertexNova</a> project
</p>
