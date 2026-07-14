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

#include <chrono>
#include <filesystem>
#include <gtest/gtest.h>
#include <sstream>

#include "vertexnova/sc/vnesc.h"

namespace vne::sc {

class ShaderArtifactCacheTest : public ::testing::Test {};

static std::filesystem::path makeUniqueTempPath(const char* prefix) {
    namespace fs = std::filesystem;
    const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::ostringstream name;
    name << prefix << "_" << stamp;
    return fs::temp_directory_path() / name.str();
}

TEST_F(ShaderArtifactCacheTest, MakeKeyIsDeterministic) {
    CompileRequest req;
    req.source = "void main() {}";
    req.entry_point = "main";
    req.stage = ShaderStage::eVertex;
    std::vector<CrossTarget> targets = {CrossTarget::eMSL};

    auto k1 = ShaderArtifactCache::makeKey(req, targets);
    auto k2 = ShaderArtifactCache::makeKey(req, targets);
    EXPECT_EQ(k1, k2);
    EXPECT_EQ(k1.size(), 16u);
}

TEST_F(ShaderArtifactCacheTest, DifferentSourceProducesDifferentKey) {
    CompileRequest a, b;
    a.source = "void main() { int x = 1; }";
    b.source = "void main() { int x = 2; }";
    std::vector<CrossTarget> targets;
    EXPECT_NE(ShaderArtifactCache::makeKey(a, targets), ShaderArtifactCache::makeKey(b, targets));
}

TEST_F(ShaderArtifactCacheTest, DifferentMetalLayoutProducesDifferentKey) {
    CompileRequest req;
    req.source = "void main() {}";
    std::vector<CrossTarget> targets = {CrossTarget::eMSL};
    MetalBindingLayout defaults;
    MetalBindingLayout custom_buffer = defaults;
    custom_buffer.buffer_base = defaults.buffer_base + 1u;

    MetalBindingLayout custom_stride = defaults;
    custom_stride.flatten_stride = defaults.flatten_stride + 1u;

    EXPECT_NE(ShaderArtifactCache::makeKey(req, targets, defaults),
              ShaderArtifactCache::makeKey(req, targets, custom_buffer));
    EXPECT_NE(ShaderArtifactCache::makeKey(req, targets, defaults),
              ShaderArtifactCache::makeKey(req, targets, custom_stride));
}

TEST_F(ShaderArtifactCacheTest, MetalLayoutIgnoredWhenMslNotAmongTargets) {
    CompileRequest req;
    req.source = "void main() {}";
    std::vector<CrossTarget> targets = {CrossTarget::eWGSL, CrossTarget::eGLSL};
    MetalBindingLayout defaults;
    MetalBindingLayout custom = defaults;
    custom.buffer_base = defaults.buffer_base + 1u;
    custom.flatten_stride = defaults.flatten_stride + 1u;

    EXPECT_EQ(ShaderArtifactCache::makeKey(req, targets, defaults), ShaderArtifactCache::makeKey(req, targets, custom));
}

TEST_F(ShaderArtifactCacheTest, DifferentMetalProgramFingerprintProducesDifferentKey) {
    CompileRequest req;
    req.source = "void main() {}";
    std::vector<CrossTarget> targets = {CrossTarget::eMSL};
    MetalBindingLayout layout;
    EXPECT_NE(ShaderArtifactCache::makeKey(req, targets, layout, 0x1111ULL),
              ShaderArtifactCache::makeKey(req, targets, layout, 0x2222ULL));
    EXPECT_EQ(ShaderArtifactCache::makeKey(req, targets, layout, 0xABCDULL),
              ShaderArtifactCache::makeKey(req, targets, layout, 0xABCDULL));
}

TEST_F(ShaderArtifactCacheTest, MakeProgramFingerprintIsSourceLevelAndDeterministic) {
    CompileRequest a;
    a.source = "void main() { int x = 1; }";
    a.stage = ShaderStage::eVertex;
    CompileRequest b = a;
    b.source = "void main() { int x = 2; }";
    MetalBindingLayout layout;

    EXPECT_EQ(ShaderArtifactCache::makeProgramFingerprint({a}, layout),
              ShaderArtifactCache::makeProgramFingerprint({a}, layout));
    EXPECT_NE(ShaderArtifactCache::makeProgramFingerprint({a}, layout),
              ShaderArtifactCache::makeProgramFingerprint({b}, layout));
}

TEST_F(ShaderArtifactCacheTest, StoreAndLookupRoundTrip) {
    namespace fs = std::filesystem;
    auto tmp = makeUniqueTempPath("vnesc_shader_artifact_cache_roundtrip");
    fs::remove_all(tmp);

    ShaderArtifactCache cache(tmp.string());

    StageArtifact artifact;
    artifact.stage = ShaderStage::eVertex;
    artifact.entry_point = "main";
    artifact.spirv = {0x07230203u, 1u, 2u, 3u};
    artifact.reflection.stage = ShaderStage::eVertex;

    CrossCompiledSource cc;
    cc.target = CrossTarget::eMSL;
    cc.source = "// msl source";
    cc.entry_point = "main0";
    artifact.cross_compiled.push_back(cc);

    const std::string key = "abcdef0123456789";
    cache.store(key, artifact);

    auto found = cache.lookup(key);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->spirv, artifact.spirv);
    EXPECT_EQ(found->reflection.stage, artifact.reflection.stage);
    ASSERT_EQ(found->cross_compiled.size(), 1u);
    EXPECT_EQ(found->cross_compiled[0].source, cc.source);
    EXPECT_EQ(found->cross_compiled[0].entry_point, cc.entry_point);

    fs::remove_all(tmp);
}

TEST_F(ShaderArtifactCacheTest, LookupMissReturnsNullopt) {
    namespace fs = std::filesystem;
    auto tmp = makeUniqueTempPath("vnesc_shader_artifact_cache_miss");
    fs::remove_all(tmp);
    ShaderArtifactCache cache(tmp.string());
    EXPECT_FALSE(cache.lookup("0000000000000000").has_value());
    fs::remove_all(tmp);
}

TEST_F(ShaderArtifactCacheTest, ClearRemovesAllEntries) {
    namespace fs = std::filesystem;
    auto tmp = makeUniqueTempPath("vnesc_shader_artifact_cache_clear");
    fs::remove_all(tmp);

    ShaderArtifactCache cache(tmp.string());
    StageArtifact a;
    a.spirv = {1u};
    cache.store("aaaaaaaaaaaaaaaa", a);
    cache.store("bbbbbbbbbbbbbbbb", a);
    EXPECT_TRUE(cache.lookup("aaaaaaaaaaaaaaaa").has_value());

    cache.clear();
    EXPECT_FALSE(cache.lookup("aaaaaaaaaaaaaaaa").has_value());
    EXPECT_FALSE(cache.lookup("bbbbbbbbbbbbbbbb").has_value());

    fs::remove_all(tmp);
}

}  // namespace vne::sc
