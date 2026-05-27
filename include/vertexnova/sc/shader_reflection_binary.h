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
 * @file shader_reflection_binary.h
 * @brief Binary serialization for @ref StageReflection and @ref ProgramReflection.
 */

#include "sc_types.h"

#include <string>
#include <vector>

namespace vne::sc {

/// Serializes a @ref StageReflection to a binary blob.
std::string serializeStageReflection(const StageReflection& reflection);

/// Deserializes a @ref StageReflection from a binary blob.
bool deserializeStageReflection(const std::string& data, StageReflection& out);

/// Serializes a @ref ProgramReflection (all stages) to a binary blob.
std::string serializeProgramReflection(const ProgramReflection& reflection);

/// Deserializes a @ref ProgramReflection from a binary blob.
bool deserializeProgramReflection(const std::string& data, ProgramReflection& out);

}  // namespace vne::sc
