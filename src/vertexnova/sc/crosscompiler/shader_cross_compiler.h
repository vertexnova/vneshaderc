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
 * @brief Concrete cross-compiler: SPIRV-Cross for MSL/GLSL/HLSL, Tint for WGSL.
 */

#include "vertexnova/sc/shader_cross_compiler.h"

#include <memory>

namespace vne::sc {

class SpirvCrossCrossCompiler;
class TintCrossCompiler;

/**
 * @brief Routes SPIR-V cross-compilation to the appropriate backend.
 *
 * WGSL requests go to Tint when it is available; all other targets (MSL, GLSL,
 * GLSL ES, HLSL) are handled by SPIRV-Cross.  When Tint is not compiled in,
 * WGSL falls back to SPIRV-Cross (which returns @ref ResultCode::eUnavailable
 * until native WGSL output is promoted out of experimental status).
 *
 * This class owns both backend instances and is created by
 * @ref ShaderCompilerFactory::createCrossCompiler.
 */
class ShaderCrossCompiler final : public IShaderCrossCompiler {
   public:
    ShaderCrossCompiler();
    ~ShaderCrossCompiler() override;

    bool isAvailable() const noexcept override;
    CrossCompileResult crossCompile(const CrossCompileRequest& req) override;

   private:
    std::unique_ptr<SpirvCrossCrossCompiler> spirvcross_;
    std::unique_ptr<TintCrossCompiler>       tint_;  ///< nullptr when VNE_SC_TINT not built
};

}  // namespace vne::sc
