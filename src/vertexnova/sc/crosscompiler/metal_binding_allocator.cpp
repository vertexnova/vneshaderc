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

#include "vertexnova/sc/metal_binding_allocator.h"

#include <spirv_cross.hpp>
#include <spirv_msl.hpp>

#include <algorithm>
#include <cassert>
#include <vector>

namespace vne::sc {

namespace {

using Key = MetalBindingAllocator::Key;

constexpr std::uint64_t kFnv1a64Offset = 14695981039346656037ULL;
constexpr std::uint64_t kFnv1a64Prime = 1099511628211ULL;

void hashMix(std::uint64_t& h, std::uint64_t v) noexcept {
    h ^= v;
    h *= kFnv1a64Prime;
}

void collect(const spirv_cross::Compiler& compiler,
             const spirv_cross::SmallVector<spirv_cross::Resource>& resources,
             std::vector<Key>& out) {
    for (const auto& r : resources) {
        const std::uint32_t set = compiler.has_decoration(r.id, spv::DecorationDescriptorSet)
                                      ? compiler.get_decoration(r.id, spv::DecorationDescriptorSet)
                                      : 0U;
        const std::uint32_t binding = compiler.has_decoration(r.id, spv::DecorationBinding)
                                          ? compiler.get_decoration(r.id, spv::DecorationBinding)
                                          : 0U;
        out.emplace_back(set, binding);
    }
}

void collectAll(const spirv_cross::Compiler& compiler,
                std::vector<Key>& buffers,
                std::vector<Key>& textures,
                std::vector<Key>& combined,
                std::vector<Key>& separate) {
    const spirv_cross::ShaderResources res = compiler.get_shader_resources();
    collect(compiler, res.uniform_buffers, buffers);
    collect(compiler, res.storage_buffers, buffers);
    collect(compiler, res.sampled_images, textures);
    collect(compiler, res.separate_images, textures);
    collect(compiler, res.storage_images, textures);
    collect(compiler, res.sampled_images, combined);
    collect(compiler, res.separate_samplers, separate);
}

void uniqueSorted(std::vector<Key>& keys) {
    std::sort(keys.begin(), keys.end());
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
}

void assign(std::vector<Key>& keys, const std::uint32_t base, std::map<Key, std::uint32_t>& out) {
    uniqueSorted(keys);
    std::uint32_t next = base;
    for (const Key& k : keys) {
        out.emplace(k, next++);
    }
}

[[nodiscard]] std::uint32_t lookup(const std::map<Key, std::uint32_t>& m,
                                   const std::uint32_t set,
                                   const std::uint32_t binding) {
    const auto it = m.find(Key{set, binding});
    if (it == m.end()) {
        assert(false && "MetalBindingAllocator: missing (set, binding) in allocation map");
        return 0U;
    }
    return it->second;
}

[[nodiscard]] std::uint32_t highest(const std::map<Key, std::uint32_t>& m) {
    if (m.empty()) {
        return 0U;
    }
    std::uint32_t hi = 0U;
    for (const auto& [_, idx] : m) {
        hi = std::max(hi, idx);
    }
    return hi;
}

void hashMap(std::uint64_t& h, const std::map<Key, std::uint32_t>& m) {
    for (const auto& [key, idx] : m) {
        hashMix(h, key.first);
        hashMix(h, key.second);
        hashMix(h, idx);
    }
}

}  // namespace

void MetalBindingAllocator::assignFromResourceLists(const std::vector<Key>& buffers_in,
                                                    const std::vector<Key>& textures_in,
                                                    const std::vector<Key>& combined_in,
                                                    const std::vector<Key>& separate_in) {
    buffer_index_.clear();
    texture_index_.clear();
    sampler_index_.clear();

    std::vector<Key> buffers = buffers_in;
    assign(buffers, layout_.buffer_base, buffer_index_);

    std::vector<Key> textures = textures_in;
    assign(textures, 0U, texture_index_);

    // Combined image-samplers: pin sampler index to the texture index so
    // fixCombinedSamplerIndices cannot desync the pair.
    std::vector<Key> combined = combined_in;
    uniqueSorted(combined);

    std::vector<std::uint32_t> reserved;
    reserved.reserve(combined.size());
    for (const Key& k : combined) {
        const std::uint32_t idx = lookup(texture_index_, k.first, k.second);
        sampler_index_.emplace(k, idx);
        reserved.push_back(idx);
    }
    std::sort(reserved.begin(), reserved.end());

    std::vector<Key> separate = separate_in;
    uniqueSorted(separate);

    std::uint32_t next = 0U;
    for (const Key& k : separate) {
        while (std::binary_search(reserved.begin(), reserved.end(), next)) {
            ++next;
        }
        sampler_index_.emplace(k, next++);
    }
}

MetalBindingAllocator::MetalBindingAllocator(const spirv_cross::Compiler& compiler, const MetalBindingLayout& layout)
    : layout_(layout) {
    std::vector<Key> buffers;
    std::vector<Key> textures;
    std::vector<Key> combined;
    std::vector<Key> separate;
    collectAll(compiler, buffers, textures, combined, separate);
    assignFromResourceLists(buffers, textures, combined, separate);
}

MetalBindingAllocator MetalBindingAllocator::fromProgram(const std::vector<std::vector<std::uint32_t>>& stage_spirv,
                                                         const MetalBindingLayout& layout) {
    MetalBindingAllocator alloc;
    alloc.layout_ = layout;

    std::vector<Key> buffers;
    std::vector<Key> textures;
    std::vector<Key> combined;
    std::vector<Key> separate;

    for (const auto& spirv : stage_spirv) {
        if (spirv.empty()) {
            continue;
        }
        spirv_cross::Compiler compiler(spirv.data(), spirv.size());
        collectAll(compiler, buffers, textures, combined, separate);
    }

    alloc.assignFromResourceLists(buffers, textures, combined, separate);
    return alloc;
}

std::uint32_t MetalBindingAllocator::buffer(const std::uint32_t set, const std::uint32_t binding) const {
    return lookup(buffer_index_, set, binding);
}

std::uint32_t MetalBindingAllocator::texture(const std::uint32_t set, const std::uint32_t binding) const {
    return lookup(texture_index_, set, binding);
}

std::uint32_t MetalBindingAllocator::sampler(const std::uint32_t set, const std::uint32_t binding) const {
    return lookup(sampler_index_, set, binding);
}

std::string MetalBindingAllocator::overflowError() const {
    if (const std::uint32_t hi = highest(buffer_index_); hi > MetalIndexLimits::kMaxBufferIndex) {
        return "Metal buffer index " + std::to_string(hi) + " exceeds the maximum of "
               + std::to_string(MetalIndexLimits::kMaxBufferIndex) + " (" + std::to_string(buffer_index_.size())
               + " buffers in this program). Metal has 31 buffer slots: index 30 is reserved for push "
                 "constants, and the slots below buffer_base for vertex buffers.";
    }
    if (const std::uint32_t hi = highest(sampler_index_); hi > MetalIndexLimits::kMaxSamplerIndex) {
        return "Metal sampler index " + std::to_string(hi) + " exceeds the maximum of "
               + std::to_string(MetalIndexLimits::kMaxSamplerIndex) + " (" + std::to_string(sampler_index_.size())
               + " samplers in this program; Metal allows 16).";
    }
    if (const std::uint32_t hi = highest(texture_index_); hi > MetalIndexLimits::kMaxTextureIndex) {
        return "Metal texture index " + std::to_string(hi) + " exceeds the maximum of "
               + std::to_string(MetalIndexLimits::kMaxTextureIndex) + ".";
    }
    return {};
}

std::uint64_t MetalBindingAllocator::fingerprint() const noexcept {
    std::uint64_t h = kFnv1a64Offset;
    hashMix(h, layout_.buffer_base);
    hashMix(h, layout_.flatten_stride);
    hashMap(h, buffer_index_);
    hashMap(h, texture_index_);
    hashMap(h, sampler_index_);
    return h;
}

void applyMslResourceBindings(spirv_cross::CompilerMSL& compiler,
                              const MetalBindingAllocator& alloc,
                              const std::uint32_t stage) {
    const auto exec_model = static_cast<spv::ExecutionModel>(stage);
    const spirv_cross::ShaderResources res = compiler.get_shader_resources();

    const auto decorations = [&compiler](const spirv_cross::Resource& r) {
        const std::uint32_t set = compiler.has_decoration(r.id, spv::DecorationDescriptorSet)
                                      ? compiler.get_decoration(r.id, spv::DecorationDescriptorSet)
                                      : 0U;
        const std::uint32_t binding = compiler.has_decoration(r.id, spv::DecorationBinding)
                                          ? compiler.get_decoration(r.id, spv::DecorationBinding)
                                          : 0U;
        return std::pair{set, binding};
    };

    const auto bindBuffers = [&](const spirv_cross::SmallVector<spirv_cross::Resource>& list) {
        for (const auto& r : list) {
            const auto [set, binding] = decorations(r);
            spirv_cross::MSLResourceBinding mb{};
            mb.stage = exec_model;
            mb.desc_set = set;
            mb.binding = binding;
            mb.msl_buffer = alloc.buffer(set, binding);
            compiler.add_msl_resource_binding(mb);
        }
    };
    const auto bindTextures = [&](const spirv_cross::SmallVector<spirv_cross::Resource>& list) {
        for (const auto& r : list) {
            const auto [set, binding] = decorations(r);
            spirv_cross::MSLResourceBinding mb{};
            mb.stage = exec_model;
            mb.desc_set = set;
            mb.binding = binding;
            mb.msl_texture = alloc.texture(set, binding);
            compiler.add_msl_resource_binding(mb);
        }
    };
    const auto bindSamplers = [&](const spirv_cross::SmallVector<spirv_cross::Resource>& list) {
        for (const auto& r : list) {
            const auto [set, binding] = decorations(r);
            spirv_cross::MSLResourceBinding mb{};
            mb.stage = exec_model;
            mb.desc_set = set;
            mb.binding = binding;
            mb.msl_sampler = alloc.sampler(set, binding);
            compiler.add_msl_resource_binding(mb);
        }
    };

    for (const auto& r : res.sampled_images) {
        const auto [set, binding] = decorations(r);
        spirv_cross::MSLResourceBinding mb{};
        mb.stage = exec_model;
        mb.desc_set = set;
        mb.binding = binding;
        mb.msl_texture = alloc.texture(set, binding);
        mb.msl_sampler = alloc.sampler(set, binding);
        compiler.add_msl_resource_binding(mb);
    }

    bindBuffers(res.uniform_buffers);
    bindBuffers(res.storage_buffers);
    bindTextures(res.separate_images);
    bindTextures(res.storage_images);
    bindSamplers(res.separate_samplers);
}

}  // namespace vne::sc
