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

#include "vertexnova/sc/shader_pipeline_spec.h"

#include "vertexnova/logging/logging.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#ifdef VNE_SC_JSON_ENABLED
#include <nlohmann/json.hpp>
#endif

CREATE_VNE_LOGGER_CATEGORY("vne.sc.spec")

namespace vne::sc {

namespace {

std::optional<ShaderStage> parseStageString(const std::string& s) {
    if (s == "vertex")
        return ShaderStage::eVertex;
    if (s == "fragment")
        return ShaderStage::eFragment;
    if (s == "compute")
        return ShaderStage::eCompute;
    if (s == "geometry")
        return ShaderStage::eGeometry;
    if (s == "tessellation_control" || s == "tess_control")
        return ShaderStage::eTessellationControl;
    if (s == "tessellation_evaluation" || s == "tess_eval")
        return ShaderStage::eTessellationEvaluation;
    return std::nullopt;
}

std::optional<CrossTarget> parseTargetString(const std::string& s) {
    if (s == "MSL" || s == "msl")
        return CrossTarget::eMSL;
    if (s == "WGSL" || s == "wgsl")
        return CrossTarget::eWGSL;
    if (s == "GLSL" || s == "glsl")
        return CrossTarget::eGLSL;
    if (s == "GLSLES" || s == "glsles")
        return CrossTarget::eGLSLES;
    if (s == "HLSL" || s == "hlsl")
        return CrossTarget::eHLSL;
    return std::nullopt;
}

std::optional<SourceLang> parseSourceLangString(const std::string& s) {
    if (s == "GLSL" || s == "glsl")
        return SourceLang::eGLSL;
    if (s == "HLSL" || s == "hlsl")
        return SourceLang::eHLSL;
    if (s == "MSL" || s == "msl")
        return SourceLang::eMSL;
    if (s == "WGSL" || s == "wgsl")
        return SourceLang::eWGSL;
    if (s == "Slang" || s == "slang")
        return SourceLang::eSlang;
    return std::nullopt;
}

}  // namespace

PipelineBuildDesc ShaderPipelineSpec::toBuildDesc(const std::filesystem::path& spec_dir) const {
    PipelineBuildDesc desc;
    desc.name = name;
    desc.validate = validate;
    desc.targets = targets;

    // Resolve include_paths relative to spec_dir (if provided).
    std::vector<std::string> resolved_includes;
    for (const auto& inc : include_paths) {
        if (!spec_dir.empty()) {
            resolved_includes.push_back((spec_dir / inc).string());
        } else {
            resolved_includes.push_back(inc);
        }
    }

    for (const auto& ss : stages) {
        CompileRequest req;
        req.file_path = ss.file;
        req.entry_point = ss.entry_point;
        req.stage = ss.stage;
        req.lang = source_lang;
        req.include_dirs = resolved_includes;
        desc.stages.push_back(std::move(req));
    }
    return desc;
}

std::optional<ShaderPipelineSpec> parseShaderPipelineSpecJson(const std::string& json) {
#ifndef VNE_SC_JSON_ENABLED
    (void)json;
    VNE_LOG_ERROR << "parseShaderPipelineSpecJson: JSON support not compiled in";
    return std::nullopt;
#else
    ShaderPipelineSpec spec;
    try {
        const auto doc = nlohmann::json::parse(json);

        if (!doc.contains("name") || !doc["name"].is_string()) {
            VNE_LOG_ERROR << "parseShaderPipelineSpecJson: missing or invalid 'name'";
            return std::nullopt;
        }
        spec.name = doc["name"].get<std::string>();

        if (doc.contains("source_lang") && doc["source_lang"].is_string()) {
            if (auto lang = parseSourceLangString(doc["source_lang"].get<std::string>())) {
                spec.source_lang = *lang;
            } else {
                spec.errors.push_back("unknown source_lang: " + doc["source_lang"].get<std::string>());
            }
        }

        if (doc.contains("validate") && doc["validate"].is_boolean()) {
            spec.validate = doc["validate"].get<bool>();
        }

        if (doc.contains("targets") && doc["targets"].is_array()) {
            for (const auto& t : doc["targets"]) {
                if (!t.is_string())
                    continue;
                if (auto target = parseTargetString(t.get<std::string>())) {
                    spec.targets.push_back(*target);
                } else {
                    spec.errors.push_back("unknown target: " + t.get<std::string>());
                }
            }
        }

        if (doc.contains("include_paths") && doc["include_paths"].is_array()) {
            for (const auto& p : doc["include_paths"]) {
                if (p.is_string()) {
                    spec.include_paths.push_back(p.get<std::string>());
                }
            }
        }

        if (!doc.contains("stages") || !doc["stages"].is_array()) {
            VNE_LOG_ERROR << "parseShaderPipelineSpecJson: missing or invalid 'stages' array";
            return std::nullopt;
        }

        for (const auto& stage_entry : doc["stages"]) {
            if (!stage_entry.contains("stage") || !stage_entry["stage"].is_string()) {
                spec.errors.push_back("stage entry missing 'stage' field");
                continue;
            }
            if (!stage_entry.contains("file") || !stage_entry["file"].is_string()) {
                spec.errors.push_back("stage entry missing 'file' field");
                continue;
            }
            auto stage_val = parseStageString(stage_entry["stage"].get<std::string>());
            if (!stage_val) {
                spec.errors.push_back("unknown stage: " + stage_entry["stage"].get<std::string>());
                continue;
            }
            ShaderStageSpec ss;
            ss.stage = *stage_val;
            ss.file = stage_entry["file"].get<std::string>();
            if (stage_entry.contains("entry_point") && stage_entry["entry_point"].is_string()) {
                ss.entry_point = stage_entry["entry_point"].get<std::string>();
            } else if (stage_entry.contains("entry") && stage_entry["entry"].is_string()) {
                // Accept legacy "entry" key for backward compat with old .manifest.json files.
                ss.entry_point = stage_entry["entry"].get<std::string>();
            }
            spec.stages.push_back(std::move(ss));
        }

        if (!spec.errors.empty()) {
            for (const auto& e : spec.errors) {
                VNE_LOG_WARN << "parseShaderPipelineSpecJson: " << e;
            }
        }
        if (spec.stages.empty()) {
            VNE_LOG_ERROR << "parseShaderPipelineSpecJson: no valid stages parsed";
            return std::nullopt;
        }

        VNE_LOG_DEBUG << "parseShaderPipelineSpecJson: loaded '" << spec.name << "' (" << spec.stages.size()
                      << " stage(s))";
        return spec;

    } catch (const std::exception& ex) {
        VNE_LOG_ERROR << "parseShaderPipelineSpecJson: " << ex.what();
        return std::nullopt;
    }
#endif
}

std::optional<ShaderPipelineSpec> loadShaderPipelineSpec(const std::string& path) {
    std::ifstream file{path};
    if (!file.is_open()) {
        VNE_LOG_ERROR << "loadShaderPipelineSpec: cannot open '" << path << "'";
        return std::nullopt;
    }
    std::ostringstream buf;
    buf << file.rdbuf();
    auto spec = parseShaderPipelineSpecJson(buf.str());
    if (spec) {
        // Pass the spec file's directory so toBuildDesc can resolve include_paths.
        spec->toBuildDesc(std::filesystem::path(path).parent_path());
    }
    return spec;
}

}  // namespace vne::sc
