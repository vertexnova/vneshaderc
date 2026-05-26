/* ---------------------------------------------------------------------
 * Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License")
 *
 * Author:    Ajeet Singh Yadav
 * Created:   May 2026
 * ----------------------------------------------------------------------
 */

#include "vertexnova/sc/shader_pipeline_builder.h"
#include "vertexnova/sc/shader_frontend.h"
#include "vertexnova/sc/shader_cross_compiler.h"
#include "vertexnova/sc/shader_reflector.h"
#include "vertexnova/sc/shader_validator.h"
#include "vertexnova/sc/shader_artifact_cache.h"

#include <memory>
#include <utility>

namespace vne::sc {

ShaderPipelineBuilder::ShaderPipelineBuilder(
    std::shared_ptr<IShaderFrontEnd>      front_end,
    std::shared_ptr<IShaderCrossCompiler> cross_compiler,
    std::shared_ptr<IShaderReflector>     reflector,
    std::shared_ptr<IShaderValidator>     validator)
    : front_end_(std::move(front_end))
    , cross_compiler_(std::move(cross_compiler))
    , reflector_(std::move(reflector))
    , validator_(std::move(validator)) {}

PipelineBuildResult ShaderPipelineBuilder::build(const PipelineBuildDesc& desc) {
    PipelineBuildResult result;
    result.artifact.name        = desc.name;
    result.artifact.source_lang = desc.stages.empty() ? SourceLang::eGLSL : desc.stages[0].lang;

    if (!front_end_ || !front_end_->isAvailable()) {
        result.code  = ResultCode::eUnavailable;
        result.error = "ShaderPipelineBuilder: front-end not available";
        return result;
    }

    std::unique_ptr<ShaderArtifactCache> cache;
    if (desc.use_cache && !desc.cache_dir.empty()) {
        cache = std::make_unique<ShaderArtifactCache>(desc.cache_dir);
    }

    for (const auto& req : desc.stages) {
        // ── Cache lookup ────────────────────────────────────────────────────
        const std::string key = cache
            ? ShaderArtifactCache::makeKey(req, desc.targets)
            : std::string{};

        if (cache) {
            auto cached = cache->lookup(key);
            if (cached.has_value()) {
                result.artifact.stages.push_back(std::move(*cached));
                continue;
            }
        }

        StageArtifact stage_artifact;
        stage_artifact.stage       = req.stage;
        stage_artifact.entry_point = req.entry_point;

        // ── 1. Compile → SPIR-V ─���──────────────────────────────────────────
        CompileResult cr = front_end_->compile(req);
        if (!cr.ok()) {
            result.code = cr.code;
            result.error = "ShaderPipelineBuilder: compile failed";
            for (const auto& e : cr.errors) result.error += "\n  " + e;
            return result;
        }
        stage_artifact.spirv = std::move(cr.spirv);

        // ���─ 2. Validate ────────────────────────────────────────────────────
        if (desc.validate && validator_ && validator_->isAvailable()) {
            ValidationResult vr = validator_->validate(stage_artifact.spirv);
            if (!vr.ok()) {
                result.code  = ResultCode::eValidationFailed;
                result.error = "ShaderPipelineBuilder: SPIR-V validation failed: " + vr.error;
                return result;
            }
        }

        // ── 3. Reflect → JSON ──────────────────────────────────────────────
        if (reflector_) {
            ReflectResult rr = reflector_->reflectToJson(stage_artifact.spirv, req.stage);
            if (rr.ok()) {
                stage_artifact.reflection_json = std::move(rr.json);
            }
            // Non-fatal — reflection failure does not block compilation
        }

        // ── 4. Cross-compile for each target ──────────────────────────────
        if (cross_compiler_) {
            for (CrossTarget target : desc.targets) {
                CrossCompileRequest ccr;
                ccr.spirv   = stage_artifact.spirv;
                ccr.target  = target;
                ccr.stage   = req.stage;

                CrossCompileResult ccres = cross_compiler_->crossCompile(ccr);
                if (ccres.ok()) {
                    CrossCompiledSource cc;
                    cc.target      = target;
                    cc.source      = std::move(ccres.source);
                    cc.entry_point = std::move(ccres.entry_point);
                    stage_artifact.cross_compiled.push_back(std::move(cc));
                }
                // Non-fatal — a missing target is noted but doesn't fail the build
            }
        }

        // ── 5. Cache store ────────────────────────────────────────────────
        if (cache && !key.empty()) {
            cache->store(key, stage_artifact);
        }

        result.artifact.stages.push_back(std::move(stage_artifact));
    }

    result.code = ResultCode::eSuccess;
    return result;
}

}  // namespace vne::sc
