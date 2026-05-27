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

#include "spirvtools_validator.h"

#ifdef VNE_SC_SPIRVTOOLS_ENABLED
#include <spirv-tools/libspirv.hpp>
#endif

namespace vne::sc {

bool SpirvToolsValidator::isAvailable() const noexcept {
#ifdef VNE_SC_SPIRVTOOLS_ENABLED
    return true;
#else
    return false;
#endif
}

ValidationResult SpirvToolsValidator::validate(const std::vector<uint32_t>& spirv) {
    ValidationResult result;

#ifndef VNE_SC_SPIRVTOOLS_ENABLED
    (void)spirv;
    result.code = ResultCode::eUnavailable;
    result.error = "SpirvToolsValidator: SPIRV-Tools not compiled in (VNE_SC_SPIRVTOOLS_ENABLED not set)";
    return result;
#else
    if (spirv.empty()) {
        result.code = ResultCode::eValidationFailed;
        result.error = "SpirvToolsValidator: empty SPIR-V";
        return result;
    }

    spvtools::SpirvTools tools(SPV_ENV_VULKAN_1_0);
    std::string diag;
    tools.SetMessageConsumer([&diag](spv_message_level_t, const char*, const spv_position_t&, const char* message) {
        if (!diag.empty())
            diag += "\n";
        diag += message;
    });

    const bool ok = tools.Validate(spirv.data(), spirv.size());
    if (ok) {
        result.code = ResultCode::eSuccess;
    } else {
        result.code = ResultCode::eValidationFailed;
        result.error = diag.empty() ? "SpirvToolsValidator: validation failed (no diagnostics from SPIRV-Tools)" : diag;
    }
    return result;
#endif
}

}  // namespace vne::sc
