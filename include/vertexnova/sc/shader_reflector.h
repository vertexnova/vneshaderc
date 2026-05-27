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
 * @file shader_reflector.h
 * @brief Interface for SPIR-V reflection producing typed binding metadata.
 */

#include "sc_types.h"
#include "sc_result.h"

#include <vector>
#include <cstdint>

namespace vne::sc {

/**
 * @brief Reflects a SPIR-V binary and returns typed binding metadata.
 *
 * The result is stored directly in @ref StageArtifact::reflection as a
 * @ref StageReflection, which includes all uniform buffers, images, samplers,
 * push constants, and backend-specific binding slot assignments.
 */
class IShaderReflector {
   public:
    virtual ~IShaderReflector() = default;

    /**
     * @brief Reflects @p spirv and returns typed stage binding metadata.
     * @param spirv  SPIR-V binary words.
     * @param stage  Pipeline stage the module belongs to.
     * @returns A @ref ReflectResult whose @c reflection is populated on success.
     */
    virtual ReflectResult reflect(const std::vector<uint32_t>& spirv,
                                  ShaderStage stage,
                                  const std::vector<CrossTarget>& targets = {}) = 0;
};

}  // namespace vne::sc
