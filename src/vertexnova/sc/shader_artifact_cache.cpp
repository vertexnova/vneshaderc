/* ---------------------------------------------------------------------
 * Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License")
 *
 * Author:    Ajeet Singh Yadav
 * Created:   May 2026
 * ----------------------------------------------------------------------
 */

#include "vertexnova/sc/shader_artifact_cache.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <functional>
#include <iomanip>

namespace vne::sc {

// ── Simple hash-based key ────���────────────────────────────────────────────────
// We use std::hash + fnv-1a mix rather than pulling in a full SHA-256 library.
// The key is a hex string of a 64-bit hash of the request content + options.
namespace {

uint64_t fnv1a64(const void* data, size_t len) noexcept {
    const uint64_t FNV_PRIME  = 0x100000001b3ULL;
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

// Serialize a StageArtifact to a simple binary format for caching.
// Format: stage(u8) | entry_point_len(u32) | entry_point | spirv_count(u32) | spirv_data |
//         reflection_json_len(u32) | reflection_json |
//         cross_count(u32) | { target(u8) | src_len(u32) | src | ep_len(u32) | ep } * N
std::string serializeArtifact(const StageArtifact& a) {
    std::ostringstream os(std::ios::binary);
    auto write8  = [&](uint8_t  v) { os.write(reinterpret_cast<const char*>(&v), 1); };
    auto write32 = [&](uint32_t v) { os.write(reinterpret_cast<const char*>(&v), 4); };
    auto writeStr = [&](const std::string& s) {
        write32(static_cast<uint32_t>(s.size()));
        os.write(s.data(), static_cast<std::streamsize>(s.size()));
    };

    write8(static_cast<uint8_t>(a.stage));
    writeStr(a.entry_point);
    write32(static_cast<uint32_t>(a.spirv.size()));
    os.write(reinterpret_cast<const char*>(a.spirv.data()),
             static_cast<std::streamsize>(a.spirv.size() * 4));
    writeStr(a.reflection_json);
    write32(static_cast<uint32_t>(a.cross_compiled.size()));
    for (const auto& cc : a.cross_compiled) {
        write8(static_cast<uint8_t>(cc.target));
        writeStr(cc.source);
        writeStr(cc.entry_point);
    }
    return os.str();
}

bool deserializeArtifact(const std::string& data, StageArtifact& out) {
    try {
        std::istringstream is(data, std::ios::binary);
        auto read8  = [&]() -> uint8_t  {
            uint8_t v; is.read(reinterpret_cast<char*>(&v), 1); return v;
        };
        auto read32 = [&]() -> uint32_t {
            uint32_t v; is.read(reinterpret_cast<char*>(&v), 4); return v;
        };
        auto readStr = [&]() -> std::string {
            uint32_t len = read32();
            std::string s(len, '\0');
            is.read(s.data(), static_cast<std::streamsize>(len));
            return s;
        };

        out.stage       = static_cast<ShaderStage>(read8());
        out.entry_point = readStr();
        uint32_t spirv_count = read32();
        out.spirv.resize(spirv_count);
        is.read(reinterpret_cast<char*>(out.spirv.data()),
                static_cast<std::streamsize>(spirv_count * 4));
        out.reflection_json = readStr();
        uint32_t cc_count = read32();
        out.cross_compiled.resize(cc_count);
        for (auto& cc : out.cross_compiled) {
            cc.target      = static_cast<CrossTarget>(read8());
            cc.source      = readStr();
            cc.entry_point = readStr();
        }
        return is.good() || is.eof();
    } catch (...) {
        return false;
    }
}

}  // namespace

// ── ShaderArtifactCache ───────────────────���────────────────────────────────────

ShaderArtifactCache::ShaderArtifactCache(std::string cache_dir)
    : cache_dir_(std::move(cache_dir)) {
    if (!cache_dir_.empty()) {
        std::filesystem::create_directories(cache_dir_);
    }
}

std::string ShaderArtifactCache::makeKey(const CompileRequest& req,
                                         const std::vector<CrossTarget>& targets) {
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
    for (const auto& m : req.macros) { hashStr(h, m.name); hashStr(h, m.value); }
    for (auto t : targets) { h ^= static_cast<uint64_t>(t); h *= 0x100000001b3ULL; }
    return toHex(h);
}

std::string ShaderArtifactCache::artifactPath(const std::string& key) const {
    return cache_dir_ + "/" + key + ".vnca";
}

std::optional<StageArtifact> ShaderArtifactCache::lookup(const std::string& key) const {
    if (cache_dir_.empty()) return std::nullopt;
    const std::string path = artifactPath(key);
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return std::nullopt;
    auto size = static_cast<std::streamsize>(f.tellg());
    f.seekg(0);
    std::string data(static_cast<size_t>(size), '\0');
    f.read(data.data(), size);
    if (!f) return std::nullopt;
    StageArtifact artifact;
    if (!deserializeArtifact(data, artifact)) return std::nullopt;
    return artifact;
}

void ShaderArtifactCache::store(const std::string& key, const StageArtifact& artifact) {
    if (cache_dir_.empty()) return;
    std::filesystem::create_directories(cache_dir_);
    const std::string path = artifactPath(key);
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f.is_open()) return;
    const std::string data = serializeArtifact(artifact);
    f.write(data.data(), static_cast<std::streamsize>(data.size()));
}

void ShaderArtifactCache::clear() {
    if (cache_dir_.empty()) return;
    for (const auto& entry : std::filesystem::directory_iterator(cache_dir_)) {
        if (entry.path().extension() == ".vnca") {
            std::filesystem::remove(entry.path());
        }
    }
}

}  // namespace vne::sc
