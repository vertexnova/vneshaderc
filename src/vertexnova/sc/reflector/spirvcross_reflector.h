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
 * @file spirvcross_reflector.h
 * @brief SPIRV-Cross based SPIR-V reflector producing typed binding metadata.
 */

#include "vertexnova/sc/shader_reflector.h"

namespace vne::sc {

/**
 * @brief Reflects a SPIR-V binary using SPIRV-Cross.
 *
 * Populates a @ref StageReflection with typed binding metadata, including
 * Metal buffer/texture/sampler slot assignments and WebGPU group/binding indices
 * derived from SPIRV-Cross's automatic MSL resource binding API.
 */
class SpirvCrossReflector final : public IShaderReflector {
   public:
    ReflectResult reflect(const std::vector<uint32_t>& spirv,
                          ShaderStage stage,
                          const std::vector<CrossTarget>& targets = {}) override;
};

}  // namespace vne::sc
