/* ---------------------------------------------------------------------
 * Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License")
 * ----------------------------------------------------------------------
 */

#include "tint_cross_compiler.h"

#include "vertexnova/logging/logging.h"
#ifdef VNE_SC_TINT_ENABLED
#include "tint/tint.h"
#endif

#include <sstream>

namespace {
CREATE_VNE_LOGGER_CATEGORY("vne.sc.tint");
}  // namespace
namespace vne::sc {

TintCrossCompiler::TintCrossCompiler() {
#ifdef VNE_SC_TINT_ENABLED
    tint::Initialize();
    initialized_ = true;
#endif
}

TintCrossCompiler::~TintCrossCompiler() = default;

bool TintCrossCompiler::isAvailable() const noexcept {
#ifdef VNE_SC_TINT_ENABLED
    return initialized_;
#else
    return false;
#endif
}

CrossCompileResult TintCrossCompiler::crossCompile(const CrossCompileRequest& req) {
    CrossCompileResult result;

    if (req.target != CrossTarget::eWGSL) {
        result.code = ResultCode::eInvalidArgument;
        result.error = "TintCrossCompiler: only CrossTarget::eWGSL is supported";
        return result;
    }

    if (req.spirv.empty()) {
        result.code = ResultCode::eInvalidArgument;
        result.error = "TintCrossCompiler: empty SPIR-V input";
        return result;
    }

#ifndef VNE_SC_TINT_ENABLED
    result.code = ResultCode::eUnavailable;
    result.error = "TintCrossCompiler: Tint not compiled in (enable -DVNE_SC_TINT=ON)";
    return result;
#else
    tint::wgsl::writer::Options wgsl_options;
    auto tint_result = tint::SpirvToWgsl(req.spirv, wgsl_options);

    if (tint_result != tint::Success) {
        result.code = ResultCode::eCrossCompileFailed;
        result.error = "TintCrossCompiler: SPIR-V to WGSL failed";
        VNE_LOG_ERROR << result.error;
        return result;
    }

    result.source = tint_result.Get();
    result.code = ResultCode::eSuccess;
    // Keep the known-safe default unless explicit entry-point extraction is implemented.
    result.entry_point = "main";

    VNE_LOG_DEBUG << "TintCrossCompiler: produced " << result.source.size() << " bytes of WGSL";
    return result;
#endif
}

}  // namespace vne::sc
