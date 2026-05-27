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
 * @file shader_pipeline_spec.h
 * @brief JSON spec describing an offline shader pipeline compile job.
 */

#include "shader_pipeline_builder.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace vne::sc {

/**
 * @brief One stage entry in a @ref ShaderPipelineSpec.
 */
struct ShaderStageSpec {
    ShaderStage stage = ShaderStage::eVertex;
    std::string file;
    std::string entry_point = "main";
};

/**
 * @brief Parsed pipeline spec (typically loaded from a `.pipeline.json` file).
 */
struct ShaderPipelineSpec {
    std::string name;
    SourceLang source_lang = SourceLang::eGLSL;
    std::vector<ShaderStageSpec> stages;
    std::vector<CrossTarget> targets;
    std::vector<std::string> include_paths;  ///< Resolved relative to the spec file's directory.
    bool validate = true;
    std::vector<std::string> errors;  ///< Non-fatal parse warnings / errors.

    /// Converts this spec into a @ref PipelineBuildDesc for @ref IShaderPipelineBuilder.
    /// @param spec_dir Directory of the spec file; used to resolve include_paths.
    PipelineBuildDesc toBuildDesc(const std::filesystem::path& spec_dir = {}) const;
};

/**
 * @brief Parses a @ref ShaderPipelineSpec from a JSON string.
 * @returns Parsed spec, or @c std::nullopt on fatal parse errors.
 */
std::optional<ShaderPipelineSpec> parseShaderPipelineSpecJson(const std::string& json);

/**
 * @brief Loads a @ref ShaderPipelineSpec from a file path.
 */
std::optional<ShaderPipelineSpec> loadShaderPipelineSpec(const std::string& path);

}  // namespace vne::sc
