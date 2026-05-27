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
 * @file slang_frontend.h
 * @brief Slang front-end: compiles Slang source to SPIR-V.
 * @note SFI — stub implementation.  @ref isAvailable returns @c false and
 */

#include "vertexnova/sc/shader_frontend.h"

namespace vne::sc {

/**
 * @brief Slang → SPIR-V front-end using the Slang shader compiler.
 *
 * @note SFI — stub.  All methods are no-ops until the Slang back-end is implemented.
 */
class SlangFrontEnd final : public IShaderFrontEnd {
   public:
    bool isAvailable() const noexcept override { return false; }

    CompileResult compile(const CompileRequest& req) override {
        (void)req;
        CompileResult r;
        r.code = ResultCode::eUnavailable;
        r.errors.push_back("SlangFrontEnd: Slang compilation is not yet implemented");
        return r;
    }
};

}  // namespace vne::sc
