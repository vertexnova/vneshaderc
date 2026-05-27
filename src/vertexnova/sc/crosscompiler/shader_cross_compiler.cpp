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
#include "tint_cross_compiler.h"

#include "vertexnova/logging/logging.h"

CREATE_VNE_LOGGER_CATEGORY("vne.sc.crosscompiler")

namespace vne::sc {

ShaderCrossCompiler::ShaderCrossCompiler()
    : spirvcross_(std::make_unique<SpirvCrossCrossCompiler>())
#ifdef VNE_SC_TINT_ENABLED
    , tint_(std::make_unique<TintCrossCompiler>())
#endif
{}

ShaderCrossCompiler::~ShaderCrossCompiler() = default;

bool ShaderCrossCompiler::isAvailable() const noexcept {
    return spirvcross_ && spirvcross_->isAvailable();
}

CrossCompileResult ShaderCrossCompiler::crossCompile(const CrossCompileRequest& req) {
    if (req.target == CrossTarget::eWGSL) {
        if (tint_ && tint_->isAvailable()) {
            VNE_LOG_DEBUG << "ShaderCrossCompiler: routing WGSL to Tint";
            return tint_->crossCompile(req);
        }
        VNE_LOG_DEBUG << "ShaderCrossCompiler: Tint not available, routing WGSL to SPIRV-Cross";
        return spirvcross_->crossCompile(req);
    }

    return spirvcross_->crossCompile(req);
}

}  // namespace vne::sc
