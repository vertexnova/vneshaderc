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

#include "glslang_frontend.h"

#include "vertexnova/logging/logging.h"

#include <glslang/Public/ShaderLang.h>
#include <glslang/Public/ResourceLimits.h>
#include <SPIRV/GlslangToSpv.h>

#include <fstream>
#include <mutex>
#include <sstream>
#include <filesystem>

CREATE_VNE_LOGGER_CATEGORY("vne.sc.glslang")

namespace {

constexpr std::streamsize kMaxShaderFileBytes = 64 * 1024 * 1024;

struct GlslangProcessLifetime {
    std::mutex mutex;
    int refs = 0;
    bool process_ok = false;

    bool acquire() {
        std::lock_guard lock(mutex);
        if (refs == 0) {
            process_ok = glslang::InitializeProcess();
        }
        if (!process_ok) {
            return false;
        }
        ++refs;
        return true;
    }

    void release() {
        std::lock_guard lock(mutex);
        if (refs <= 0) {
            return;
        }
        if (--refs == 0 && process_ok) {
            glslang::FinalizeProcess();
            process_ok = false;
        }
    }
};

GlslangProcessLifetime& glslangProcessLifetime() {
    static GlslangProcessLifetime lifetime;
    return lifetime;
}

bool readBinaryFile(std::ifstream& file, std::string& out) {
    file.clear();
    file.seekg(0, std::ios::end);
    if (!file.good()) {
        return false;
    }
    const auto end = file.tellg();
    if (end < 0) {
        return false;
    }
    const auto size = static_cast<std::streamsize>(end);
    if (size > kMaxShaderFileBytes) {
        return false;
    }
    file.clear();
    file.seekg(0);
    if (!file.good()) {
        return false;
    }
    out.resize(static_cast<size_t>(size));
    if (size == 0) {
        return true;
    }
    file.read(out.data(), size);
    return file.gcount() == size;
}

// ── Stage mapping ─────────────────────────────────────────────────────────────
EShLanguage toEshLang(vne::sc::ShaderStage stage) {
    switch (stage) {
        case vne::sc::ShaderStage::eVertex:
            return EShLangVertex;
        case vne::sc::ShaderStage::eFragment:
            return EShLangFragment;
        case vne::sc::ShaderStage::eCompute:
            return EShLangCompute;
        case vne::sc::ShaderStage::eGeometry:
            return EShLangGeometry;
        case vne::sc::ShaderStage::eTessellationControl:
            return EShLangTessControl;
        case vne::sc::ShaderStage::eTessellationEvaluation:
            return EShLangTessEvaluation;
    }
    return EShLangVertex;
}

// ── #include resolver ─────────────────────────────────────────────────────────
class FileIncluder : public glslang::TShader::Includer {
   public:
    explicit FileIncluder(const std::vector<std::string>& include_dirs)
        : include_dirs_(include_dirs) {}

    IncludeResult* includeSystem(const char* header_name, const char* /*includer_name*/, size_t /*depth*/) override {
        return searchAndLoad(header_name);
    }

    IncludeResult* includeLocal(const char* header_name, const char* includer_name, size_t /*depth*/) override {
        if (includer_name && *includer_name) {
            auto candidate = std::filesystem::path(includer_name).parent_path() / header_name;
            if (std::filesystem::exists(candidate)) {
                return loadFile(candidate.string(), header_name);
            }
        }
        return searchAndLoad(header_name);
    }

    void releaseInclude(IncludeResult* result) override {
        if (result) {
            delete static_cast<std::string*>(result->userData);
            delete result;
        }
    }

   private:
    const std::vector<std::string>& include_dirs_;

    IncludeResult* searchAndLoad(const char* header_name) {
        for (const auto& dir : include_dirs_) {
            auto p = std::filesystem::path(dir) / header_name;
            if (std::filesystem::exists(p)) {
                return loadFile(p.string(), header_name);
            }
        }
        return new IncludeResult(header_name, "// not found", 13, nullptr);
    }

    IncludeResult* loadFile(const std::string& path, const char* header_name) {
        std::ifstream f(path, std::ios::binary);
        if (!f.is_open()) {
            return new IncludeResult(header_name, "// open failed", 14, nullptr);
        }
        auto* content = new std::string();
        if (!readBinaryFile(f, *content)) {
            delete content;
            return new IncludeResult(header_name, "// read failed", 14, nullptr);
        }
        return new IncludeResult(header_name, content->data(), content->size(), content);
    }
};

// ── Macro preamble ────────────────────────────────────────────────────────────
std::string buildPreamble(const std::vector<vne::sc::ShaderMacro>& macros) {
    std::string preamble;
    for (const auto& m : macros) {
        preamble += "#define " + m.name;
        if (!m.value.empty()) {
            preamble += " " + m.value;
        }
        preamble += "\n";
    }
    return preamble;
}

}  // namespace

namespace vne::sc {

GlslangFrontEnd::GlslangFrontEnd() {
    initialized_ = glslangProcessLifetime().acquire();
    if (!initialized_) {
        VNE_LOG_ERROR << "GlslangFrontEnd: glslang::InitializeProcess() failed";
    } else {
        VNE_LOG_DEBUG << "GlslangFrontEnd: initialized";
    }
}

GlslangFrontEnd::~GlslangFrontEnd() {
    if (initialized_) {
        glslangProcessLifetime().release();
        initialized_ = false;
    }
}

bool GlslangFrontEnd::isAvailable() const noexcept {
    return initialized_;
}

CompileResult GlslangFrontEnd::compile(const CompileRequest& req) {
    CompileResult result;

    if (!initialized_) {
        result.errors.push_back("GlslangFrontEnd: glslang initialisation failed");
        result.code = ResultCode::eUnavailable;
        VNE_LOG_ERROR << "GlslangFrontEnd: not initialized";
        return result;
    }

    // ── Resolve source text ────────────────────────────────────────────────────
    std::string source;
    std::string source_path;

    if (!req.source.empty()) {
        source = req.source;
        source_path = req.file_path.empty() ? "<inline>" : req.file_path;
    } else if (!req.file_path.empty()) {
        std::ifstream f(req.file_path, std::ios::binary);
        if (!f.is_open()) {
            result.errors.push_back("GlslangFrontEnd: cannot open '" + req.file_path + "'");
            result.code = ResultCode::eFileNotFound;
            return result;
        }
        if (!readBinaryFile(f, source)) {
            result.errors.push_back("GlslangFrontEnd: cannot read '" + req.file_path + "'");
            result.code = ResultCode::eInvalidArgument;
            return result;
        }
        source_path = req.file_path;
    } else {
        result.errors.push_back("GlslangFrontEnd: CompileRequest has no source and no file_path");
        result.code = ResultCode::eInvalidArgument;
        return result;
    }

    // ── Build TShader ──────────────────────────────────────────────────────────
    EShLanguage esh_lang = toEshLang(req.stage);
    glslang::TShader shader(esh_lang);

    const char* src_ptr = source.c_str();
    const char* src_name = source_path.c_str();
    shader.setStringsWithLengthsAndNames(&src_ptr, nullptr, &src_name, 1);
    shader.setEntryPoint(req.entry_point.c_str());
    shader.setSourceEntryPoint(req.entry_point.c_str());

    shader.setEnvInput(glslang::EShSourceGlsl, esh_lang, glslang::EShClientVulkan, 100);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);

    std::string preamble = buildPreamble(req.macros);
    if (!preamble.empty()) {
        shader.setPreamble(preamble.c_str());
    }

    auto messages = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules);
    if (req.debug_info) {
        messages = static_cast<EShMessages>(messages | EShMsgDebugInfo);
    }

    FileIncluder includer(req.include_dirs);
    const bool parsed =
        shader.parse(GetDefaultResources(), static_cast<int>(req.glsl_version), false, messages, includer);

    // Collect info-log lines, classified as warnings or errors
    if (const char* info = shader.getInfoLog(); info && *info) {
        std::istringstream ss(info);
        std::string line;
        while (std::getline(ss, line)) {
            if (line.empty()) {
                continue;
            }
            if (line.find("WARNING") != std::string::npos) {
                result.warnings.push_back(line);
            } else {
                result.errors.push_back(line);
            }
        }
    }

    if (!parsed) {
        result.code = ResultCode::eCompileFailed;
        VNE_LOG_ERROR << "GlslangFrontEnd: parse failed for " << source_path;
        return result;
    }

    // ── Link ───────────────────────────────────────────────────────────────────
    glslang::TProgram program;
    program.addShader(&shader);
    if (!program.link(messages)) {
        if (const char* info = program.getInfoLog(); info && *info) {
            result.errors.push_back(std::string(info));
        }
        result.code = ResultCode::eCompileFailed;
        return result;
    }

    // ── SPIR-V generation ──────────────────────────────────────────────────────
    glslang::SpvOptions spv_opts;
    spv_opts.generateDebugInfo = req.debug_info;
    spv_opts.disableOptimizer = (req.opt_level == OptLevel::eNone);
    spv_opts.optimizeSize = (req.opt_level == OptLevel::eSize);
    spv_opts.stripDebugInfo = !req.debug_info;

    spv::SpvBuildLogger logger;
    glslang::GlslangToSpv(*program.getIntermediate(esh_lang), result.spirv, &logger, &spv_opts);

    if (const std::string spv_log = logger.getAllMessages(); !spv_log.empty()) {
        result.warnings.push_back(spv_log);
    }

    if (result.spirv.empty()) {
        result.errors.push_back("GlslangFrontEnd: SPIR-V generation produced no output");
        result.code = ResultCode::eCompileFailed;
        return result;
    }

    result.code = result.warnings.empty() ? ResultCode::eSuccess : ResultCode::eCompileWarnings;
    VNE_LOG_DEBUG << "GlslangFrontEnd: compiled " << source_path << " → " << result.spirv.size() << " SPIR-V words";
    return result;
}

}  // namespace vne::sc
