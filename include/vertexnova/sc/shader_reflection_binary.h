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
 *
 * @c ProgramReflection blobs (@c reflection.bin) use format version 3.0.0
 * (@c 0x00030000). Deserialization requires an exact version match; regenerate
 * stale bundles. Version 3 stores the WebGPU bind-group fields on each binding
 * (@c storage_format_hint, @c storage_access_hint, @c multisampled,
 * @c depth_texture, @c dynamic_offset).
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
