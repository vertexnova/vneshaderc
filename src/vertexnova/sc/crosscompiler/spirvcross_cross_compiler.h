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
 * @file spirvcross_cross_compiler.h
 * @brief SPIRV-Cross based cross-compiler: SPIR-V → MSL / GLSL / GLSL ES.
 */

#include "vertexnova/sc/shader_cross_compiler.h"

namespace vne::sc {

/**
 * @brief Translates SPIR-V to Metal Shading Language, GLSL, or GLSL ES
 *        using the SPIRV-Cross library.
 *
 * WGSL output is not supported by SPIRV-Cross; requests for @ref CrossTarget::eWGSL
 * return @ref ResultCode::eUnavailable.
 */
class SpirvCrossCrossCompiler final : public IShaderCrossCompiler {
   public:
    bool isAvailable() const noexcept override;
    CrossCompileResult crossCompile(const CrossCompileRequest& req) override;

    /**
     * @brief Post-processes a fragment MSL string to align its @c stage_in parameter
     *        with the vertex shader's output struct.
     *
     * SPIRV-Cross can omit the @c stage_in struct from fragment shaders that do not
     * explicitly use vertex attributes.  This function inserts the missing parameter
     * so that vertex and fragment stages link correctly on Metal.
     *
     * @param frag_msl  Fragment MSL source, modified in place.
     * @param vert_msl  Vertex MSL source used as the reference for the output struct.
     */
    static void fixMSLFragmentSignature(std::string& frag_msl, const std::string& vert_msl);

   private:
    CrossCompileResult toMSL(const CrossCompileRequest& req);
    CrossCompileResult toGLSL(const CrossCompileRequest& req, bool es);
    CrossCompileResult toWGSL(const CrossCompileRequest& req);
};

}  // namespace vne::sc
