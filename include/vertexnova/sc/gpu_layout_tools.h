#pragma once
/* ---------------------------------------------------------------------
 * Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License")
 * ---------------------------------------------------------------------- */

/**
 * @file gpu_layout_tools.h
 * @brief Binding-declaration emission and layout validation for shared _gpu.h pipelines.
 */

#include "shader_artifact.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace vne::sc {

struct ExpectedUniformBufferLayout {
    std::string block_name;
    uint32_t total_size = 0;
};

struct GpuLayoutRegistry {
    std::vector<ExpectedUniformBufferLayout> uniform_buffers;
};

struct EmitBindingDeclOptions {
    std::vector<std::string> skip_blocks;
    std::vector<std::string> include_blocks;    ///< Non-empty = whitelist by block/resource name.
    std::vector<std::string> compose_includes;  ///< Paths written as #include lines before emitted blocks.
    std::string bindings_stage = "fragment";    ///< fragment | vertex | all
};

/// Parse a layout registry JSON file (uniform_buffers[].name / .size).
[[nodiscard]] bool loadGpuLayoutRegistry(const std::filesystem::path& path, GpuLayoutRegistry& out, std::string& error);

/// Merge multiple registry files plus inline entries. Fails on duplicate block names.
[[nodiscard]] bool mergeGpuLayoutRegistries(const std::vector<std::filesystem::path>& registry_paths,
                                            const std::vector<ExpectedUniformBufferLayout>& inline_buffers,
                                            GpuLayoutRegistry& out,
                                            std::string& error);

/// Compare reflected UBO sizes against @p registry. Returns false and sets @p error on mismatch.
[[nodiscard]] bool validateGpuLayouts(const ShaderArtifact& artifact,
                                      const GpuLayoutRegistry& registry,
                                      std::string& error);

/// Emit composed GLSL: compose #includes + reflected layout(...) declarations.
[[nodiscard]] std::string buildBindingDeclsGlsl(const ShaderArtifact& artifact, const EmitBindingDeclOptions& options);

/// Legacy emit API (skip list only).
[[nodiscard]] std::string emitBindingDeclsGlsl(const ShaderArtifact& artifact,
                                               const std::vector<std::string>& skip_blocks);

/// Write @p contents to @p output_path (creates parent directories).
[[nodiscard]] bool writeBindingDeclsFile(const std::filesystem::path& output_path,
                                         const std::string& contents,
                                         std::string& error);

}  // namespace vne::sc
