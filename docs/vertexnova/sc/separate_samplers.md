# GLSL separate samplers for WebGPU (WGSL)

vnesc produces WGSL via **Tint** (SPIR-V -> WGSL). Tint does not support Vulkan-style **combined** `sampler2D` bindings in the WGSL output path.

## Required GLSL style (all targets that include WGSL)

```glsl
layout(set = 0, binding = 0) uniform texture2D albedoTex;
layout(set = 0, binding = 1) uniform sampler   albedoSampler;

void main() {
    vec4 c = texture(sampler2D(albedoTex, albedoSampler), uv);
}
```

## Avoid for WGSL builds

```glsl
layout(set = 0, binding = 0) uniform sampler2D albedoTex;  // combined - fails Tint -> WGSL
```

SPIRV-Cross may still cross-compile combined samplers to MSL for Metal-only pipelines, but the engine should migrate to **separate texture + sampler** for one GLSL corpus across Vulkan, Metal, and WebGPU.

## Offline bundle output

When `CrossTarget::eWGSL` is in the build targets, vnesc emits:

- `*.wgsl` per stage in the `.vneshader` bundle
- `reflection.bin` with `wgpu_group` / `wgpu_binding` matching SPIR-V `(set, binding)`

vnerhi loads the bundle via `loadShaderBundle()` and uses baked reflection - no runtime WGSL parsing in production.
