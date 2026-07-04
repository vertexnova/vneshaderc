/* ---------------------------------------------------------------------
 * Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License")
 * ---------------------------------------------------------------------- */

#include "vertexnova/sc/gpu_layout_tools.h"

#include "vertexnova/logging/logging.h"

#include <cctype>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <unordered_set>

#ifdef VNE_SC_JSON_ENABLED
#include <nlohmann/json.hpp>
#endif

CREATE_VNE_LOGGER_CATEGORY("vne.sc.gpu_layout")

namespace vne::sc {

namespace {

#ifdef VNE_SC_JSON_ENABLED
bool parseUniformBuffersArray(const nlohmann::json& arr,
                              std::vector<ExpectedUniformBufferLayout>& out,
                              std::string& error) {
    if (!arr.is_array()) {
        error = "parseUniformBuffersArray: expected array";
        return false;
    }
    for (const auto& entry : arr) {
        if (!entry.contains("name") || !entry["name"].is_string()) {
            continue;
        }
        ExpectedUniformBufferLayout layout;
        layout.block_name = entry["name"].get<std::string>();
        if (entry.contains("size") && entry["size"].is_number_unsigned()) {
            layout.total_size = entry["size"].get<uint32_t>();
        } else if (entry.contains("total_size") && entry["total_size"].is_number_unsigned()) {
            layout.total_size = entry["total_size"].get<uint32_t>();
        }
        out.push_back(std::move(layout));
    }
    return true;
}
#endif

const StageReflection* pickReflectionStage(const ShaderArtifact& artifact, const std::string& stage_mode) {
    if (stage_mode == "vertex") {
        for (const auto& stage : artifact.stages) {
            if (stage.stage == ShaderStage::eVertex && !stage.reflection.bindings.empty()) {
                return &stage.reflection;
            }
        }
        if (!artifact.stages.empty()) {
            return &artifact.stages.front().reflection;
        }
        return nullptr;
    }

    if (stage_mode == "all") {
        return nullptr;
    }

    for (const auto& stage : artifact.stages) {
        if (stage.stage == ShaderStage::eFragment && !stage.reflection.bindings.empty()) {
            return &stage.reflection;
        }
    }
    if (artifact.stages.empty()) {
        return nullptr;
    }
    return &artifact.stages.back().reflection;
}

std::vector<const ReflectedBindingInfo*> collectBindings(const ShaderArtifact& artifact,
                                                         const std::string& stage_mode) {
    std::vector<const ReflectedBindingInfo*> bindings;

    if (stage_mode == "all") {
        std::unordered_set<std::string> seen;
        for (const auto& stage : artifact.stages) {
            for (const auto& binding : stage.reflection.bindings) {
                const std::string key =
                    std::to_string(binding.set) + ":" + std::to_string(binding.binding) + ":" + binding.name;
                if (seen.count(key) != 0) {
                    continue;
                }
                seen.insert(key);
                bindings.push_back(&binding);
            }
        }
    } else {
        const StageReflection* stage_refl = pickReflectionStage(artifact, stage_mode);
        if (stage_refl) {
            bindings.reserve(stage_refl->bindings.size());
            for (const auto& binding : stage_refl->bindings) {
                bindings.push_back(&binding);
            }
        }
    }

    std::sort(bindings.begin(), bindings.end(), [](const ReflectedBindingInfo* a, const ReflectedBindingInfo* b) {
        if (a->set != b->set) {
            return a->set < b->set;
        }
        return a->binding < b->binding;
    });

    return bindings;
}

bool shouldSkipBlock(const std::string& block_name, const EmitBindingDeclOptions& options) {
    if (!options.include_blocks.empty()) {
        for (const auto& include : options.include_blocks) {
            if (include == block_name) {
                return false;
            }
        }
        return true;
    }
    for (const auto& skip : options.skip_blocks) {
        if (skip == block_name) {
            return true;
        }
    }
    return false;
}

std::string blockNameToInstance(const std::string& block_name);

bool shouldSkipResource(const std::string& resource_name, const EmitBindingDeclOptions& options) {
    auto matches_skip = [&](const std::string& skip) -> bool {
        if (skip == resource_name) {
            return true;
        }
        return blockNameToInstance(skip) == resource_name;
    };

    if (options.include_blocks.empty()) {
        for (const auto& skip : options.skip_blocks) {
            if (matches_skip(skip)) {
                return true;
            }
        }
        return false;
    }
    for (const auto& include : options.include_blocks) {
        if (include == resource_name) {
            return false;
        }
    }
    return true;
}

uint32_t reflectedUboSize(const ReflectedBindingInfo& binding) {
    if (binding.struct_members.empty()) {
        return 0;
    }
    uint32_t total = 0;
    for (const auto& member : binding.struct_members) {
        total = std::max(total, member.offset + member.size);
    }
    return total;
}

std::string blockNameToInstance(const std::string& block_name) {
    if (block_name.empty()) {
        return block_name;
    }
    std::string out;
    out.reserve(block_name.size() + 4);
    for (size_t i = 0; i < block_name.size(); ++i) {
        const char c = block_name[i];
        if (i > 0 && std::isupper(static_cast<unsigned char>(c)) && !out.empty() && out.back() != '_') {
            out.push_back('_');
        }
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

std::string memberGlslType(const ReflectedStructMember& member) {
    if (!member.type_name.empty()) {
        return member.type_name;
    }
    if (member.is_matrix && member.matrix_columns == 4 && member.matrix_rows == 4) {
        return "mat4";
    }
    if (member.size == 16) {
        return "vec4";
    }
    if (member.size == 8) {
        return "vec2";
    }
    if (member.size == 4) {
        return "float";
    }
    return "float";
}

// array_count: 0 = unsized [], >1 = fixed [N], 1 = scalar (no brackets).
void emitMemberDecl(std::ostringstream& out, const std::string& type, const ReflectedStructMember& member) {
    out << "    " << type << " " << member.name;
    if (member.array_count == 0) {
        out << "[]";
    } else if (member.array_count > 1) {
        out << "[" << member.array_count << "]";
    }
    out << ";\n";
}

std::string emitSingleBinding(const ReflectedBindingInfo& binding) {
    std::ostringstream out;
    switch (binding.type) {
        case ReflectedResourceType::eUniformBuffer: {
            out << "layout(std140, set = " << binding.set << ", binding = " << binding.binding << ") uniform "
                << binding.name << " {\n";
            if (binding.struct_members.size() == 1 && !binding.struct_members[0].type_name.empty()) {
                const auto& member = binding.struct_members[0];
                emitMemberDecl(out, member.type_name, member);
                out << "};\n\n";
            } else {
                for (const auto& member : binding.struct_members) {
                    emitMemberDecl(out, memberGlslType(member), member);
                }
                out << "} " << blockNameToInstance(binding.name) << ";\n\n";
            }
            break;
        }
        case ReflectedResourceType::eStorageBuffer:
            out << "layout(std430, set = " << binding.set << ", binding = " << binding.binding << ") buffer "
                << binding.name << " {\n";
            if (binding.struct_members.empty()) {
                out << "    float data[];\n";
            } else {
                for (const auto& member : binding.struct_members) {
                    emitMemberDecl(out, memberGlslType(member), member);
                }
            }
            out << "} " << blockNameToInstance(binding.name) << ";\n\n";
            break;
        case ReflectedResourceType::eSampler:
            out << "layout(set = " << binding.set << ", binding = " << binding.binding << ") uniform sampler "
                << binding.name << ";\n\n";
            break;
        case ReflectedResourceType::eSampledImage:
            out << "layout(set = " << binding.set << ", binding = " << binding.binding << ") uniform texture2D "
                << binding.name << ";\n\n";
            break;
        case ReflectedResourceType::eSampledCubemap:
            out << "layout(set = " << binding.set << ", binding = " << binding.binding << ") uniform textureCube "
                << binding.name << ";\n\n";
            break;
        case ReflectedResourceType::eCombinedImageSampler:
            out << "layout(set = " << binding.set << ", binding = " << binding.binding << ") uniform sampler2D "
                << binding.name << ";\n\n";
            break;
        default:
            break;
    }
    return out.str();
}

}  // namespace

bool loadGpuLayoutRegistry(const std::filesystem::path& path, GpuLayoutRegistry& out, std::string& error) {
#ifndef VNE_SC_JSON_ENABLED
    (void)path;
    (void)out;
    error = "loadGpuLayoutRegistry: JSON support not compiled in";
    return false;
#else
    std::ifstream file{path};
    if (!file.is_open()) {
        error = "loadGpuLayoutRegistry: cannot open '" + path.string() + "'";
        return false;
    }
    try {
        nlohmann::json doc;
        file >> doc;
        std::vector<ExpectedUniformBufferLayout> parsed;
        if (!doc.contains("uniform_buffers") || !doc["uniform_buffers"].is_array()) {
            error = "loadGpuLayoutRegistry: missing uniform_buffers array";
            return false;
        }
        if (!parseUniformBuffersArray(doc["uniform_buffers"], parsed, error)) {
            return false;
        }
        out.uniform_buffers.insert(out.uniform_buffers.end(), parsed.begin(), parsed.end());
        return !parsed.empty();
    } catch (const std::exception& ex) {
        error = std::string("loadGpuLayoutRegistry: ") + ex.what();
        return false;
    }
#endif
}

bool mergeGpuLayoutRegistries(const std::vector<std::filesystem::path>& registry_paths,
                              const std::vector<ExpectedUniformBufferLayout>& inline_buffers,
                              GpuLayoutRegistry& out,
                              std::string& error) {
    out.uniform_buffers.clear();
    std::unordered_set<std::string> seen;

    auto addEntry = [&](const ExpectedUniformBufferLayout& entry) -> bool {
        if (entry.block_name.empty()) {
            return true;
        }
        if (seen.count(entry.block_name) != 0) {
            error = "mergeGpuLayoutRegistries: duplicate UBO '" + entry.block_name + "'";
            return false;
        }
        seen.insert(entry.block_name);
        out.uniform_buffers.push_back(entry);
        return true;
    };

    for (const auto& path : registry_paths) {
        GpuLayoutRegistry file_registry;
        if (!loadGpuLayoutRegistry(path, file_registry, error)) {
            return false;
        }
        for (const auto& entry : file_registry.uniform_buffers) {
            if (!addEntry(entry)) {
                return false;
            }
        }
    }

    for (const auto& entry : inline_buffers) {
        if (!addEntry(entry)) {
            return false;
        }
    }

    return true;
}

bool validateGpuLayouts(const ShaderArtifact& artifact, const GpuLayoutRegistry& registry, std::string& error) {
    const StageReflection* stage_refl = pickReflectionStage(artifact, "fragment");
    if (!stage_refl && !artifact.stages.empty()) {
        stage_refl = &artifact.stages.back().reflection;
    }
    if (!stage_refl) {
        error = "validateGpuLayouts: no reflection data available";
        return false;
    }

    for (const auto& expected : registry.uniform_buffers) {
        const ReflectedBindingInfo* match = nullptr;
        for (const auto& binding : stage_refl->bindings) {
            if (binding.type != ReflectedResourceType::eUniformBuffer) {
                continue;
            }
            if (binding.name == expected.block_name) {
                match = &binding;
                break;
            }
        }
        if (!match) {
            error = "validateGpuLayouts: missing UBO '" + expected.block_name + "' in reflection";
            return false;
        }
        const uint32_t reflected_size = reflectedUboSize(*match);
        if (reflected_size != expected.total_size) {
            error = "validateGpuLayouts: '" + expected.block_name + "' size mismatch: reflected="
                    + std::to_string(reflected_size) + " expected=" + std::to_string(expected.total_size);
            return false;
        }
    }
    return true;
}

std::string buildBindingDeclsGlsl(const ShaderArtifact& artifact, const EmitBindingDeclOptions& options) {
    std::ostringstream out;
    out << "// AUTO-GENERATED by vnesc_shader_compiler — DO NOT EDIT\n\n";

    for (const auto& include_path : options.compose_includes) {
        out << "#include \"" << include_path << "\"\n";
    }
    if (!options.compose_includes.empty()) {
        out << "\n";
    }

    const auto bindings = collectBindings(artifact, options.bindings_stage);
    for (const ReflectedBindingInfo* binding : bindings) {
        if (binding->type == ReflectedResourceType::eUniformBuffer
            || binding->type == ReflectedResourceType::eStorageBuffer) {
            if (shouldSkipBlock(binding->name, options)) {
                continue;
            }
        } else if (shouldSkipResource(binding->name, options)) {
            continue;
        }

        out << emitSingleBinding(*binding);
    }

    return out.str();
}

std::string emitBindingDeclsGlsl(const ShaderArtifact& artifact, const std::vector<std::string>& skip_blocks) {
    EmitBindingDeclOptions options;
    options.skip_blocks = skip_blocks;
    return buildBindingDeclsGlsl(artifact, options);
}

bool writeBindingDeclsFile(const std::filesystem::path& output_path, const std::string& contents, std::string& error) {
    if (output_path.empty()) {
        error = "writeBindingDeclsFile: empty output path";
        return false;
    }
    if (contents.empty()) {
        error = "writeBindingDeclsFile: empty contents";
        return false;
    }
    const auto parent = output_path.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
    }
    std::ofstream file{output_path};
    if (!file.is_open()) {
        error = "writeBindingDeclsFile: cannot open '" + output_path.string() + "'";
        return false;
    }
    file << contents;
    return static_cast<bool>(file);
}

}  // namespace vne::sc
