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

#include "spirvcross_reflector.h"

#include "spirv_cross.hpp"
#include "spirv_msl.hpp"

#include "vertexnova/logging/logging.h"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <memory>
#include <string>
#include <vector>

CREATE_VNE_LOGGER_CATEGORY("vne.sc.reflector")

namespace {

// ── Metal binding constants (must match kFlattenBinding / kMetalUboBase in cross-compiler) ──
constexpr uint32_t kFlattenBinding = 32u;
constexpr uint32_t kMetalUboBase = 16u;

inline uint32_t metalBuf(uint32_t set, uint32_t binding) noexcept {
    return kMetalUboBase + (set * kFlattenBinding + binding);
}
inline uint32_t metalTex(uint32_t set, uint32_t binding) noexcept {
    return set * kFlattenBinding + binding;
}
inline uint32_t metalSmp(uint32_t set, uint32_t binding) noexcept {
    return set * kFlattenBinding + binding;
}

// ── Reflect struct members from a SPIRV-Cross block type ─────────────────────
std::vector<vne::sc::ReflectedStructMember> reflectStructMembers(const spirv_cross::Compiler& compiler,
                                                                 const spirv_cross::SPIRType& block_type) {
    std::vector<vne::sc::ReflectedStructMember> members;
    const uint32_t member_count = static_cast<uint32_t>(block_type.member_types.size());
    members.reserve(member_count);

    for (uint32_t i = 0; i < member_count; ++i) {
        vne::sc::ReflectedStructMember m;
        m.name = compiler.get_member_name(block_type.self, i);

        const spirv_cross::SPIRType& mt = compiler.get_type(block_type.member_types[i]);
        m.offset = compiler.type_struct_member_offset(block_type, i);
        m.size = static_cast<uint32_t>(compiler.get_declared_struct_member_size(block_type, i));
        m.array_count = mt.array.empty() ? 1u : (mt.array[0] == 0u ? 0u : mt.array[0]);
        m.array_stride =
            mt.array.empty() ? 0u : static_cast<uint32_t>(compiler.type_struct_member_array_stride(block_type, i));
        m.is_matrix = mt.columns > 1;
        if (m.is_matrix) {
            m.matrix_columns = mt.columns;
            m.matrix_rows = mt.vecsize;
        }
        m.type_name = compiler.get_name(mt.self);
        members.push_back(std::move(m));
    }
    return members;
}

// ── Populate one ReflectedBindingInfo from a SPIRV-Cross resource ────────────
template<vne::sc::ReflectedResourceType Type>
void appendBinding(const spirv_cross::Compiler& compiler,
                   const spirv_cross::CompilerMSL* msl,  // nullptr → MSL not a target
                   bool has_webgpu,
                   const spirv_cross::Resource& res,
                   vne::sc::ShaderStage stage,
                   std::vector<vne::sc::ReflectedBindingInfo>& out) {
    using namespace vne::sc;

    const uint32_t set = compiler.get_decoration(res.id, spv::DecorationDescriptorSet);
    const uint32_t binding = compiler.get_decoration(res.id, spv::DecorationBinding);

    const spirv_cross::SPIRType& ty = compiler.get_type(res.type_id);
    const uint32_t array_size = ty.array.empty() ? 1u : (ty.array[0] == 0u ? 0u : ty.array[0]);

    // Classify cubemap images separately
    ReflectedResourceType actual_type = Type;
    if constexpr (Type == ReflectedResourceType::eSampledImage) {
        if (ty.image.dim == spv::DimCube) {
            actual_type = ReflectedResourceType::eSampledCubemap;
        }
    }

    // ── Backend slot assignment ───────────────────────────────────────────────
    ResourceBackendSlots slots;

    if (msl) {
        MetalResourceSlot metal;
        bool from_spirvcross = false;
        try {
            const uint32_t primary = msl->get_automatic_msl_resource_binding(res.id);
            if (primary != ~0u) {
                from_spirvcross = true;
                if constexpr (Type == ReflectedResourceType::eUniformBuffer
                              || Type == ReflectedResourceType::eStorageBuffer) {
                    metal.buffer = primary;
                } else if constexpr (Type == ReflectedResourceType::eSampledImage
                                     || Type == ReflectedResourceType::eStorageImage) {
                    metal.texture = primary;
                } else if constexpr (Type == ReflectedResourceType::eSampler) {
                    metal.sampler = primary;
                } else if constexpr (Type == ReflectedResourceType::eCombinedImageSampler) {
                    metal.texture = primary;
                    const uint32_t sec = msl->get_automatic_msl_resource_binding_secondary(res.id);
                    if (sec != ~0u) {
                        metal.sampler = sec;
                    }
                }
            }
        } catch (...) {
        }
        if (!from_spirvcross) {
            // Deterministic fallback formula when SPIRV-Cross auto-binding unavailable
            if constexpr (Type == ReflectedResourceType::eUniformBuffer
                          || Type == ReflectedResourceType::eStorageBuffer) {
                metal.buffer = metalBuf(set, binding);
            } else {
                metal.texture = metalTex(set, binding);
                metal.sampler = metalSmp(set, binding);
            }
        }
        slots.metal = metal;
    }

    if (has_webgpu) {
        slots.webgpu = WebGpuResourceSlot{set, binding};
    }

    // ── Assemble binding info ─────────────────────────────────────────────────
    ReflectedBindingInfo info;
    info.name = res.name;
    info.type = actual_type;
    info.set = set;
    info.binding = binding;
    info.array_size = array_size;
    info.stages = ShaderStageFlags::eNone;  // caller sets stage flags
    info.slots = slots;

    // Struct members for buffer types
    if constexpr (Type == ReflectedResourceType::eUniformBuffer || Type == ReflectedResourceType::eStorageBuffer) {
        try {
            const spirv_cross::SPIRType& block = compiler.get_type(res.base_type_id);
            info.struct_members = reflectStructMembers(compiler, block);
        } catch (...) {
        }
    }

    out.push_back(std::move(info));
}

vne::sc::ShaderStageFlags stageToFlag(vne::sc::ShaderStage s) noexcept {
    using F = vne::sc::ShaderStageFlags;
    using S = vne::sc::ShaderStage;
    switch (s) {
        case S::eVertex:
            return F::eVertex;
        case S::eFragment:
            return F::eFragment;
        case S::eCompute:
            return F::eCompute;
        case S::eGeometry:
            return F::eGeometry;
        case S::eTessellationControl:
            return F::eTessellationControl;
        case S::eTessellationEvaluation:
            return F::eTessellationEvaluation;
    }
    return F::eNone;
}

}  // namespace

namespace vne::sc {

ReflectResult SpirvCrossReflector::reflect(const std::vector<uint32_t>& spirv,
                                           ShaderStage stage,
                                           const std::vector<CrossTarget>& targets) {
    ReflectResult result;

    if (spirv.empty()) {
        result.code = ResultCode::eReflectionFailed;
        result.error = "SpirvCrossReflector: empty SPIR-V";
        VNE_LOG_ERROR << result.error;
        return result;
    }

    const bool has_msl = std::find(targets.begin(), targets.end(), CrossTarget::eMSL) != targets.end();
    const bool has_webgpu = std::find(targets.begin(), targets.end(), CrossTarget::eWGSL) != targets.end();

    try {
        // Base compiler for Vulkan reflection
        spirv_cross::Compiler compiler(spirv.data(), spirv.size());

        // MSL compiler — only instantiated when eMSL is a requested target
        std::unique_ptr<spirv_cross::CompilerMSL> msl_compiler;
        if (has_msl) {
            msl_compiler = std::make_unique<spirv_cross::CompilerMSL>(spirv.data(), spirv.size());
            spirv_cross::CompilerMSL::Options msl_opts;
            msl_opts.platform = spirv_cross::CompilerMSL::Options::macOS;
            msl_compiler->set_msl_options(msl_opts);
            try {
                msl_compiler->compile();
            } catch (...) {
                VNE_LOG_WARN << "SpirvCrossReflector: MSL compile pass failed; using fallback slot formula";
            }
        }

        const auto resources = compiler.get_shader_resources();
        const ShaderStageFlags stage_flag = stageToFlag(stage);

        StageReflection& sr = result.reflection;
        sr.stage = stage;
        sr.workgroup_size = {1, 1, 1};

        auto& bindings = sr.bindings;

#define VNE_REFLECT_LIST(Type, list)                                                                                \
    for (const auto& res : resources.list) {                                                                        \
        appendBinding<ReflectedResourceType::Type>(compiler, msl_compiler.get(), has_webgpu, res, stage, bindings); \
    }

        VNE_REFLECT_LIST(eUniformBuffer, uniform_buffers)
        VNE_REFLECT_LIST(eStorageBuffer, storage_buffers)
        VNE_REFLECT_LIST(eSampledImage, separate_images)
        VNE_REFLECT_LIST(eStorageImage, storage_images)
        VNE_REFLECT_LIST(eSampler, separate_samplers)
        VNE_REFLECT_LIST(eCombinedImageSampler, sampled_images)

#undef VNE_REFLECT_LIST

        // Tag each binding with this stage
        for (auto& b : bindings) {
            b.stages = stage_flag;
        }

        // Push-constant block size
        for (const auto& pc : resources.push_constant_buffers) {
            try {
                const spirv_cross::SPIRType& ty = compiler.get_type(pc.base_type_id);
                sr.push_constant_size = static_cast<uint32_t>(compiler.get_declared_struct_size(ty));
            } catch (...) {
            }
        }

        // Workgroup size for compute shaders
        if (stage == ShaderStage::eCompute) {
            const auto wg = compiler.get_execution_mode_argument(spv::ExecutionModeLocalSize, 0);
            const auto wg_y = compiler.get_execution_mode_argument(spv::ExecutionModeLocalSize, 1);
            const auto wg_z = compiler.get_execution_mode_argument(spv::ExecutionModeLocalSize, 2);
            if (wg > 0 || wg_y > 0 || wg_z > 0) {
                sr.workgroup_size = {wg > 0 ? wg : 1u, wg_y > 0 ? wg_y : 1u, wg_z > 0 ? wg_z : 1u};
            }
        }

        VNE_LOG_DEBUG << "SpirvCrossReflector: " << bindings.size() << " binding(s) reflected for stage "
                      << static_cast<int>(stage);

        result.code = ResultCode::eSuccess;

    } catch (const std::exception& e) {
        result.code = ResultCode::eReflectionFailed;
        result.error = std::string("SpirvCrossReflector: ") + e.what();
        VNE_LOG_ERROR << result.error;
    }

    return result;
}

}  // namespace vne::sc
