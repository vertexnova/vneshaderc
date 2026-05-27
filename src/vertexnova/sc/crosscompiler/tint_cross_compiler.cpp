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

#include "tint_cross_compiler.h"

#include "vertexnova/logging/logging.h"
#ifdef VNE_SC_TINT_ENABLED
#include "src/tint/api/tint.h"
#include "src/tint/lang/spirv/reader/common/options.h"
#include "src/tint/lang/spirv/reader/reader.h"
#include "src/tint/lang/wgsl/writer/options.h"
#include "src/tint/lang/wgsl/writer/writer.h"
#endif

#include <regex>
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
    tint::spirv::reader::Options spv_options;
    tint::Program program = tint::spirv::reader::Read(req.spirv, spv_options);
    if (!program.IsValid()) {
        result.code = ResultCode::eCrossCompileFailed;
        result.error = "TintCrossCompiler: SPIR-V parse failed";
        VNE_LOG_ERROR << result.error;
        return result;
    }

    tint::wgsl::writer::Options wgsl_options;
    auto wgsl_result = tint::wgsl::writer::Generate(program, wgsl_options);
    if (wgsl_result != tint::Success) {
        result.code = ResultCode::eCrossCompileFailed;
        result.error = "TintCrossCompiler: SPIR-V to WGSL failed";
        VNE_LOG_ERROR << result.error;
        return result;
    }

    result.source = wgsl_result.Get().wgsl;
    result.code = ResultCode::eSuccess;
    result.entry_point = "main";

    // Scan emitted WGSL for actual @group/@binding assignments.
    // Tint may reassign bindings; callers use this table to patch reflection.
    {
        // Matches: @group(N) @binding(M) var<...> name  OR  @group(N) @binding(M) var name
        static const std::regex kBindingRe(R"(@group\((\d+)\)\s*@binding\((\d+)\)\s*var[^:]*?:\s*\S+\s+(\w+))"
                                           "|"
                                           R"(@group\((\d+)\)\s*@binding\((\d+)\)\s*var\s+(\w+))");
        std::sregex_iterator it(result.source.begin(), result.source.end(), kBindingRe);
        for (std::sregex_iterator end; it != end; ++it) {
            const std::smatch& m = *it;
            WgpuBindingRemap remap;
            if (m[1].matched) {
                remap.group = static_cast<uint32_t>(std::stoul(m[1].str()));
                remap.binding = static_cast<uint32_t>(std::stoul(m[2].str()));
                remap.name = m[3].str();
            } else {
                remap.group = static_cast<uint32_t>(std::stoul(m[4].str()));
                remap.binding = static_cast<uint32_t>(std::stoul(m[5].str()));
                remap.name = m[6].str();
            }
            if (!remap.name.empty()) {
                result.wgpu_binding_remap.push_back(std::move(remap));
            }
        }
    }

    VNE_LOG_DEBUG << "TintCrossCompiler: produced " << result.source.size() << " bytes of WGSL, "
                  << result.wgpu_binding_remap.size() << " binding(s) remapped";
    return result;
#endif
}

}  // namespace vne::sc
