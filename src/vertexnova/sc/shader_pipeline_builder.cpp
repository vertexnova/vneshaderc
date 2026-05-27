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
#include "vertexnova/sc/shader_frontend.h"
#include "vertexnova/sc/shader_cross_compiler.h"
#include "vertexnova/sc/shader_reflector.h"
#include "vertexnova/sc/shader_validator.h"
#include "vertexnova/sc/shader_artifact_cache.h"

#include "vertexnova/logging/logging.h"

#include <memory>
#include <utility>

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

    for (const auto& req : desc.stages) {
        // ── Cache lookup ────────────────────────────────────────────────────
        const std::string key = cache ? ShaderArtifactCache::makeKey(req, desc.targets) : std::string{};

        if (cache) {
            auto cached = cache->lookup(key);
            if (cached.has_value()) {
                VNE_LOG_DEBUG << "ShaderPipelineBuilder: cache hit for stage " << static_cast<int>(req.stage);
                result.artifact.stages.push_back(std::move(*cached));
                continue;
            }
        }

        StageArtifact stage_artifact;
        stage_artifact.stage = req.stage;
        stage_artifact.entry_point = req.entry_point;

        // ── 1. Compile → SPIR-V ───────────────────────────────────────────
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
        stage_artifact.spirv = std::move(cr.spirv);

        // ── 2. Validate ───────────────────────────────────────────────────
        if (desc.validate && (!validator_ || !validator_->isAvailable())) {
            result.code = ResultCode::eUnavailable;
            result.error = "ShaderPipelineBuilder: validator requested but not available";
            VNE_LOG_ERROR << result.error;
            return result;
        }
        if (desc.validate) {
            ValidationResult vr = validator_->validate(stage_artifact.spirv);
            if (!vr.ok()) {
                result.code = ResultCode::eValidationFailed;
                result.error = "ShaderPipelineBuilder: SPIR-V validation failed: " + vr.error;
                VNE_LOG_ERROR << result.error;
                return result;
            }
        }

        // ── 3. Reflect ────────────────────────────────────────────────────
        if (reflector_) {
            ReflectResult rr = reflector_->reflect(stage_artifact.spirv, req.stage, desc.targets);
            if (rr.ok()) {
                stage_artifact.reflection = std::move(rr.reflection);
            } else {
                VNE_LOG_WARN << "ShaderPipelineBuilder: reflection failed (non-fatal): " << rr.error;
            }
        }

        // ── 4. Cross-compile for each requested target ────────────────────
        if (!desc.targets.empty() && !cross_compiler_) {
            result.code = ResultCode::eUnavailable;
            result.error = "ShaderPipelineBuilder: cross-compiler not available";
            VNE_LOG_ERROR << result.error;
            return result;
        }
        if (cross_compiler_) {
            for (CrossTarget target : desc.targets) {
                CrossCompileRequest ccr;
                ccr.spirv = stage_artifact.spirv;
                ccr.target = target;
                ccr.stage = req.stage;

                CrossCompileResult ccres = cross_compiler_->crossCompile(ccr);
                if (ccres.ok()) {
                    // Apply WGSL binding remap to reflection so WebGpuResourceSlot matches emitted WGSL.
                    if (target == CrossTarget::eWGSL && !ccres.wgpu_binding_remap.empty()) {
                        for (auto& binding : stage_artifact.reflection.bindings) {
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
                    stage_artifact.cross_compiled.push_back(std::move(cc));
                } else {
                    result.code = ccres.code;
                    result.error = "ShaderPipelineBuilder: cross-compile to target "
                                   + std::to_string(static_cast<int>(target)) + " failed: " + ccres.error;
                    VNE_LOG_ERROR << result.error;
                    return result;
                }
            }
        }
        if (stage_artifact.cross_compiled.size() != desc.targets.size()) {
            result.code = ResultCode::eCompileFailed;
            result.error = "ShaderPipelineBuilder: failed to cross-compile one or more requested targets";
            VNE_LOG_ERROR << result.error;
            return result;
        }

        // ── 5. Cache store ─────────────────────────────────────────────────
        if (cache && !key.empty()) {
            cache->store(key, stage_artifact);
        }

        result.artifact.stages.push_back(std::move(stage_artifact));
    }

    VNE_LOG_INFO << "ShaderPipelineBuilder: build complete — " << result.artifact.stages.size() << " stage(s)";
    result.code = ResultCode::eSuccess;
    return result;
}

}  // namespace vne::sc
