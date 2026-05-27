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
 * @file shader_frontend.h
 * @brief Interface for compiler front-ends that translate source code to SPIR-V.
 */

#include "sc_types.h"
#include "sc_result.h"

namespace vne::sc {

/**
 * @brief Translates shader source code into a SPIR-V binary.
 *
 * Each concrete implementation supports one source language family.
 * Callers should check @ref isAvailable before calling @ref compile.
 */
class IShaderFrontEnd {
   public:
    virtual ~IShaderFrontEnd() = default;

    /**
     * @brief Returns @c true when this front-end is ready to accept compile requests.
     *
     * A front-end may be unavailable if its backing library was not compiled in
     * or failed to initialise.
     */
    virtual bool isAvailable() const noexcept = 0;

    /**
     * @brief Compiles shader source into SPIR-V.
     * @param req  Compilation parameters including source, stage, and options.
     * @returns A @ref CompileResult whose @c ok() is @c true on success.
     */
    virtual CompileResult compile(const CompileRequest& req) = 0;
};

}  // namespace vne::sc
