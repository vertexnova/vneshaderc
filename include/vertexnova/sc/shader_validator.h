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
 * @file shader_validator.h
 * @brief Interface for SPIR-V binary validators.
 */

#include "sc_types.h"
#include "sc_result.h"

#include <vector>
#include <cstdint>

namespace vne::sc {

/**
 * @brief Validates a SPIR-V binary against the Vulkan rules.
 *
 * A no-op implementation is used when the backing validation library is not
 * compiled in — it always returns @ref ResultCode::eSuccess.
 */
class IShaderValidator {
   public:
    virtual ~IShaderValidator() = default;

    /**
     * @brief Returns @c true when real validation is active.
     *
     * Returns @c false for the no-op implementation.
     */
    virtual bool isAvailable() const noexcept = 0;

    /**
     * @brief Validates @p spirv.
     * @returns A @ref ValidationResult whose @c ok() is @c true when the binary is valid.
     */
    virtual ValidationResult validate(const std::vector<uint32_t>& spirv) = 0;
};

}  // namespace vne::sc
