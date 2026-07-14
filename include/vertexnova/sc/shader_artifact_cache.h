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
 * @file shader_artifact_cache.h
 * @brief File-based cache for compiled shader stage artifacts.
 * Each artifact is stored in a separate file under a configured directory.
 * Cache keys are 64-bit FNV-1a hashes of the compile request parameters
 * formatted as 16-character hex strings.
 */

#include "sc_result.h"
#include "shader_artifact.h"

#include <optional>
#include <string>
#include <vector>

namespace vne::sc {

/**
 * @brief Persistent, file-based cache for @ref StageArtifact objects.
 *
 * The cache directory is created on first use.  Each entry is a binary
 * @c .vnca file whose name is the 16-character hex cache key.
 */
class ShaderArtifactCache {
   public:
    /**
     * @brief Constructs a cache rooted at @p cache_dir.
     *
     * Pass an empty string to create a disabled (no-op) cache instance.
     */
    explicit ShaderArtifactCache(std::string cache_dir);
    ~ShaderArtifactCache() = default;

    /**
     * @brief Program-wide fingerprint from compile inputs only (no SPIR-V).
     *
     * Hashes every stage's source/path/entry/stage/lang/macros plus
     * @p metal_layout. Use this for MSL dense program maps so cache keys are
     * available before front-end compile. Changing any stage invalidates all
     * stage keys that embed this fingerprint.
     */
    [[nodiscard]] static std::uint64_t makeProgramFingerprint(const std::vector<CompileRequest>& stages,
                                                              const MetalBindingLayout& metal_layout = {});

    /**
     * @brief Derives a deterministic cache key from a compile request.
     *
     * The key encodes source text, file path, entry point, stage, language,
     * optimisation level, preprocessor macros, cross-compilation targets,
     * @ref MetalBindingLayout, and an optional program-wide fingerprint when
     * @c CrossTarget::eMSL is among @p targets. Prefer
     * @ref makeProgramFingerprint for that value so lookup can run before compile.
     */
    static std::string makeKey(const CompileRequest& req,
                               const std::vector<CrossTarget>& targets,
                               const MetalBindingLayout& metal_layout = {},
                               std::uint64_t metal_program_fingerprint = 0);

    /**
     * @brief Looks up a cached artifact by key.
     * @returns The deserialized @ref StageArtifact, or @c std::nullopt on miss.
     */
    [[nodiscard]] std::optional<StageArtifact> lookup(const std::string& key) const;

    /**
     * @brief Serialises and stores @p artifact under @p key.
     */
    void store(const std::string& key, const StageArtifact& artifact);

    /**
     * @brief Removes all @c .vnca files from the cache directory.
     */
    void clear();

   private:
    std::string cache_dir_;

    [[nodiscard]] std::string artifactPath(const std::string& key) const;
};

}  // namespace vne::sc
