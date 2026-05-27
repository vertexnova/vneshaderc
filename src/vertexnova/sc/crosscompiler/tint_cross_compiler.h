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
 * @file tint_cross_compiler.h
 * @brief SPIR-V → WGSL cross-compiler using Dawn Tint.
 */

#include "vertexnova/sc/shader_cross_compiler.h"

namespace vne::sc {

/**
 * @brief Translates SPIR-V to WGSL via @c tint::SpirvToWgsl when @c VNE_SC_TINT_ENABLED is defined.
 */
class TintCrossCompiler final : public IShaderCrossCompiler {
   public:
    TintCrossCompiler();
    ~TintCrossCompiler() override;

    bool isAvailable() const noexcept override;
    CrossCompileResult crossCompile(const CrossCompileRequest& req) override;

   private:
    bool initialized_ = false;
};

}  // namespace vne::sc
