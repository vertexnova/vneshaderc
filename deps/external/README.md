# External dependencies

Third-party dependencies live here. Khronos shader toolchain deps are pinned to tag `vulkan-sdk-1.3.296.0` (see `cmake/VnescDeps.cmake`).

## Initialize all external submodules

From the project root:

```bash
git submodule update --init --recursive
```

## Submodules

| Path | Purpose |
|------|---------|
| `googletest/` | Unit tests (gtest, gmock) |
| `SPIRV-Cross/` | SPIR-V cross-compilation and reflection |
| `SPIRV-Headers/` | SPIR-V headers (required by glslang) |
| `glslang/` | GLSL → SPIR-V front-end |
| `SPIRV-Tools/` | SPIR-V validation (when `VNE_SC_SPIRVTOOLS=ON`) |
| `nlohmann_json/` | JSON manifests and bundle metadata (when `VNE_SC_JSON=ON`) |

If a vendored tree is missing or empty, CMake uses FetchContent at configure time. Do not set `VNE_SC_*_DIR` to an empty `deps/external/*` path unless you have a valid checkout elsewhere.
