/* ---------------------------------------------------------------------
 * Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License")
 *
 * Author:    Ajeet Singh Yadav
 * Created:   May 2026
 * ----------------------------------------------------------------------
 */

#include "spirvcross_cross_compiler.h"

#include "spirv_cross.hpp"
#include "spirv_glsl.hpp"
#include "spirv_msl.hpp"

#include <regex>
#include <sstream>
#include <unordered_set>

namespace {

// ── Metal binding constants (must match vnerhi MetalResourceBinder) ───────────
constexpr uint32_t kFlattenBinding        = 32u;
constexpr uint32_t kMetalUboBase          = 16u;

inline uint32_t metalBufferSlot(uint32_t set, uint32_t binding) noexcept {
    return kMetalUboBase + (set * kFlattenBinding + binding);
}
inline uint32_t metalTextureSlot(uint32_t set, uint32_t binding) noexcept {
    return set * kFlattenBinding + binding;
}
inline uint32_t metalSamplerSlot(uint32_t set, uint32_t binding) noexcept {
    return set * kFlattenBinding + binding;
}

// ── Sampler index post-processing ─────────────────────────────────────────────
// SPIRV-Cross doesn't always correctly remap sampler indices when splitting combined
// samplers. Fix by making each sampler's [[sampler(N)]] match its paired texture's
// [[texture(N)]] index.
void fixCombinedSamplerIndices(std::string& msl) {
    std::unordered_map<std::string, uint32_t> tex_indices;
    std::regex tex_re(R"(texture\w*<[^>]+>\s+(\w+)\s+\[\[texture\((\d+)\)\]\])");
    std::sregex_iterator it(msl.begin(), msl.end(), tex_re);
    for (std::sregex_iterator end; it != end; ++it) {
        tex_indices[(*it)[1].str()] = static_cast<uint32_t>(std::stoul((*it)[2].str()));
    }
    for (const auto& [name, idx] : tex_indices) {
        const std::string smplr_name = name + "Smplr";
        const std::string prefix = "sampler " + smplr_name + " [[sampler(";
        size_t pos = 0;
        while ((pos = msl.find(prefix, pos)) != std::string::npos) {
            size_t start = pos + prefix.size();
            size_t end   = msl.find(")]]", start);
            if (end == std::string::npos) break;
            msl.replace(start, end - start, std::to_string(idx));
            pos = end + 3;
        }
    }
}

// ── FixMSLFragmentSignature ────────────────────────────────────────────────────
// Ensure the fragment shader references a valid stage_in struct.
// Ports the hard-won FixMSLFragmentSignature logic from the old spirv_cross_compiler.cpp.
void fixMSLFragmentSignature(std::string& frag_msl, const std::string& vert_msl) {
    if (frag_msl.find("[[stage_in]]") != std::string::npos) {
        // stage_in already present — verify the referenced struct exists
        std::regex frag_fn_re(R"(fragment\s+(\w+)\s+(\w+)\s*\(([^)]*)\))");
        std::smatch frag_fn_m;
        if (std::regex_search(frag_msl, frag_fn_m, frag_fn_re)) {
            const std::string params = frag_fn_m[3].str();
            std::regex si_re(R"((\w+)\s+\w+\s*\[\[stage_in\]\])");
            std::smatch si_m;
            if (std::regex_search(params, si_m, si_re)) {
                const std::string si_type = si_m[1].str();
                if (frag_msl.find("struct " + si_type) != std::string::npos) {
                    return;  // struct exists — nothing to do
                }
                // Try to synthesize struct from vertex output
                std::regex vo_re(R"(struct\s+(\w+)\s*\{[^}]*\[\[position\]\])");
                std::smatch vo_m;
                if (std::regex_search(vert_msl, vo_m, vo_re)) {
                    const std::string vo_name = vo_m[1].str();
                    std::regex vs_re(R"(struct\s+)" + vo_name + R"(\s*\{[^}]*\})");
                    std::smatch vs_m;
                    if (std::regex_search(vert_msl, vs_m, vs_re)) {
                        std::string def = vs_m[0].str();
                        def = std::regex_replace(def,
                            std::regex(R"(struct\s+)" + vo_name + R"(\s*)"),
                            "struct " + si_type + " ");
                        if (def.back() != ';') def += ";";
                        std::regex first_re(R"(struct\s+\w+\s*\{)");
                        std::smatch first_m;
                        if (std::regex_search(frag_msl, first_m, first_re)) {
                            frag_msl.insert(static_cast<size_t>(first_m.position()), def + "\n\n");
                        } else {
                            frag_msl = def + "\n\n" + frag_msl;
                        }
                    }
                }
            }
        }
        return;
    }

    // No stage_in — try to inject vertex output struct + stage_in parameter.
    std::regex frag_fn_re(R"(fragment\s+(\w+)\s+(\w+)\s*\(([^)]*)\))");
    std::smatch frag_fn_m;
    if (!std::regex_search(frag_msl, frag_fn_m, frag_fn_re)) return;

    const std::string ret_type  = frag_fn_m[1].str();
    const std::string fn_name   = frag_fn_m[2].str();
    const std::string cur_params = frag_fn_m[3].str();

    std::regex vo_re(R"(struct\s+(\w+)\s*\{[^}]*\[\[position\]\])");
    std::smatch vo_m;
    if (!std::regex_search(vert_msl, vo_m, vo_re)) return;

    const std::string vo_name = vo_m[1].str();
    std::string vi_name = vo_name + "_vert_in";

    // Insert vertex output struct (renamed) if not already in fragment shader.
    if (frag_msl.find("[[position]]") == std::string::npos) {
        std::regex vs_re(R"(struct\s+)" + vo_name + R"(\s*\{[^}]*\})");
        std::smatch vs_m;
        if (std::regex_search(vert_msl, vs_m, vs_re)) {
            std::string def = vs_m[0].str();
            def = std::regex_replace(def,
                std::regex(R"(struct\s+)" + vo_name + R"(\s*)"),
                "struct " + vi_name + " ");
            if (def.back() != ';') def += ";";
            std::regex fs_re(R"(struct\s+\w+\s*\{)");
            std::smatch fs_m;
            if (std::regex_search(frag_msl, fs_m, fs_re)) {
                frag_msl.insert(static_cast<size_t>(fs_m.position()), def + "\n\n");
            }
        }
    } else {
        std::regex vif_re(R"(struct\s+(\w+)\s*\{[^}]*\[\[position\]\])");
        std::smatch vif_m;
        if (std::regex_search(frag_msl, vif_m, vif_re)) {
            vi_name = vif_m[1].str();
        }
    }

    const std::string new_params = cur_params.empty()
        ? vi_name + " vert_out [[stage_in]]"
        : vi_name + " vert_out [[stage_in]], " + cur_params;
    const std::string new_sig = "fragment " + ret_type + " " + fn_name + "(" + new_params + ")";
    frag_msl = std::regex_replace(frag_msl,
        std::regex(R"(fragment\s+)" + ret_type + R"(\s+)" + fn_name + R"(\s*\([^)]*\))"),
        new_sig, std::regex_constants::format_first_only);
}

}  // namespace

namespace vne::sc {

bool SpirvCrossCrossCompiler::isAvailable() const noexcept {
    return true;  // spirv-cross-core is always linked into vnesc_spirvcross
}

CrossCompileResult SpirvCrossCrossCompiler::crossCompile(const CrossCompileRequest& req) {
    if (req.spirv.empty()) {
        CrossCompileResult r;
        r.code  = ResultCode::eCrossCompileFailed;
        r.error = "SpirvCrossCrossCompiler: empty SPIR-V";
        return r;
    }
    switch (req.target) {
        case CrossTarget::eMSL:    return toMSL(req);
        case CrossTarget::eGLSL:   return toGLSL(req, false);
        case CrossTarget::eGLSLES: return toGLSL(req, true);
        case CrossTarget::eWGSL:   return toWGSL(req);
        default: {
            CrossCompileResult r;
            r.code  = ResultCode::eUnavailable;
            r.error = "SpirvCrossCrossCompiler: unsupported target";
            return r;
        }
    }
}

// ── MSL ───────────────────────────────────────────────────────────────────────
CrossCompileResult SpirvCrossCrossCompiler::toMSL(const CrossCompileRequest& req) {
    CrossCompileResult result;
    try {
        spirv_cross::CompilerMSL compiler(req.spirv.data(), req.spirv.size());

        spirv_cross::CompilerMSL::Options opts;
        opts.set_msl_version(req.msl_version);
        opts.use_framebuffer_fetch_subpasses = false;
        compiler.set_msl_options(opts);

        // ── Determine execution model ──────────────────────────────────────────
        std::string entry_name = "main";
        spv::ExecutionModel exec_model = spv::ExecutionModelMax;
        {
            auto eps = compiler.get_entry_points_and_stages();
            if (!eps.empty()) {
                entry_name = eps[0].name;
                exec_model = eps[0].execution_model;
            }
        }

        if (exec_model == spv::ExecutionModelMax) {
            result.code  = ResultCode::eCrossCompileFailed;
            result.error = "SpirvCrossCrossCompiler: no valid entry point found in SPIR-V";
            return result;
        }

        // ── Resource binding remapping ─────────────────────────────────────────
        auto resources = compiler.get_shader_resources();
        compiler.build_combined_image_samplers();
        auto combined = compiler.get_combined_image_samplers();

        std::unordered_set<spirv_cross::VariableID> comb_img_ids, comb_smp_ids;
        for (const auto& c : combined) {
            comb_img_ids.insert(c.image_id);
            comb_smp_ids.insert(c.sampler_id);
        }

        // Combined image samplers
        for (const auto& c : combined) {
            uint32_t set = compiler.has_decoration(c.image_id, spv::DecorationDescriptorSet)
                ? compiler.get_decoration(c.image_id, spv::DecorationDescriptorSet) : 0;
            uint32_t binding = compiler.has_decoration(c.image_id, spv::DecorationBinding)
                ? compiler.get_decoration(c.image_id, spv::DecorationBinding) : 0;
            uint32_t tex_idx = metalTextureSlot(set, binding);
            uint32_t smp_idx = metalSamplerSlot(set, binding);

            spirv_cross::MSLResourceBinding mb{};
            mb.stage       = exec_model;
            mb.desc_set    = set;
            mb.binding     = binding;
            mb.msl_texture = tex_idx;
            mb.msl_sampler = smp_idx;
            compiler.add_msl_resource_binding(mb);

            // Also remap the sampler resource separately
            uint32_t sset = compiler.has_decoration(c.sampler_id, spv::DecorationDescriptorSet)
                ? compiler.get_decoration(c.sampler_id, spv::DecorationDescriptorSet) : set;
            uint32_t sbind = compiler.has_decoration(c.sampler_id, spv::DecorationBinding)
                ? compiler.get_decoration(c.sampler_id, spv::DecorationBinding) : binding;
            spirv_cross::MSLResourceBinding smb{};
            smb.stage       = exec_model;
            smb.desc_set    = sset;
            smb.binding     = sbind;
            smb.msl_sampler = smp_idx;
            compiler.add_msl_resource_binding(smb);
        }

        // Uniform buffers
        for (const auto& r : resources.uniform_buffers) {
            uint32_t set = compiler.has_decoration(r.id, spv::DecorationDescriptorSet)
                ? compiler.get_decoration(r.id, spv::DecorationDescriptorSet) : 0;
            uint32_t binding = compiler.has_decoration(r.id, spv::DecorationBinding)
                ? compiler.get_decoration(r.id, spv::DecorationBinding) : 0;
            spirv_cross::MSLResourceBinding mb{};
            mb.stage      = exec_model;
            mb.desc_set   = set;
            mb.binding    = binding;
            mb.msl_buffer = metalBufferSlot(set, binding);
            compiler.add_msl_resource_binding(mb);
        }

        // Storage buffers
        for (const auto& r : resources.storage_buffers) {
            uint32_t set = compiler.has_decoration(r.id, spv::DecorationDescriptorSet)
                ? compiler.get_decoration(r.id, spv::DecorationDescriptorSet) : 0;
            uint32_t binding = compiler.has_decoration(r.id, spv::DecorationBinding)
                ? compiler.get_decoration(r.id, spv::DecorationBinding) : 0;
            spirv_cross::MSLResourceBinding mb{};
            mb.stage      = exec_model;
            mb.desc_set   = set;
            mb.binding    = binding;
            mb.msl_buffer = metalBufferSlot(set, binding);
            compiler.add_msl_resource_binding(mb);
        }

        // Separate images
        for (const auto& r : resources.separate_images) {
            if (comb_img_ids.count(r.id)) continue;
            uint32_t set = compiler.has_decoration(r.id, spv::DecorationDescriptorSet)
                ? compiler.get_decoration(r.id, spv::DecorationDescriptorSet) : 0;
            uint32_t binding = compiler.has_decoration(r.id, spv::DecorationBinding)
                ? compiler.get_decoration(r.id, spv::DecorationBinding) : 0;
            spirv_cross::MSLResourceBinding mb{};
            mb.stage       = exec_model;
            mb.desc_set    = set;
            mb.binding     = binding;
            mb.msl_texture = metalTextureSlot(set, binding);
            compiler.add_msl_resource_binding(mb);
        }

        // Separate samplers
        for (const auto& r : resources.separate_samplers) {
            if (comb_smp_ids.count(r.id)) continue;
            uint32_t set = compiler.has_decoration(r.id, spv::DecorationDescriptorSet)
                ? compiler.get_decoration(r.id, spv::DecorationDescriptorSet) : 0;
            uint32_t binding = compiler.has_decoration(r.id, spv::DecorationBinding)
                ? compiler.get_decoration(r.id, spv::DecorationBinding) : 0;
            spirv_cross::MSLResourceBinding mb{};
            mb.stage       = exec_model;
            mb.desc_set    = set;
            mb.binding     = binding;
            mb.msl_sampler = metalSamplerSlot(set, binding);
            compiler.add_msl_resource_binding(mb);
        }

        // Disable unused fragment inputs to suppress spurious [[stage_in]] parameters
        if (exec_model == spv::ExecutionModelFragment) {
            try {
                auto frag_res  = compiler.get_shader_resources();
                auto active    = compiler.get_active_interface_variables();
                if (!frag_res.stage_inputs.empty()) {
                    std::unordered_set<spirv_cross::VariableID> in_ids;
                    for (const auto& si : frag_res.stage_inputs) in_ids.insert(si.id);
                    bool any_active = false;
                    for (auto vid : in_ids) {
                        if (active.count(vid)) { any_active = true; break; }
                    }
                    if (!any_active) {
                        std::unordered_set<spirv_cross::VariableID> enabled;
                        for (auto vid : active) {
                            if (!in_ids.count(vid)) enabled.insert(vid);
                        }
                        compiler.set_enabled_interface_variables(enabled);
                    }
                }
            } catch (...) {}
        }

        result.source = compiler.compile();

        // Post-process: fix combined sampler indices
        if (exec_model == spv::ExecutionModelFragment) {
            fixCombinedSamplerIndices(result.source);
        }

        // Extract final MSL entry point name (SPIRV-Cross may rename "main" → "main0")
        try {
            result.entry_point = compiler.get_cleansed_entry_point_name(entry_name, exec_model);
        } catch (...) {
            result.entry_point = entry_name;
        }

        result.code = ResultCode::eSuccess;
        return result;

    } catch (const std::exception& e) {
        result.code  = ResultCode::eCrossCompileFailed;
        result.error = std::string("SpirvCrossCrossCompiler MSL: ") + e.what();
        return result;
    }
}

// ── GLSL / GLSL ES ────────────────────────────────────────────────────────────
CrossCompileResult SpirvCrossCrossCompiler::toGLSL(const CrossCompileRequest& req, bool es) {
    CrossCompileResult result;
    try {
        spirv_cross::CompilerGLSL compiler(req.spirv.data(), req.spirv.size());

        spirv_cross::CompilerGLSL::Options opts;
        opts.version                           = req.glsl_version;
        opts.es                                = es;
        opts.vulkan_semantics                  = false;
        opts.enable_420pack_extension          = false;
        opts.separate_shader_objects           = false;
        opts.emit_uniform_buffer_as_plain_uniforms = false;
        opts.vertex.fixup_clipspace            = true;
        opts.vertex.flip_vert_y                = false;
        compiler.set_common_options(opts);

        // Build combined image samplers (required for GLSL — no separate images+samplers)
        compiler.build_combined_image_samplers();
        for (const auto& c : compiler.get_combined_image_samplers()) {
            const std::string img_name = compiler.get_name(c.image_id);
            const std::string smp_name = compiler.get_name(c.sampler_id);
            std::string comb_name = img_name.empty() ? "combined_sampler" : img_name;
            if (!smp_name.empty() && smp_name != img_name) {
                comb_name += "_" + smp_name;
            }
            compiler.set_name(c.combined_id, comb_name);
        }

        result.source      = compiler.compile();
        result.entry_point = "main";
        result.code        = ResultCode::eSuccess;
        return result;

    } catch (const std::exception& e) {
        result.code  = ResultCode::eCrossCompileFailed;
        result.error = std::string("SpirvCrossCrossCompiler GLSL: ") + e.what();
        return result;
    }
}

// ── WGSL ─────────────────────────────────────────────────────────────────────
CrossCompileResult SpirvCrossCrossCompiler::toWGSL(const CrossCompileRequest& req) {
    CrossCompileResult result;
    // SPIRV-Cross does not have a WGSL backend; SPIR-V set/binding decorations
    // are preserved verbatim and naga/tint (external tools) perform the conversion.
    // For vnesc's purpose we return the SPIR-V as a placeholder and note that the
    // caller should use naga/tint for WGSL output.
    (void)req;
    result.code  = ResultCode::eUnavailable;
    result.error = "SpirvCrossCrossCompiler: WGSL output requires naga or tint (not yet integrated)";
    return result;
}

// ── FixMSLFragmentSignature ────────────────────────────────────────────────────
void SpirvCrossCrossCompiler::fixMSLFragmentSignature(std::string& frag_msl,
                                                      const std::string& vert_msl) {
    ::fixMSLFragmentSignature(frag_msl, vert_msl);
}

}  // namespace vne::sc
