/* ---------------------------------------------------------------------
 * Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License")
 *
 * Author:    Ajeet Singh Yadav
 * Created:   May 2026
 * ----------------------------------------------------------------------
 */

#include "spirvcross_reflector.h"

#include "spirv_cross.hpp"
#include "spirv_msl.hpp"

#include <sstream>
#include <string>
#include <vector>
#include <cstdint>
#include <exception>

namespace {

// ── Metal binding constants (must match kFlattenBinding / kMetalUboBase in cross-compiler) ──
constexpr uint32_t kFlattenBinding = 32u;
constexpr uint32_t kMetalUboBase   = 16u;

inline uint32_t metalBuf(uint32_t set, uint32_t binding) noexcept {
    return kMetalUboBase + (set * kFlattenBinding + binding);
}
inline uint32_t metalTex(uint32_t set, uint32_t binding) noexcept {
    return set * kFlattenBinding + binding;
}
inline uint32_t metalSmp(uint32_t set, uint32_t binding) noexcept {
    return set * kFlattenBinding + binding;
}

// ── String helpers ─────────────────────────────────────────────────────────────
const char* typeStr(vne::sc::ReflectedResourceType t) noexcept {
    using T = vne::sc::ReflectedResourceType;
    switch (t) {
        case T::eUniformBuffer:       return "uniform_buffer";
        case T::eStorageBuffer:       return "storage_buffer";
        case T::eSampledImage:        return "sampled_image";
        case T::eSampledCubemap:      return "sampled_image";  // same tag; runtime distinguishes via size hint
        case T::eStorageImage:        return "storage_image";
        case T::eSampler:             return "sampler";
        case T::ePushConstant:        return "push_constant";
        case T::eCombinedImageSampler: return "combined_image_sampler";
    }
    return "uniform_buffer";
}

const char* stageStr(vne::sc::ShaderStage s) noexcept {
    using S = vne::sc::ShaderStage;
    switch (s) {
        case S::eVertex:                  return "vertex";
        case S::eFragment:                return "fragment";
        case S::eCompute:                 return "compute";
        case S::eGeometry:                return "geometry";
        case S::eTessellationControl:     return "tess_control";
        case S::eTessellationEvaluation:  return "tess_eval";
    }
    return "vertex";
}

// ── Minimal JSON writer (no external dependency) ──────────────────────────────
// Escapes a string for JSON embedding.
std::string jsonStr(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    out += '"';
    for (char c : s) {
        if (c == '"') { out += "\\\""; }
        else if (c == '\\') { out += "\\\\"; }
        else if (c == '\n') { out += "\\n"; }
        else if (c == '\r') { out += "\\r"; }
        else if (c == '\t') { out += "\\t"; }
        else { out += c; }
    }
    out += '"';
    return out;
}

// ── Process one resource list, appending JSON binding objects ─────────────────
template<vne::sc::ReflectedResourceType Type>
void appendBindings(const spirv_cross::Compiler&    compiler,
                    const spirv_cross::CompilerMSL&  msl,
                    const spirv_cross::SmallVector<spirv_cross::Resource>& list,
                    vne::sc::ShaderStage             stage,
                    std::ostringstream&              json,
                    bool&                            first) {
    for (const auto& res : list) {
        uint32_t set     = compiler.get_decoration(res.id, spv::DecorationDescriptorSet);
        uint32_t binding = compiler.get_decoration(res.id, spv::DecorationBinding);

        const spirv_cross::SPIRType& ty = compiler.get_type(res.type_id);
        uint32_t array_size = ty.array.empty() ? 1u : (ty.array[0] == 0 ? 0u : ty.array[0]);

        // Buffer size (for uniform/storage buffers)
        uint32_t size = 0;
        if constexpr (Type == vne::sc::ReflectedResourceType::eUniformBuffer ||
                      Type == vne::sc::ReflectedResourceType::eStorageBuffer) {
            try {
                const spirv_cross::SPIRType& block = compiler.get_type(res.base_type_id);
                size = static_cast<uint32_t>(compiler.get_declared_struct_size(block));
            } catch (...) {}
        }

        // Detect cubemap
        vne::sc::ReflectedResourceType actual_type = Type;
        if constexpr (Type == vne::sc::ReflectedResourceType::eSampledImage) {
            if (ty.image.dim == spv::DimCube) {
                actual_type = vne::sc::ReflectedResourceType::eSampledCubemap;
            }
        }

        // Metal automatic binding slot via CompilerMSL
        uint32_t metal_buf  = 0;
        uint32_t metal_tex  = 0;
        uint32_t metal_smp  = 0;
        bool     metal_ok   = false;
        try {
            uint32_t primary = msl.get_automatic_msl_resource_binding(res.id);
            if (primary != ~0u) {
                metal_ok = true;
                if constexpr (Type == vne::sc::ReflectedResourceType::eUniformBuffer ||
                              Type == vne::sc::ReflectedResourceType::eStorageBuffer) {
                    metal_buf = primary;
                } else if constexpr (Type == vne::sc::ReflectedResourceType::eSampledImage ||
                                     Type == vne::sc::ReflectedResourceType::eStorageImage) {
                    metal_tex = primary;
                } else if constexpr (Type == vne::sc::ReflectedResourceType::eSampler) {
                    metal_smp = primary;
                } else if constexpr (Type == vne::sc::ReflectedResourceType::eCombinedImageSampler) {
                    metal_tex = primary;
                    uint32_t sec = msl.get_automatic_msl_resource_binding_secondary(res.id);
                    if (sec != ~0u) metal_smp = sec;
                }
            }
        } catch (...) {}

        // Fallback: derive from formula if MSL API didn't provide a slot
        if (!metal_ok) {
            if constexpr (Type == vne::sc::ReflectedResourceType::eUniformBuffer ||
                          Type == vne::sc::ReflectedResourceType::eStorageBuffer) {
                metal_buf = metalBuf(set, binding);
            } else {
                metal_tex = metalTex(set, binding);
                metal_smp = metalSmp(set, binding);
            }
        }

        if (!first) json << ",";
        first = false;

        json << "\n    {"
             << "\n      \"name\":" << jsonStr(res.name) << ","
             << "\n      \"type\":" << jsonStr(typeStr(actual_type)) << ","
             << "\n      \"set\":" << set << ","
             << "\n      \"binding\":" << binding << ","
             << "\n      \"size\":" << size << ","
             << "\n      \"array_size\":" << array_size << ","
             << "\n      \"stages\":[" << jsonStr(stageStr(stage)) << "],"
             << "\n      \"overrides\":{"
             << "\n        \"metal\":{\"buffer\":" << metal_buf
             << ",\"texture\":" << metal_tex
             << ",\"sampler\":" << metal_smp << "},"
             << "\n        \"wgpu\":{\"group\":" << set << ",\"binding\":" << binding << "}"
             << "\n      }"
             << "\n    }";
    }
}

}  // namespace

namespace vne::sc {

ReflectResult SpirvCrossReflector::reflectToJson(const std::vector<uint32_t>& spirv,
                                                  ShaderStage stage) {
    ReflectResult result;
    if (spirv.empty()) {
        result.code  = ResultCode::eReflectionFailed;
        result.error = "SpirvCrossReflector: empty SPIR-V";
        return result;
    }

    try {
        // Base compiler for Vulkan reflection
        spirv_cross::Compiler compiler(spirv.data(), spirv.size());

        // MSL compiler to populate automatic Metal slot assignments
        spirv_cross::CompilerMSL msl_compiler(spirv.data(), spirv.size());
        {
            spirv_cross::CompilerMSL::Options msl_opts;
            msl_opts.platform = spirv_cross::CompilerMSL::Options::macOS;
            msl_compiler.set_msl_options(msl_opts);
            try {
                msl_compiler.compile();
            } catch (...) {
                // Non-fatal — fallback formula will be used
            }
        }

        const auto resources = compiler.get_shader_resources();

        std::ostringstream json;
        json << "{";
        json << "\n  \"name\":" << jsonStr(stageStr(stage)) << ",";
        json << "\n  \"bindings\":[";
        bool first = true;

        appendBindings<ReflectedResourceType::eUniformBuffer>(
            compiler, msl_compiler, resources.uniform_buffers, stage, json, first);
        appendBindings<ReflectedResourceType::eStorageBuffer>(
            compiler, msl_compiler, resources.storage_buffers, stage, json, first);
        appendBindings<ReflectedResourceType::eSampledImage>(
            compiler, msl_compiler, resources.separate_images, stage, json, first);
        appendBindings<ReflectedResourceType::eStorageImage>(
            compiler, msl_compiler, resources.storage_images, stage, json, first);
        appendBindings<ReflectedResourceType::eSampler>(
            compiler, msl_compiler, resources.separate_samplers, stage, json, first);
        appendBindings<ReflectedResourceType::eCombinedImageSampler>(
            compiler, msl_compiler, resources.sampled_images, stage, json, first);

        if (!first) json << "\n  ";
        json << "]";
        json << "\n}";

        result.json = json.str();
        result.code = ResultCode::eSuccess;
        return result;

    } catch (const std::exception& e) {
        result.code  = ResultCode::eReflectionFailed;
        result.error = std::string("SpirvCrossReflector: ") + e.what();
        return result;
    }
}

}  // namespace vne::sc
