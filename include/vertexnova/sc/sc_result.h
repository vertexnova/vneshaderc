/**
 * @file sc_result.h
 * @brief Result codes and result structs for the vnesc shader compiler pipeline.
 *
 * Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License").
 */

#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace vne::sc {

// ─────────────────────────────────────────────────────────────────────────────
// Result code
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Status code returned by every compiler pipeline operation.
 *
 * Non-negative values indicate success (possibly with warnings).
 * Negative values indicate failure.
 */
enum class ResultCode : int32_t {
    eSuccess            =  0, ///< Operation completed without errors.
    eCompileWarnings    =  1, ///< Compilation succeeded but generated warnings.
    eCompileFailed      = -1, ///< Source code could not be compiled.
    eCrossCompileFailed = -2, ///< SPIR-V → target cross-compilation failed.
    eReflectionFailed   = -3, ///< SPIR-V reflection could not be completed.
    eValidationFailed   = -4, ///< SPIR-V validation failed.
    eFileNotFound       = -5, ///< Source file does not exist.
    eUnavailable        = -6, ///< The requested front-end or feature is not compiled in.
    eInvalidArgument    = -7, ///< A required argument is missing or malformed.
    eCacheMiss          = -8, ///< No cached artifact was found for the given key.
};

/// @returns @c true when @p code represents a successful outcome.
inline bool succeeded(ResultCode code) noexcept {
    return static_cast<int32_t>(code) >= 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Result structs
// ─────────────────────────────────────────────────────────────────────────────

/// Result of a source-to-SPIR-V compilation.
struct CompileResult {
    ResultCode                code     = ResultCode::eCompileFailed;
    std::vector<uint32_t>     spirv;    ///< SPIR-V words; non-empty on success.
    std::vector<std::string>  errors;
    std::vector<std::string>  warnings;

    /// @returns @c true when the compilation produced valid SPIR-V.
    bool ok() const noexcept { return succeeded(code); }
};

/// Result of a SPIR-V → shading-language cross-compilation.
struct CrossCompileResult {
    ResultCode  code        = ResultCode::eCrossCompileFailed;
    std::string source;      ///< Cross-compiled source text (MSL, GLSL, …).
    std::string entry_point; ///< Actual entry-point name in the output source.
    std::string error;

    bool ok() const noexcept { return succeeded(code); }
};

/// Result of a SPIR-V validation check.
struct ValidationResult {
    ResultCode  code  = ResultCode::eValidationFailed;
    std::string error;

    bool ok() const noexcept { return succeeded(code); }
};

/// Result of a SPIR-V reflection pass.
struct ReflectResult {
    ResultCode  code  = ResultCode::eReflectionFailed;
    std::string json;  ///< Serialised stage reflection as JSON; empty on failure.
    std::string error;

    bool ok() const noexcept { return succeeded(code); }
};

}  // namespace vne::sc
