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
 * @file shader_artifact.h
 * @brief Output types produced by the shader compilation pipeline.
 * A @ref ShaderArtifact bundles every compiler output for a complete shader
 * program: SPIR-V binaries, cross-compiled source texts, and typed binding
 * reflection data for each pipeline stage.
 */

#include "sc_types.h"

#include <string>
#include <vector>

namespace vne::sc {

/**
 * @brief Cross-compiled source text for a single target shading language.
 */
struct CrossCompiledSource {
    CrossTarget target = CrossTarget::eMSL;  ///< Target language this source was compiled to.
    std::string source;                      ///< Cross-compiled source text.
    std::string entry_point;                 ///< Entry-point name as it appears in the output source.
};

/**
 * @brief All compiler outputs for a single shader stage.
 */
struct StageArtifact {
    ShaderStage stage = ShaderStage::eVertex;
    std::string entry_point;
    std::vector<uint32_t> spirv;                      ///< SPIR-V binary words.
    StageReflection reflection;                       ///< Typed binding and push-constant metadata.
    std::vector<CrossCompiledSource> cross_compiled;  ///< One entry per requested target.

    /**
     * @brief Finds cross-compiled output for the given target.
     * @returns Pointer to the matching entry, or @c nullptr if not present.
     */
    const CrossCompiledSource* findCrossCompiled(CrossTarget target) const noexcept {
        for (const auto& cc : cross_compiled) {
            if (cc.target == target) {
                return &cc;
            }
        }
        return nullptr;
    }
};

/**
 * @brief Bundle of per-stage artifacts representing a complete compiled shader program.
 */
struct ShaderArtifact {
    std::string name;
    SourceLang source_lang = SourceLang::eGLSL;
    std::vector<StageArtifact> stages;

    /**
     * @brief Finds the artifact for the given pipeline stage.
     * @returns Pointer to the matching entry, or @c nullptr if not present.
     */
    const StageArtifact* findStage(ShaderStage stage) const noexcept {
        for (const auto& s : stages) {
            if (s.stage == stage) {
                return &s;
            }
        }
        return nullptr;
    }

    /**
     * @brief Assembles a @ref ProgramReflection from the per-stage reflection data.
     *
     * The returned object owns its own copy of the data and can be passed to
     * the engine's shader loader without keeping the artifact alive.
     */
    ProgramReflection assembleReflection() const noexcept {
        ProgramReflection pr;
        pr.stages.reserve(stages.size());
        for (const auto& s : stages) {
            pr.stages.push_back(s.reflection);
        }
        return pr;
    }

    /**
     * @brief Returns @c true when all stages have non-empty SPIR-V.
     */
    bool isValid() const noexcept {
        if (stages.empty()) {
            return false;
        }
        for (const auto& s : stages) {
            if (s.spirv.empty()) {
                return false;
            }
        }
        return true;
    }
};

}  // namespace vne::sc
