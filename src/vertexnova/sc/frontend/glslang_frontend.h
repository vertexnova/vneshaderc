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
 * @file glslang_frontend.h
 * @brief glslang-based GLSL → SPIR-V compiler front-end.
 */

#include "vertexnova/sc/shader_frontend.h"

namespace vne::sc {

/**
 * @brief Compiles GLSL source to SPIR-V using glslang.
 *
 * @c glslang::InitializeProcess() / @c glslang::FinalizeProcess() use a
 * process-wide reference count so multiple instances can coexist safely.
 */
class GlslangFrontEnd final : public IShaderFrontEnd {
   public:
    GlslangFrontEnd();
    ~GlslangFrontEnd() override;

    bool isAvailable() const noexcept override;
    CompileResult compile(const CompileRequest& req) override;

   private:
    bool initialized_ = false;
};

}  // namespace vne::sc
