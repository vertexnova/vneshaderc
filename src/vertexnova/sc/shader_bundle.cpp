/* ---------------------------------------------------------------------
 * Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License")
 * ----------------------------------------------------------------------
 */

#include "vertexnova/sc/shader_bundle.h"
#include "vertexnova/sc/shader_reflection_binary.h"

#include "vertexnova/logging/logging.h"

#include <fstream>
#include <sstream>

#ifdef VNE_SC_JSON_ENABLED
#include <nlohmann/json.hpp>
#endif

CREATE_VNE_LOGGER_CATEGORY("vne.sc.bundle")

namespace vne::sc {

namespace {

constexpr char kBundleMagic[4] = {'V', 'N', 'S', 'H'};
constexpr uint32_t kBundleVersion = 1u;

std::string stageSuffix(ShaderStage stage) {
    switch (stage) {
        case ShaderStage::eVertex:
            return "vert";
        case ShaderStage::eFragment:
            return "frag";
        case ShaderStage::eCompute:
            return "comp";
        case ShaderStage::eGeometry:
            return "geom";
        case ShaderStage::eTessellationControl:
            return "tesc";
        case ShaderStage::eTessellationEvaluation:
            return "tese";
    }
    return "stage";
}

bool writeSpirvFile(const std::filesystem::path& path, const std::vector<uint32_t>& spirv) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }
    out.write(reinterpret_cast<const char*>(spirv.data()),
              static_cast<std::streamsize>(spirv.size() * sizeof(uint32_t)));
    return static_cast<bool>(out);
}

bool writeTextFile(const std::filesystem::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }
    out << text;
    return static_cast<bool>(out);
}

bool writeBinaryFile(const std::filesystem::path& path, const std::string& data) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    return static_cast<bool>(out);
}

struct Writer {
    std::ostringstream os{std::ios::binary};
    void bytes(const char* data, size_t len) { os.write(data, static_cast<std::streamsize>(len)); }
    void u32(uint32_t v) { os.write(reinterpret_cast<const char*>(&v), 4); }
    void u8(uint8_t v) { os.write(reinterpret_cast<const char*>(&v), 1); }
    void str(const std::string& s) {
        u32(static_cast<uint32_t>(s.size()));
        os.write(s.data(), static_cast<std::streamsize>(s.size()));
    }
};

struct Reader {
    std::istringstream is;
    explicit Reader(const std::string& data)
        : is(data, std::ios::binary) {}
    void bytes(char* data, size_t len) { is.read(data, static_cast<std::streamsize>(len)); }
    uint32_t u32() {
        uint32_t v{};
        is.read(reinterpret_cast<char*>(&v), 4);
        return v;
    }
    uint8_t u8() {
        uint8_t v{};
        is.read(reinterpret_cast<char*>(&v), 1);
        return v;
    }
    std::string str() {
        const uint32_t len = u32();
        std::string s(len, '\0');
        is.read(s.data(), static_cast<std::streamsize>(len));
        return s;
    }
    bool ok() const { return is.good() || is.eof(); }
};

}  // namespace

bool writeShaderBundle(const ShaderArtifact& artifact, const std::filesystem::path& bundle_dir) {
    if (!artifact.isValid()) {
        VNE_LOG_ERROR << "writeShaderBundle: artifact is invalid";
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(bundle_dir, ec);
    if (ec) {
        VNE_LOG_ERROR << "writeShaderBundle: cannot create directory " << bundle_dir;
        return false;
    }

    ShaderBundleHeader header;
    header.name = artifact.name;
    header.source_lang = artifact.source_lang;

    for (const auto& stage : artifact.stages) {
        const std::string base = artifact.name + "." + stageSuffix(stage.stage);
        BundleStageFiles files;
        files.stage = stage.stage;
        files.entry_point = stage.entry_point;
        files.spirv_file = base + ".spv";

        if (!writeSpirvFile(bundle_dir / files.spirv_file, stage.spirv)) {
            VNE_LOG_ERROR << "writeShaderBundle: failed to write " << files.spirv_file;
            return false;
        }

        if (const auto* msl = stage.findCrossCompiled(CrossTarget::eMSL)) {
            files.msl_file = base + ".msl";
            if (!writeTextFile(bundle_dir / files.msl_file, msl->source)) {
                return false;
            }
        }
        if (const auto* wgsl = stage.findCrossCompiled(CrossTarget::eWGSL)) {
            files.wgsl_file = base + ".wgsl";
            if (!writeTextFile(bundle_dir / files.wgsl_file, wgsl->source)) {
                return false;
            }
        }

        header.stages.push_back(std::move(files));
    }

    const ProgramReflection reflection = artifact.assembleReflection();
    if (!writeBinaryFile(bundle_dir / "reflection.bin", serializeProgramReflection(reflection))) {
        VNE_LOG_ERROR << "writeShaderBundle: failed to write reflection.bin";
        return false;
    }

    Writer w;
    w.bytes(kBundleMagic, 4);
    w.u32(kBundleVersion);
    w.str(header.name);
    w.u8(static_cast<uint8_t>(header.source_lang));
    w.u32(static_cast<uint32_t>(header.stages.size()));
    for (const auto& s : header.stages) {
        w.u8(static_cast<uint8_t>(s.stage));
        w.str(s.entry_point);
        w.str(s.spirv_file);
        w.str(s.msl_file);
        w.str(s.wgsl_file);
    }
    if (!writeBinaryFile(bundle_dir / "bundle.header", w.os.str())) {
        return false;
    }

#ifdef VNE_SC_JSON_ENABLED
    nlohmann::json manifest;
    manifest["name"] = artifact.name;
    manifest["source_lang"] = "GLSL";
    manifest["stages"] = nlohmann::json::array();
    for (const auto& s : header.stages) {
        nlohmann::json entry;
        entry["stage"] = stageSuffix(s.stage);
        entry["entry"] = s.entry_point;
        entry["spirv"] = s.spirv_file;
        if (!s.msl_file.empty()) {
            entry["msl"] = s.msl_file;
        }
        if (!s.wgsl_file.empty()) {
            entry["wgsl"] = s.wgsl_file;
        }
        manifest["stages"].push_back(std::move(entry));
    }
    writeTextFile(bundle_dir / "manifest.json", manifest.dump(2));
#endif

    VNE_LOG_INFO << "writeShaderBundle: wrote bundle to " << bundle_dir;
    return true;
}

std::optional<ShaderBundleHeader> readShaderBundleHeader(const std::filesystem::path& bundle_dir) {
    const auto header_path = bundle_dir / "bundle.header";
    std::ifstream in(header_path, std::ios::binary | std::ios::ate);
    if (!in.is_open()) {
        return std::nullopt;
    }
    const auto end_pos = in.tellg();
    if (end_pos < 0) {
        return std::nullopt;
    }
    const auto size = static_cast<size_t>(end_pos);
    in.seekg(0);
    std::string data(size, '\0');
    in.read(data.data(), static_cast<std::streamsize>(size));

    try {
        Reader r(data);
        char magic[4]{};
        r.bytes(magic, 4);
        if (std::string(magic, 4) != std::string(kBundleMagic, 4)) {
            return std::nullopt;
        }
        if (r.u32() != kBundleVersion) {
            return std::nullopt;
        }
        ShaderBundleHeader header;
        header.name = r.str();
        header.source_lang = static_cast<SourceLang>(r.u8());
        const uint32_t count = r.u32();
        constexpr uint32_t kMaxStages = 16;
        if (count > kMaxStages) {
            return std::nullopt;
        }
        header.stages.resize(count);
        for (auto& s : header.stages) {
            s.stage = static_cast<ShaderStage>(r.u8());
            s.entry_point = r.str();
            s.spirv_file = r.str();
            s.msl_file = r.str();
            s.wgsl_file = r.str();
        }
        if (!r.ok()) {
            return std::nullopt;
        }
        return header;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<ProgramReflection> readShaderBundleReflection(const std::filesystem::path& bundle_dir) {
    std::ifstream in(bundle_dir / "reflection.bin", std::ios::binary | std::ios::ate);
    if (!in.is_open()) {
        return std::nullopt;
    }
    const auto end_pos = in.tellg();
    if (end_pos < 0) {
        return std::nullopt;
    }
    const auto size = static_cast<size_t>(end_pos);
    in.seekg(0);
    std::string data(size, '\0');
    in.read(data.data(), static_cast<std::streamsize>(size));
    ProgramReflection reflection;
    if (!deserializeProgramReflection(data, reflection)) {
        return std::nullopt;
    }
    return reflection;
}

}  // namespace vne::sc
