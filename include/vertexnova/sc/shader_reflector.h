/**
 * @file shader_reflector.h
 * @brief Interface for SPIR-V reflection that produces a JSON binding description.
 *
 * Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License").
 */

#pragma once

#include "sc_types.h"
#include "sc_result.h"

#include <vector>
#include <cstdint>

namespace vne::sc {

/**
 * @brief Reflects a SPIR-V binary and serialises binding metadata as JSON.
 *
 * The resulting JSON string is stored in @ref StageArtifact::reflection_json
 * and describes all uniform buffers, images, samplers, and push constants
 * together with their backend-specific binding indices.
 */
class IShaderReflector {
public:
    virtual ~IShaderReflector() = default;

    /**
     * @brief Reflects @p spirv and returns a JSON binding description.
     * @param spirv  SPIR-V binary words.
     * @param stage  Pipeline stage the module belongs to.
     * @returns A @ref ReflectResult whose @c json is non-empty on success.
     */
    virtual ReflectResult reflectToJson(const std::vector<uint32_t>& spirv,
                                        ShaderStage stage) = 0;
};

}  // namespace vne::sc
