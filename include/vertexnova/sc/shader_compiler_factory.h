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
 * @file shader_compiler_factory.h
 * @brief Factory that assembles the compiler pipeline for a given source language.
 */

#include "sc_types.h"

#include <memory>

namespace vne::sc {

class IShaderFrontEnd;
class IShaderCrossCompiler;
class IShaderReflector;
class IShaderValidator;
class IShaderPipelineBuilder;

/**
 * @brief Creates and wires together concrete compiler components.
 *
 * All methods are static.  The typical entry point is @ref createPipelineBuilder,
 * which returns a fully-configured @ref IShaderPipelineBuilder ready to accept
 * @ref PipelineBuildDesc requests.
 *
 * @code{.cpp}
 * auto builder = vne::sc::ShaderCompilerFactory::createPipelineBuilder(vne::sc::SourceLang::eGLSL);
 * auto result  = builder->build(desc);
 * @endcode
 */
class ShaderCompilerFactory {
   public:
    /// Creates a front-end for the given source language.
    static std::shared_ptr<IShaderFrontEnd> createFrontEnd(SourceLang lang);

    /// Creates the SPIR-V → target cross-compiler (dispatches MSL/GLSL to SPIRV-Cross, WGSL to Tint).
    static std::shared_ptr<IShaderCrossCompiler> createCrossCompiler();

    /// Creates the SPIRV-Cross cross-compiler only.
    static std::shared_ptr<IShaderCrossCompiler> createSpirvCrossCrossCompiler();

    /// Creates the Tint WGSL cross-compiler, or @c nullptr when Tint is not built in.
    static std::shared_ptr<IShaderCrossCompiler> createTintCrossCompiler();

    /// Creates the SPIR-V reflector.
    static std::shared_ptr<IShaderReflector> createReflector();

    /// Creates the SPIR-V validator (no-op when the validation library is absent).
    static std::shared_ptr<IShaderValidator> createValidator();

    /**
     * @brief Convenience factory: assembles a fully-wired pipeline builder.
     * @param lang  Source language of the shaders the builder will process.
     */
    static std::shared_ptr<IShaderPipelineBuilder> createPipelineBuilder(SourceLang lang);
};

}  // namespace vne::sc
