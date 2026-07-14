#pragma once
/* ---------------------------------------------------------------------
 * Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License")
 *
 * Author:    Ajeet Singh Yadav
 * Created:   July 2026
 *
 * Autodoc:   yes
 * ----------------------------------------------------------------------
 */

/**
 * @file metal_binding_allocator.h
 * @brief Assigns MSL `[[buffer]]` / `[[texture]]` / `[[sampler]]` indices.
 * @ingroup vne::sc
 *
 * @par Why this exists
 * Metal's index spaces are small and fixed - **31 buffers**, **16 samplers** - while Vulkan's
 * `(set, binding)` space is effectively unbounded. The previous mapping *flattened* the two:
 *
 *     msl_buffer = buffer_base + set * flatten_stride + binding      // base 16, stride 32
 *
 * That overflows for set >= 1. This allocator packs indices **densely** instead: resources are
 * enumerated in ascending `(set, binding)` order and handed consecutive Metal indices.
 *
 * @par Program-wide invariant
 * Vertex and fragment (and any other stages in a program) must agree on Metal slots for the same
 * logical `(set, binding)`. Build one allocator from the **union** of all stage SPIR-V modules and
 * reuse it for every stage's reflect + MSL cross-compile.
 *
 * @note This header intentionally does **not** include SPIRV-Cross. Cross-compiler/reflector
 *       translation units that call @ref applyMslResourceBindings include SPIRV-Cross themselves.
 */

#include "vertexnova/sc/sc_types.h"

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace spirv_cross {
class Compiler;
class CompilerMSL;
}  // namespace spirv_cross

namespace vne::sc {

/** @brief Hard limits of Metal's argument tables. Exceeding one is an error, not a warning. */
struct MetalIndexLimits {
    //! vnerhi binds push constants at index 30, so 29 is the last usable descriptor buffer.
    static constexpr std::uint32_t kMaxBufferIndex = 29;
    static constexpr std::uint32_t kMaxSamplerIndex = 15;   //!< Metal allows 16 samplers.
    static constexpr std::uint32_t kMaxTextureIndex = 127;  //!< Metal 2+ allows 128 textures.
};

/**
 * @brief Dense, deterministic `(set, binding)` -> Metal index assignment.
 *
 * Prefer @ref fromProgram for multi-stage pipelines so all stages share one map. Stage-local
 * construction from a single @c Compiler remains available for unit tests / legacy paths.
 */
class MetalBindingAllocator {
   public:
    using Key = std::pair<std::uint32_t, std::uint32_t>;  //!< (set, binding)

    MetalBindingAllocator(const spirv_cross::Compiler& compiler, const MetalBindingLayout& layout);

    /**
     * @brief Program-wide map from the union of every stage's resources.
     *
     * Empty SPIR-V modules are skipped. Deterministic: sorted by `(set, binding)` across the union.
     */
    [[nodiscard]] static MetalBindingAllocator fromProgram(const std::vector<std::vector<std::uint32_t>>& stage_spirv,
                                                           const MetalBindingLayout& layout);

    [[nodiscard]] std::uint32_t buffer(std::uint32_t set, std::uint32_t binding) const;
    [[nodiscard]] std::uint32_t texture(std::uint32_t set, std::uint32_t binding) const;
    [[nodiscard]] std::uint32_t sampler(std::uint32_t set, std::uint32_t binding) const;

    /**
     * @brief Empty when the allocation fits Metal's tables; otherwise names what overflowed.
     */
    [[nodiscard]] std::string overflowError() const;

    /**
     * @brief Stable fingerprint of the assigned maps (for artifact cache keys).
     *
     * Two programs with the same logical resource union produce the same fingerprint.
     */
    [[nodiscard]] std::uint64_t fingerprint() const noexcept;

    [[nodiscard]] const MetalBindingLayout& layout() const noexcept { return layout_; }

   private:
    using IndexMap = std::map<Key, std::uint32_t>;

    MetalBindingAllocator() = default;

    void assignFromResourceLists(const std::vector<Key>& buffers,
                                 const std::vector<Key>& textures,
                                 const std::vector<Key>& combined_images,
                                 const std::vector<Key>& separate_samplers);

    IndexMap buffer_index_;
    IndexMap texture_index_;
    IndexMap sampler_index_;
    MetalBindingLayout layout_{};
};

/**
 * @brief Serializable view of a program-wide Metal binding map (alias for cache / request plumbing).
 *
 * Owned by @ref ShaderPipelineBuilder for the duration of a build; pointed to from
 * @ref CrossCompileRequest and reflection calls. Vulkan/WebGPU ignore this entirely.
 */
using MetalProgramBindingSignature = MetalBindingAllocator;

/**
 * @brief Push @p alloc's indices into @p compiler as explicit @c MSLResourceBinding%s.
 *
 * Both the cross-compiler and the reflector call this before @c compile() so
 * @c get_automatic_msl_resource_binding() returns the same indices in both.
 *
 * Defined in the SPIRV-Cross translation unit; callers must include SPIRV-Cross headers.
 * @p stage is a @c spv::ExecutionModel value passed as @c std::uint32_t to keep this header
 * free of SPIR-V headers.
 */
void applyMslResourceBindings(spirv_cross::CompilerMSL& compiler,
                              const MetalBindingAllocator& alloc,
                              std::uint32_t stage);

}  // namespace vne::sc
