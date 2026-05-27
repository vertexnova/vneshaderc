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
 * @file noop_validator.h
 * @brief No-op SPIR-V validator that always reports success.
 * Used when the SPIRV-Tools validation library is not compiled in.
 */

#include "vertexnova/sc/shader_validator.h"

namespace vne::sc {

/**
 * @brief Pass-through validator — accepts every SPIR-V binary without inspection.
 *
 * @ref isAvailable returns @c true so the pipeline builder does not skip the
 * validation step; it simply always succeeds.
 */
class NoopValidator final : public IShaderValidator {
   public:
    bool isAvailable() const noexcept override { return true; }

    ValidationResult validate(const std::vector<uint32_t>& /*spirv*/) override {
        ValidationResult r;
        r.code = ResultCode::eSuccess;
        return r;
    }
};

}  // namespace vne::sc
