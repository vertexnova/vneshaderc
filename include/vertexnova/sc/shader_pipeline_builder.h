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
 * @file shader_pipeline_builder.h
 * @brief Orchestrates the full offline shader compilation pipeline.
 */

#include "sc_types.h"
#include "sc_result.h"
#include "shader_artifact.h"

#include <memory>
#include <string>
#include <vector>

namespace vne::sc {

class IShaderFrontEnd;
class IShaderCrossCompiler;
class IShaderReflector;
class IShaderValidator;

// ─────────────────────────────────────────────────────────────────────────────
// Pipeline descriptor
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Describes a complete pipeline build job — all stages and targets.
 */
struct PipelineBuildDesc {
    std::string name;
    std::vector<CompileRequest> stages;  ///< One entry per shader stage.
    std::vector<CrossTarget> targets;    ///< Cross-compilation targets (e.g. MSL, WGSL).
    bool validate = true;                ///< Run SPIR-V validation after compilation.
    bool use_cache = true;               ///< Enable the file-based artifact cache.
    std::string cache_dir;               ///< Cache root directory; empty disables caching.
};

/**
 * @brief Result returned by @ref IShaderPipelineBuilder::build.
 */
struct PipelineBuildResult {
    ResultCode code = ResultCode::eCompileFailed;
    ShaderArtifact artifact;
    std::string error;

    bool ok() const noexcept { return succeeded(code); }
};

// ─────────────────────────────────────────────────────────────────────────────
// Pipeline builder interface
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Drives the full offline compilation sequence:
 *        source → SPIR-V → validate → reflect → cross-compile → @ref ShaderArtifact.
 *
 * The pipeline is:
 *  -# Cache lookup — return early on hit.
 *  -# @ref IShaderFrontEnd::compile      — source → SPIR-V.
 *  -# @ref IShaderValidator::validate    — optional SPIR-V validation.
 *  -# @ref IShaderReflector::reflect — SPIR-V → typed @ref StageReflection.
 *  -# @ref IShaderCrossCompiler::crossCompile — SPIR-V → target source (one per target).
 *  -# Cache store.
 */
class IShaderPipelineBuilder {
   public:
    virtual ~IShaderPipelineBuilder() = default;

    /**
     * @brief Executes the full compilation pipeline for all stages in @p desc.
     */
    virtual PipelineBuildResult build(const PipelineBuildDesc& desc) = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
// Concrete implementation
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Default @ref IShaderPipelineBuilder returned by @ref ShaderCompilerFactory.
 *
 * All compiler components are injected through the constructor and may be
 * replaced for testing.
 */
class ShaderPipelineBuilder : public IShaderPipelineBuilder {
   public:
    ShaderPipelineBuilder(std::shared_ptr<IShaderFrontEnd> front_end,
                          std::shared_ptr<IShaderCrossCompiler> cross_compiler,
                          std::shared_ptr<IShaderReflector> reflector,
                          std::shared_ptr<IShaderValidator> validator);

    PipelineBuildResult build(const PipelineBuildDesc& desc) override;

   private:
    std::shared_ptr<IShaderFrontEnd> front_end_;
    std::shared_ptr<IShaderCrossCompiler> cross_compiler_;
    std::shared_ptr<IShaderReflector> reflector_;
    std::shared_ptr<IShaderValidator> validator_;
};

}  // namespace vne::sc
