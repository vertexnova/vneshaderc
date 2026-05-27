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

#include "shader_cross_compiler.h"
#include "spirvcross_cross_compiler.h"
#ifdef VNE_SC_TINT_ENABLED
#include "tint_cross_compiler.h"
#endif

#include "vertexnova/logging/logging.h"

CREATE_VNE_LOGGER_CATEGORY("vne.sc.crosscompiler")

namespace vne::sc {

ShaderCrossCompiler::ShaderCrossCompiler()
    : spirvcross_(std::make_unique<SpirvCrossCrossCompiler>())
#ifdef VNE_SC_TINT_ENABLED
    , tint_(std::make_unique<TintCrossCompiler>())
#endif
{
}

ShaderCrossCompiler::~ShaderCrossCompiler() = default;

bool ShaderCrossCompiler::isAvailable() const noexcept {
    return spirvcross_ && spirvcross_->isAvailable();
}

IShaderCrossCompiler* ShaderCrossCompiler::backendFor(CrossTarget target) const noexcept {
    if (target == CrossTarget::eWGSL) {
#ifdef VNE_SC_TINT_ENABLED
        if (tint_ && tint_->isAvailable()) {
            return tint_.get();
        }
#endif
        return nullptr;
    }
    return spirvcross_ ? spirvcross_.get() : nullptr;
}

CrossCompileResult ShaderCrossCompiler::crossCompile(const CrossCompileRequest& req) {
    IShaderCrossCompiler* const backend = backendFor(req.target);
    if (!backend) {
        CrossCompileResult result;
        result.code = ResultCode::eUnavailable;
        if (req.target == CrossTarget::eWGSL) {
            result.error = "ShaderCrossCompiler: WGSL requires Tint (build with -DVNE_SC_TINT=ON)";
        } else {
            result.error = "ShaderCrossCompiler: SPIRV-Cross backend not available";
        }
        VNE_LOG_ERROR << result.error;
        return result;
    }

    if (req.target == CrossTarget::eWGSL) {
        VNE_LOG_DEBUG << "ShaderCrossCompiler: routing WGSL to Tint";
    } else {
        VNE_LOG_DEBUG << "ShaderCrossCompiler: routing target " << static_cast<int>(req.target)
                      << " to SPIRV-Cross";
    }
    return backend->crossCompile(req);
}

}  // namespace vne::sc
