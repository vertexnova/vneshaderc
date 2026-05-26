/**
 * @file dxc_frontend.h
 * @brief DXC front-end: compiles HLSL source to SPIR-V.
 *
 * @note SFI — stub implementation.  @ref isAvailable returns @c false and
 *       @ref compile returns @ref ResultCode::eUnavailable until DXC is wired in.
 *
 * Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License").
 */

#pragma once

#include "vertexnova/sc/shader_frontend.h"

namespace vne::sc {

/**
 * @brief HLSL → SPIR-V front-end using the DirectX Shader Compiler (DXC).
 *
 * @note SFI — stub.  All methods are no-ops until the DXC back-end is implemented.
 */
class DxcFrontEnd final : public IShaderFrontEnd {
public:
    bool isAvailable() const noexcept override { return false; }

    CompileResult compile(const CompileRequest& req) override {
        (void)req;
        CompileResult r;
        r.code = ResultCode::eUnavailable;
        r.errors.push_back("DxcFrontEnd: HLSL compilation is not yet implemented");
        return r;
    }
};

}  // namespace vne::sc
