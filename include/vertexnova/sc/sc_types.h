#pragma once
/* ---------------------------------------------------------------------
 * Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License")
 *
 * Author:    Ajeet Singh Yadav
 * Created:   May 2026
 *
 * Autodoc:   yes
 * ----------------------------------------------------------------------
 */

/**
 * @file sc_types.h
 * @brief Core types for the vnesc shader compiler library.
 * Defines enumerations, flags, and POD structs used throughout the compilation
 * pipeline.  All types live in the @c vne::sc namespace.
 */

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace vne::sc {

// ─────────────────────────────────────────────────────────────────────────────
// Shader stage
// ─────────────────────────────────────────────────────────────────────────────

/// Identifies a single programmable stage in the graphics or compute pipeline.
enum class ShaderStage : uint8_t {
    eVertex = 0,                  ///< Vertex processing.
    eFragment = 1,                ///< Fragment / pixel shading.
    eCompute = 2,                 ///< General-purpose compute.
    eGeometry = 3,                ///< Geometry amplification.
    eTessellationControl = 4,     ///< Hull / tessellation-control.
    eTessellationEvaluation = 5,  ///< Domain / tessellation-evaluation.
};

/// Bitmask form of @ref ShaderStage for describing multi-stage resources.
enum class ShaderStageFlags : uint32_t {
    eNone = 0,
    eVertex = 1u << 0,
    eFragment = 1u << 1,
    eCompute = 1u << 2,
    eGeometry = 1u << 3,
    eTessellationControl = 1u << 4,
    eTessellationEvaluation = 1u << 5,
    eAll = 0x3Fu,  ///< All stages.
};

inline ShaderStageFlags operator|(ShaderStageFlags a, ShaderStageFlags b) noexcept {
    return static_cast<ShaderStageFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline ShaderStageFlags& operator|=(ShaderStageFlags& a, ShaderStageFlags b) noexcept {
    return a = a | b;
}
/// @returns @c true if any bit in @p b is set in @p a.
inline bool operator&(ShaderStageFlags a, ShaderStageFlags b) noexcept {
    return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Front-end selection
// ─────────────────────────────────────────────────────────────────────────────

/// Selects the compiler front-end that translates source code to SPIR-V.
enum class FrontEnd : uint8_t {
    eGlslang = 0,  ///< glslang — vulkan's GLSL (4.5) to SPIR-V (active).
    eDxc = 1,      ///< DXC    — HLSL to SPIR-V (stub).
    eSlang = 2,    ///< Slang  — Slang to SPIR-V (stub).
};

// ─────────────────────────────────────────────────────────────────────────────
// Cross-compilation target
// ─────────────────────────────────────────────────────────────────────────────

/// Shading-language target for SPIR-V cross-compilation.
enum class CrossTarget : uint8_t {
    eMSL = 0,     ///< Metal Shading Language.
    eWGSL = 1,    ///< WebGPU Shading Language.
    eGLSL = 2,    ///< Desktop GLSL.
    eGLSLES = 3,  ///< GLSL ES (mobile/web).
    eHLSL = 4,    ///< High-Level Shading Language.
};

// ─────────────────────────────────────────────────────────────────────────────
// Source language
// ─────────────────────────────────────────────────────────────────────────────

/// Source language of the shader being compiled.
enum class SourceLang : uint8_t {
    eGLSL = 0,   ///< OpenGL Shading Language (4.5).
    eHLSL = 1,   ///< High-Level Shading Language.
    eMSL = 2,    ///< Metal Shading Language (native, no SPIR-V intermediate).
    eWGSL = 3,   ///< WebGPU Shading Language (native).
    eSlang = 4,  ///< Slang.
};

// ─────────────────────────────────────────────────────────────────────────────
// Optimisation level
// ─────────────────────────────────────────────────────────────────────────────

/// Controls SPIR-V optimisation during compilation.
enum class OptLevel : uint8_t {
    eNone = 0,         ///< No optimisation — fastest compile, largest output.
    eSize = 1,         ///< Minimise SPIR-V binary size.
    ePerformance = 2,  ///< Optimise for runtime performance (default).
};

// ─────────────────────────────────────────────────────────────────────────────
// Compile request
// ─────────────────────────────────────────────────────────────────────────────

/// A preprocessor macro injected into the shader source via a @c #define preamble.
struct ShaderMacro {
    std::string name;   ///< Macro name.
    std::string value;  ///< Macro expansion (may be empty for flag macros).
};

/**
 * @brief Describes a single shader stage compilation job.
 *
 * Either @c source or @c file_path must be non-empty; @c source takes
 * precedence when both are provided.
 */
struct CompileRequest {
    std::string source;                ///< Inline shader source text.
    std::string file_path;             ///< Path to the shader source file.
    std::string entry_point = "main";  ///< Entry-point function name.
    ShaderStage stage = ShaderStage::eVertex;
    SourceLang lang = SourceLang::eGLSL;
    OptLevel opt_level = OptLevel::ePerformance;
    std::vector<ShaderMacro> macros;        ///< Preprocessor definitions.
    std::vector<std::string> include_dirs;  ///< Directories searched for @c #include.
    bool debug_info = false;                ///< Embed debug information in SPIR-V.
    bool validate = true;                   ///< Validate SPIR-V after generation.
    uint32_t glsl_version = 450;            ///< GLSL version number (e.g. 450 → @c #version 450).
};

/**
 * @brief Describes a single SPIR-V → target cross-compilation job.
 */
struct CrossCompileRequest {
    std::vector<uint32_t> spirv;  ///< Input SPIR-V binary.
    CrossTarget target = CrossTarget::eMSL;
    ShaderStage stage = ShaderStage::eVertex;
    uint32_t msl_version = 30000;  ///< MSL version (e.g. 30000 → Metal 3.0).
    uint32_t glsl_version = 450;
    bool fix_msl_fragment_signature = true;  ///< Align fragment stage_in with vertex output.
};

// ─────────────────────────────────────────────────────────────────────────────
// Reflection types
// ─────────────────────────────────────────────────────────────────────────────

/// Metal binding indices derived from SPIRV-Cross automatic slot assignment.
struct MetalResourceSlot {
    uint32_t buffer = 0;   ///< [[buffer(N)]]
    uint32_t texture = 0;  ///< [[texture(N)]]
    uint32_t sampler = 0;  ///< [[sampler(N)]]
};

/// WebGPU binding indices (mirrors Vulkan set/binding layout after WGSL cross-compile).
struct WebGpuResourceSlot {
    uint32_t group = 0;    ///< @group(N)
    uint32_t binding = 0;  ///< @binding(N)
};

/// Per-binding backend slot assignments. std::nullopt means that backend was not compiled.
struct ResourceBackendSlots {
    std::optional<MetalResourceSlot> metal;    ///< Populated when eMSL was a requested target.
    std::optional<WebGpuResourceSlot> webgpu;  ///< Populated when eWGSL was a requested target.
};

/// Classifies a reflected shader resource.
enum class ReflectedResourceType : uint8_t {
    eUniformBuffer = 0,
    eStorageBuffer = 1,
    eSampledImage = 2,
    eStorageImage = 3,
    eSampler = 4,
    ePushConstant = 5,
    eCombinedImageSampler = 6,
    eSampledCubemap = 7,
};

/// A single member of a reflected struct (e.g. a field inside a uniform block).
struct ReflectedStructMember {
    std::string name;
    uint32_t offset = 0;
    uint32_t size = 0;
    uint32_t array_count = 1;
    uint32_t array_stride = 0;
    bool is_matrix = false;
    uint32_t matrix_columns = 0;
    uint32_t matrix_rows = 0;
    std::string type_name;
};

/// Reflection data for one binding point (buffer, image, sampler, etc.).
struct ReflectedBindingInfo {
    std::string name;
    ReflectedResourceType type = ReflectedResourceType::eUniformBuffer;
    uint32_t set = 0;
    uint32_t binding = 0;
    uint32_t array_size = 1;
    ShaderStageFlags stages = ShaderStageFlags::eNone;
    ResourceBackendSlots slots;
    std::vector<ReflectedStructMember> struct_members;  ///< Non-empty for buffer types.
};

/// Workgroup size for compute shaders.
struct WorkgroupSize {
    uint32_t x = 1;
    uint32_t y = 1;
    uint32_t z = 1;
};

/// Reflection data for a single shader stage.
struct StageReflection {
    ShaderStage stage = ShaderStage::eVertex;
    std::vector<ReflectedBindingInfo> bindings;
    uint32_t push_constant_size = 0;  ///< Total size of the push-constant block in bytes.
    WorkgroupSize workgroup_size;
};

/// Reflection data for all stages that make up a shader program.
struct ProgramReflection {
    std::vector<StageReflection> stages;
};

}  // namespace vne::sc
