/* ---------------------------------------------------------------------
 * Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License")
 *
 * Author:    Ajeet Singh Yadav
 * Created:   May 2026
 * ----------------------------------------------------------------------
 */

#include "vertexnova/sc/shader_compiler_factory.h"
#include "vertexnova/sc/shader_pipeline_builder.h"
#include "vertexnova/sc/shader_frontend.h"
#include "vertexnova/sc/shader_cross_compiler.h"
#include "vertexnova/sc/shader_reflector.h"
#include "vertexnova/sc/shader_validator.h"

#include "frontend/glslang_frontend.h"
#include "frontend/dxc_frontend.h"
#include "frontend/slang_frontend.h"
#include "crosscompiler/spirvcross_cross_compiler.h"
#include "reflector/spirvcross_reflector.h"

#ifdef VNE_SC_SPIRVTOOLS_ENABLED
#include "validator/spirvtools_validator.h"
#else
#include "validator/noop_validator.h"
#endif

#include <memory>

namespace vne::sc {

std::shared_ptr<IShaderFrontEnd> ShaderCompilerFactory::createFrontEnd(SourceLang lang) {
    switch (lang) {
        case SourceLang::eGLSL:
            return std::make_shared<GlslangFrontEnd>();
        case SourceLang::eHLSL:
            return std::make_shared<DxcFrontEnd>();
        case SourceLang::eSlang:
            return std::make_shared<SlangFrontEnd>();
        default:
            return std::make_shared<GlslangFrontEnd>();
    }
}

std::shared_ptr<IShaderCrossCompiler> ShaderCompilerFactory::createCrossCompiler() {
    return std::make_shared<SpirvCrossCrossCompiler>();
}

std::shared_ptr<IShaderReflector> ShaderCompilerFactory::createReflector() {
    return std::make_shared<SpirvCrossReflector>();
}

std::shared_ptr<IShaderValidator> ShaderCompilerFactory::createValidator() {
#ifdef VNE_SC_SPIRVTOOLS_ENABLED
    return std::make_shared<SpirvToolsValidator>();
#else
    return std::make_shared<NoopValidator>();
#endif
}

std::shared_ptr<IShaderPipelineBuilder>
ShaderCompilerFactory::createPipelineBuilder(SourceLang lang) {
    return std::make_shared<ShaderPipelineBuilder>(
        createFrontEnd(lang),
        createCrossCompiler(),
        createReflector(),
        createValidator());
}

}  // namespace vne::sc
