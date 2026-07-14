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

#include "gpu_layout_tools.h"
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
 * @brief One compile variant of a @ref ShaderPipelineSpec - the same stage
 * files recompiled with a different set of preprocessor @c #define macros,
 * producing a separate, sibling-named output bundle.
 *
 * An empty @c name denotes the base/default variant (no defines, no bundle
 * suffix) - every manifest without an explicit "variants" array behaves as
 * if it had exactly one variant with an empty name, preserving prior output
 * layout and naming for every existing manifest.
 */
struct ShaderVariantSpec {
    std::string name;                  ///< "" = base variant (no bundle suffix).
    std::vector<ShaderMacro> defines;  ///< Injected into every stage's CompileRequest::macros.
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
    bool validate_layout = false;    ///< Opt-in: compare reflected UBO sizes against layout registries.
    std::string emit_binding_decls;  ///< Relative to spec dir; writes layout(...) GLSL after build.
    std::vector<std::string> emit_binding_decls_skip;          ///< Blacklist counterpart to emit_binding_decls_include.
    std::vector<std::string> emit_binding_decls_include;       ///< Whitelist block/resource names for emit.
    std::vector<std::string> emit_binding_decls_compose;       ///< #include paths prepended to emitted file.
    std::string emit_bindings_stage = "fragment";              ///< fragment | vertex | all
    std::string layout_registry;                               ///< Deprecated: use layout_registries.
    std::vector<std::string> layout_registries;                ///< Relative to spec dir; merged for validate_layout.
    std::vector<ExpectedUniformBufferLayout> uniform_buffers;  ///< Inline pass-specific UBO sizes.
    MetalBindingLayout metal_layout;                           ///< Optional override; defaults match vnerhi.
    std::vector<ShaderVariantSpec> variants;                   ///< Optional; empty = single implicit base variant.
    std::vector<std::string> errors;                           ///< Non-fatal parse warnings / errors.

    /// Converts this spec into a @ref PipelineBuildDesc for @ref IShaderPipelineBuilder.
    /// @param spec_dir Directory of the spec file; used to resolve include_paths.
    [[nodiscard]] PipelineBuildDesc toBuildDesc(const std::filesystem::path& spec_dir = {}) const;
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
