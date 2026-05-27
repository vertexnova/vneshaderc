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
 * @file shader_bundle.h
 * @brief Writes and reads offline @c .vneshader bundles for vnerhi consumption.
 */

#include "shader_artifact.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace vne::sc {

/**
 * @brief Per-stage file names inside a bundle directory.
 */
struct BundleStageFiles {
    ShaderStage stage = ShaderStage::eVertex;
    std::string entry_point;  ///< SPIR-V / Vulkan entry point
    std::string spirv_file;
    std::string msl_file;
    std::string msl_entry_point;  ///< MSL entry (SPIRV-Cross may rename main→main0)
    std::string wgsl_file;
    std::string wgsl_entry_point;  ///< WGSL entry point
};

/**
 * @brief Binary bundle header (vnerhi loads this; no JSON parser required at runtime).
 */
struct ShaderBundleHeader {
    std::string name;
    SourceLang source_lang = SourceLang::eGLSL;
    std::vector<BundleStageFiles> stages;
};

/**
 * @brief Writes a @c .vneshader directory from a compiled @ref ShaderArtifact.
 *
 * Creates:
 * - @c bundle.header  — binary index for vnerhi
 * - @c reflection.bin — @ref ProgramReflection
 * - @c manifest.json  — human-readable index (when JSON enabled)
 * - Per-stage @c .spv / @c .msl / @c .wgsl files
 */
bool writeShaderBundle(const ShaderArtifact& artifact, const std::filesystem::path& bundle_dir);

/**
 * @brief Reads the binary bundle header from a @c .vneshader directory.
 */
std::optional<ShaderBundleHeader> readShaderBundleHeader(const std::filesystem::path& bundle_dir);

/**
 * @brief Reads @c reflection.bin from a bundle directory.
 */
std::optional<ProgramReflection> readShaderBundleReflection(const std::filesystem::path& bundle_dir);

}  // namespace vne::sc
