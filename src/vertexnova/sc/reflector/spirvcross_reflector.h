/**
 * @file spirvcross_reflector.h
 * @brief SPIRV-Cross based SPIR-V reflector that produces a JSON binding description.
 *
 * Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License").
 */

#pragma once

#include "vertexnova/sc/shader_reflector.h"

namespace vne::sc {

/**
 * @brief Reflects a SPIR-V binary using SPIRV-Cross and serialises the
 *        result as a JSON binding description stored in @ref StageArtifact::reflection_json.
 *
 * The JSON structure mirrors the format consumed by the engine's shader loader,
 * and includes Metal and WebGPU binding slot overrides for each resource.
 */
class SpirvCrossReflector final : public IShaderReflector {
public:
    ReflectResult reflectToJson(const std::vector<uint32_t>& spirv,
                                ShaderStage stage) override;
};

}  // namespace vne::sc
