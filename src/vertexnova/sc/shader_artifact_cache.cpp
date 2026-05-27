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

#include "vertexnova/sc/shader_artifact_cache.h"
#include "vertexnova/sc/shader_reflection_binary.h"

#include "vertexnova/logging/logging.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <functional>
#include <iomanip>

CREATE_VNE_LOGGER_CATEGORY("vne.sc.cache")

namespace vne::sc {

// ── FNV-1a 64-bit hash ────────────────────────────────────────────────────────
namespace {

uint64_t fnv1a64(const void* data, size_t len) noexcept {
    const uint64_t FNV_PRIME = 0x100000001b3ULL;
    const uint64_t FNV_OFFSET = 0xcbf29ce484222325ULL;
    uint64_t hash = FNV_OFFSET;
    const auto* ptr = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < len; ++i) {
        hash ^= ptr[i];
        hash *= FNV_PRIME;
    }
    return hash;
}

void hashStr(uint64_t& h, const std::string& s) noexcept {
    h ^= fnv1a64(s.data(), s.size());
    h *= 0x100000001b3ULL;
    const size_t len = s.size();
    h ^= fnv1a64(&len, sizeof(len));
    h *= 0x100000001b3ULL;
}

std::string toHex(uint64_t v) {
    std::ostringstream os;
    os << std::hex << std::setfill('0') << std::setw(16) << v;
    return os.str();
}

// ── Binary I/O helpers ────────────────────────────────────────────────────────
struct Writer {
    std::ostringstream os{std::ios::binary};
    void u8(uint8_t v) { os.write(reinterpret_cast<const char*>(&v), 1); }
    void u32(uint32_t v) { os.write(reinterpret_cast<const char*>(&v), 4); }
    void boolean(bool v) { u8(static_cast<uint8_t>(v ? 1 : 0)); }
    void str(const std::string& s) {
        u32(static_cast<uint32_t>(s.size()));
        os.write(s.data(), static_cast<std::streamsize>(s.size()));
    }
};

struct Reader {
    std::istringstream is;
    explicit Reader(const std::string& data)
        : is(data, std::ios::binary) {}
    uint8_t u8() {
        uint8_t v{};
        is.read(reinterpret_cast<char*>(&v), 1);
        return v;
    }
    uint32_t u32() {
        uint32_t v{};
        is.read(reinterpret_cast<char*>(&v), 4);
        return v;
    }
    bool boolean() { return u8() != 0u; }
    std::string str() {
        const uint32_t len = u32();
        std::string s(len, '\0');
        is.read(s.data(), static_cast<std::streamsize>(len));
        return s;
    }
    bool ok() const { return is.good() || is.eof(); }
};

// ── StageArtifact binary format ───────────────────────────────────────────────
// stage(u8) | entry_point(str) | spirv_count(u32) | spirv_data |
// StageReflection | cross_count(u32) | { target(u8) | source(str) | ep(str) } * N

std::string serializeArtifact(const StageArtifact& a) {
    Writer w;
    w.u8(static_cast<uint8_t>(a.stage));
    w.str(a.entry_point);
    w.u32(static_cast<uint32_t>(a.spirv.size()));
    w.os.write(reinterpret_cast<const char*>(a.spirv.data()), static_cast<std::streamsize>(a.spirv.size() * 4));
    {
        const std::string refl_blob = vne::sc::serializeStageReflection(a.reflection);
        w.u32(static_cast<uint32_t>(refl_blob.size()));
        w.os.write(refl_blob.data(), static_cast<std::streamsize>(refl_blob.size()));
    }
    w.u32(static_cast<uint32_t>(a.cross_compiled.size()));
    for (const auto& cc : a.cross_compiled) {
        w.u8(static_cast<uint8_t>(cc.target));
        w.str(cc.source);
        w.str(cc.entry_point);
    }
    return w.os.str();
}

bool deserializeArtifact(const std::string& data, StageArtifact& out) {
    try {
        Reader r(data);
        out.stage = static_cast<ShaderStage>(r.u8());
        out.entry_point = r.str();
        const uint32_t spirv_count = r.u32();
        out.spirv.resize(spirv_count);
        r.is.read(reinterpret_cast<char*>(out.spirv.data()), static_cast<std::streamsize>(spirv_count * 4));
        {
            const uint32_t refl_size = r.u32();
            std::string refl_blob(refl_size, '\0');
            r.is.read(refl_blob.data(), static_cast<std::streamsize>(refl_size));
            if (!vne::sc::deserializeStageReflection(refl_blob, out.reflection)) {
                return false;
            }
        }
        out.reflection.stage = out.stage;
        const uint32_t cc_count = r.u32();
        out.cross_compiled.resize(cc_count);
        for (auto& cc : out.cross_compiled) {
            cc.target = static_cast<CrossTarget>(r.u8());
            cc.source = r.str();
            cc.entry_point = r.str();
        }
        return r.ok();
    } catch (...) {
        return false;
    }
}

}  // namespace

// ── ShaderArtifactCache ───────────────────────────────────────────────────────

ShaderArtifactCache::ShaderArtifactCache(std::string cache_dir)
    : cache_dir_(std::move(cache_dir)) {
    if (!cache_dir_.empty()) {
        std::filesystem::create_directories(cache_dir_);
        VNE_LOG_DEBUG << "ShaderArtifactCache: cache directory: " << cache_dir_;
    }
}

std::string ShaderArtifactCache::makeKey(const CompileRequest& req, const std::vector<CrossTarget>& targets) {
    uint64_t h = 0xcbf29ce484222325ULL;
    hashStr(h, req.source);
    hashStr(h, req.file_path);
    hashStr(h, req.entry_point);
    h ^= static_cast<uint64_t>(req.stage);
    h *= 0x100000001b3ULL;
    h ^= static_cast<uint64_t>(req.lang);
    h *= 0x100000001b3ULL;
    h ^= static_cast<uint64_t>(req.opt_level);
    h *= 0x100000001b3ULL;
    h ^= req.glsl_version;
    h *= 0x100000001b3ULL;
    for (const auto& m : req.macros) {
        hashStr(h, m.name);
        hashStr(h, m.value);
    }
    for (auto t : targets) {
        h ^= static_cast<uint64_t>(t);
        h *= 0x100000001b3ULL;
    }
    return toHex(h);
}

std::string ShaderArtifactCache::artifactPath(const std::string& key) const {
    return cache_dir_ + "/" + key + ".vnca";
}

std::optional<StageArtifact> ShaderArtifactCache::lookup(const std::string& key) const {
    if (cache_dir_.empty())
        return std::nullopt;
    const std::string path = artifactPath(key);
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open())
        return std::nullopt;
    auto size = static_cast<std::streamsize>(f.tellg());
    f.seekg(0);
    std::string data(static_cast<size_t>(size), '\0');
    f.read(data.data(), size);
    if (!f)
        return std::nullopt;
    StageArtifact artifact;
    if (!deserializeArtifact(data, artifact)) {
        VNE_LOG_WARN << "ShaderArtifactCache: corrupt artifact at " << path << " — ignoring";
        return std::nullopt;
    }
    VNE_LOG_DEBUG << "ShaderArtifactCache: cache hit for key " << key;
    return artifact;
}

void ShaderArtifactCache::store(const std::string& key, const StageArtifact& artifact) {
    if (cache_dir_.empty())
        return;
    std::filesystem::create_directories(cache_dir_);
    const std::string path = artifactPath(key);
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f.is_open()) {
        VNE_LOG_WARN << "ShaderArtifactCache: cannot write artifact to " << path;
        return;
    }
    const std::string data = serializeArtifact(artifact);
    f.write(data.data(), static_cast<std::streamsize>(data.size()));
    VNE_LOG_DEBUG << "ShaderArtifactCache: stored artifact for key " << key << " (" << data.size() << " bytes)";
}

void ShaderArtifactCache::clear() {
    if (cache_dir_.empty())
        return;
    uint32_t removed = 0;
    for (const auto& entry : std::filesystem::directory_iterator(cache_dir_)) {
        if (entry.path().extension() == ".vnca") {
            std::filesystem::remove(entry.path());
            ++removed;
        }
    }
    VNE_LOG_DEBUG << "ShaderArtifactCache: cleared " << removed << " artifact(s) from " << cache_dir_;
}

}  // namespace vne::sc
