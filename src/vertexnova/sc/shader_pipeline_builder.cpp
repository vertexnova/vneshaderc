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

#include "vertexnova/sc/shader_pipeline_builder.h"
#include "vertexnova/sc/metal_binding_allocator.h"
#include "vertexnova/sc/shader_frontend.h"
#include "vertexnova/sc/shader_cross_compiler.h"
#include "vertexnova/sc/shader_reflector.h"
#include "vertexnova/sc/shader_validator.h"
#include "vertexnova/sc/shader_artifact_cache.h"

#include "vertexnova/logging/logging.h"

#include <algorithm>
#include <memory>
#include <sstream>
#include <utility>
#include <vector>

CREATE_VNE_LOGGER_CATEGORY("vne.sc.pipeline")

namespace vne::sc {

ShaderPipelineBuilder::ShaderPipelineBuilder(std::shared_ptr<IShaderFrontEnd> front_end,
                                             std::shared_ptr<IShaderCrossCompiler> cross_compiler,
                                             std::shared_ptr<IShaderReflector> reflector,
                                             std::shared_ptr<IShaderValidator> validator)
    : front_end_(std::move(front_end))
    , cross_compiler_(std::move(cross_compiler))
    , reflector_(std::move(reflector))
    , validator_(std::move(validator)) {}

PipelineBuildResult ShaderPipelineBuilder::build(const PipelineBuildDesc& desc) {
    PipelineBuildResult result;
    if (desc.stages.empty()) {
        result.code = ResultCode::eCompileFailed;
        result.error = "ShaderPipelineBuilder: at least one stage is required";
        VNE_LOG_ERROR << result.error;
        return result;
    }
    result.artifact.name = desc.name;
    result.artifact.source_lang = desc.stages[0].lang;

    if (!front_end_ || !front_end_->isAvailable()) {
        result.code = ResultCode::eUnavailable;
        result.error = "ShaderPipelineBuilder: front-end not available";
        VNE_LOG_ERROR << result.error;
        return result;
    }

    VNE_LOG_INFO << "ShaderPipelineBuilder: building '" << desc.name << "' (" << desc.stages.size() << " stage(s))";

    std::unique_ptr<ShaderArtifactCache> cache;
    if (desc.use_cache && !desc.cache_dir.empty()) {
        cache = std::make_unique<ShaderArtifactCache>(desc.cache_dir);
    }

    const bool wants_msl = std::find(desc.targets.begin(), desc.targets.end(), CrossTarget::eMSL) != desc.targets.end();

    // ── Single pass, phase A: compile + validate every stage into memory ─────
    struct StageWork {
        CompileRequest req;
        StageArtifact artifact;
        bool from_cache = false;
    };
    std::vector<StageWork> work;
    work.reserve(desc.stages.size());

    for (const auto& req : desc.stages) {
        StageWork sw;
        sw.req = req;
        sw.artifact.stage = req.stage;
        sw.artifact.entry_point = req.entry_point;

        VNE_LOG_DEBUG << "ShaderPipelineBuilder: compiling stage " << static_cast<int>(req.stage);
        CompileResult cr = front_end_->compile(req);
        if (!cr.ok()) {
            result.code = cr.code;
            result.error = "ShaderPipelineBuilder: compile failed";
            for (const auto& e : cr.errors)
                result.error += "\n  " + e;
            VNE_LOG_ERROR << result.error;
            return result;
        }
        for (const auto& w : cr.warnings) {
            VNE_LOG_WARN << "ShaderPipelineBuilder: compile warning: " << w;
        }
        sw.artifact.spirv = std::move(cr.spirv);

        if (desc.validate && (!validator_ || !validator_->isAvailable())) {
            result.code = ResultCode::eUnavailable;
            result.error = "ShaderPipelineBuilder: validator requested but not available";
            VNE_LOG_ERROR << result.error;
            return result;
        }
        if (desc.validate) {
            ValidationResult vr = validator_->validate(sw.artifact.spirv);
            if (!vr.ok()) {
                result.code = ResultCode::eValidationFailed;
                result.error = "ShaderPipelineBuilder: SPIR-V validation failed: " + vr.error;
                VNE_LOG_ERROR << result.error;
                return result;
            }
        }

        work.push_back(std::move(sw));
    }

    // ── Program-link precompute (Metal only): one dense map for all stages ───
    std::unique_ptr<MetalBindingAllocator> metal_program_map;
    std::uint64_t metal_fingerprint = 0;
    if (wants_msl && desc.metal_dense_program_map) {
        std::vector<std::vector<std::uint32_t>> stage_spirv;
        stage_spirv.reserve(work.size());
        for (const auto& sw : work) {
            stage_spirv.push_back(sw.artifact.spirv);
        }
        metal_program_map =
            std::make_unique<MetalBindingAllocator>(MetalBindingAllocator::fromProgram(stage_spirv, desc.metal_layout));
        if (const std::string overflow = metal_program_map->overflowError(); !overflow.empty()) {
            result.code = ResultCode::eCrossCompileFailed;
            result.error = "ShaderPipelineBuilder: " + overflow;
            VNE_LOG_ERROR << result.error;
            return result;
        }
        metal_fingerprint = metal_program_map->fingerprint();
        {
            std::ostringstream oss;
            oss << "ShaderPipelineBuilder: Metal program signature fingerprint=0x" << std::hex << metal_fingerprint;
            VNE_LOG_DEBUG << oss.str();
        }
    } else if (wants_msl) {
        VNE_LOG_WARN << "ShaderPipelineBuilder: metal_dense_program_map=false — using stage-local Metal maps";
    }

    // ── Single pass, phase B: cache / reflect / cross-compile per stage ──────
    if (!desc.targets.empty() && !cross_compiler_) {
        result.code = ResultCode::eUnavailable;
        result.error = "ShaderPipelineBuilder: cross-compiler not available";
        VNE_LOG_ERROR << result.error;
        return result;
    }

    for (auto& sw : work) {
        const std::string key =
            cache ? ShaderArtifactCache::makeKey(sw.req, desc.targets, desc.metal_layout, metal_fingerprint)
                  : std::string{};

        if (cache) {
            auto cached = cache->lookup(key);
            if (cached.has_value()) {
                VNE_LOG_DEBUG << "ShaderPipelineBuilder: cache hit for stage " << static_cast<int>(sw.req.stage);
                // Keep freshly compiled SPIR-V if cache somehow lacked it; prefer cached full stage.
                result.artifact.stages.push_back(std::move(*cached));
                continue;
            }
        }

        // Reflect with shared Metal map (Vulkan/WebGPU set/binding unchanged).
        if (reflector_) {
            ReflectResult rr = reflector_->reflect(sw.artifact.spirv,
                                                   sw.req.stage,
                                                   desc.targets,
                                                   desc.metal_layout,
                                                   metal_program_map.get());
            if (rr.ok()) {
                sw.artifact.reflection = std::move(rr.reflection);
            } else {
                VNE_LOG_WARN << "ShaderPipelineBuilder: reflection failed (non-fatal): " << rr.error;
            }
        }

        if (cross_compiler_) {
            for (CrossTarget target : desc.targets) {
                CrossCompileRequest ccr;
                ccr.spirv = sw.artifact.spirv;
                ccr.target = target;
                ccr.stage = sw.req.stage;
                ccr.metal_layout = desc.metal_layout;
                ccr.metal_program_map = metal_program_map.get();

                CrossCompileResult ccres = cross_compiler_->crossCompile(ccr);
                if (ccres.ok()) {
                    if (target == CrossTarget::eWGSL && !ccres.wgpu_binding_remap.empty()) {
                        for (auto& binding : sw.artifact.reflection.bindings) {
                            for (const auto& remap : ccres.wgpu_binding_remap) {
                                if (binding.name == remap.name) {
                                    binding.slots.webgpu = WebGpuResourceSlot{remap.group, remap.binding};
                                    break;
                                }
                            }
                        }
                    }
                    CrossCompiledSource cc;
                    cc.target = target;
                    cc.source = std::move(ccres.source);
                    cc.entry_point = std::move(ccres.entry_point);
                    sw.artifact.cross_compiled.push_back(std::move(cc));
                } else {
                    if (target == CrossTarget::eWGSL) {
                        VNE_LOG_WARN << "ShaderPipelineBuilder: WGSL cross-compile failed (non-fatal): " << ccres.error;
                    } else {
                        result.code = ccres.code;
                        result.error = "ShaderPipelineBuilder: cross-compile to target "
                                       + std::to_string(static_cast<int>(target)) + " failed: " + ccres.error;
                        VNE_LOG_ERROR << result.error;
                        return result;
                    }
                }
            }
        }

        if (cache && !key.empty()) {
            cache->store(key, sw.artifact);
        }

        result.artifact.stages.push_back(std::move(sw.artifact));
    }

    VNE_LOG_INFO << "ShaderPipelineBuilder: build complete — " << result.artifact.stages.size() << " stage(s)";
    result.code = ResultCode::eSuccess;
    return result;
}

}  // namespace vne::sc
