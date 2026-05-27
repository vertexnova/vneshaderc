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
 * @file shader_cross_compiler.h
 * @brief Interface for cross-compilers that translate SPIR-V to a target shading language.
 */

#include "sc_types.h"
#include "sc_result.h"

namespace vne::sc {

/**
 * @brief Translates a SPIR-V binary into a target shading language (MSL, GLSL, …).
 */
class IShaderCrossCompiler {
   public:
    virtual ~IShaderCrossCompiler() = default;

    /**
     * @brief Returns @c true when this cross-compiler can process requests.
     */
    virtual bool isAvailable() const noexcept = 0;

    /**
     * @brief Translates the SPIR-V in @p req to the requested target language.
     * @returns A @ref CrossCompileResult whose @c ok() is @c true on success.
     */
    virtual CrossCompileResult crossCompile(const CrossCompileRequest& req) = 0;
};

}  // namespace vne::sc
