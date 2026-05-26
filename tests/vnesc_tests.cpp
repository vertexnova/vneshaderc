/*
 * Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License").
 *
 * vnesc unit and integration tests.
 *
 * Test groups:
 *   ResultCode     — succeeded() helper and result struct ok() predicates
 *   ShaderArtifact — isValid, findStage, findCrossCompiled
 *   ArtifactCache  — makeKey determinism, store/lookup round-trip, clear
 *   Factory        — createFrontEnd stub availability
 *   GlslangCompile — end-to-end GLSL → SPIR-V (requires VNE_SC_GLSLANG_ENABLED)
 *   Reflection     — SPIR-V → JSON binding description
 *   CrossCompile   — SPIR-V → MSL
 *   Pipeline       — full PipelineBuilder flow including cache hit
 */

#include <gtest/gtest.h>
#include <filesystem>

#include "vertexnova/sc/vnesc.h"

// ─────────────────────────────────────────────────────────────────────────────
// ResultCode
// ─────────────────────────────────────────────────────────────────────────────

TEST(ResultCode, SucceededForNonNegative) {
    EXPECT_TRUE(vne::sc::succeeded(vne::sc::ResultCode::eSuccess));
    EXPECT_TRUE(vne::sc::succeeded(vne::sc::ResultCode::eCompileWarnings));
}

TEST(ResultCode, FailedForNegative) {
    EXPECT_FALSE(vne::sc::succeeded(vne::sc::ResultCode::eCompileFailed));
    EXPECT_FALSE(vne::sc::succeeded(vne::sc::ResultCode::eCrossCompileFailed));
    EXPECT_FALSE(vne::sc::succeeded(vne::sc::ResultCode::eUnavailable));
    EXPECT_FALSE(vne::sc::succeeded(vne::sc::ResultCode::eFileNotFound));
}

TEST(ResultCode, OkHelperMatchesSucceeded) {
    vne::sc::CompileResult cr;
    cr.code = vne::sc::ResultCode::eSuccess;
    EXPECT_TRUE(cr.ok());
    cr.code = vne::sc::ResultCode::eCompileFailed;
    EXPECT_FALSE(cr.ok());
}

// ─────────────────────────────────────────────────────────────────────────────
// ShaderArtifact
// ─────────────────────────────────────────────────────────────────────────────

TEST(ShaderArtifact, InvalidWhenEmpty) {
    vne::sc::ShaderArtifact a;
    EXPECT_FALSE(a.isValid());
}

TEST(ShaderArtifact, InvalidWhenStageHasNoSpirv) {
    vne::sc::ShaderArtifact a;
    vne::sc::StageArtifact  s;
    s.stage = vne::sc::ShaderStage::eVertex;
    // spirv intentionally left empty
    a.stages.push_back(s);
    EXPECT_FALSE(a.isValid());
}

TEST(ShaderArtifact, ValidWhenAllStagesHaveSpirv) {
    vne::sc::ShaderArtifact a;
    vne::sc::StageArtifact  s;
    s.stage = vne::sc::ShaderStage::eVertex;
    s.spirv = {0x07230203u, 0u};
    a.stages.push_back(s);
    EXPECT_TRUE(a.isValid());
}

TEST(ShaderArtifact, FindStageReturnsCorrectEntry) {
    vne::sc::ShaderArtifact a;
    vne::sc::StageArtifact  vert, frag;
    vert.stage = vne::sc::ShaderStage::eVertex;
    vert.spirv = {1u};
    frag.stage = vne::sc::ShaderStage::eFragment;
    frag.spirv = {2u};
    a.stages.push_back(vert);
    a.stages.push_back(frag);

    ASSERT_NE(a.findStage(vne::sc::ShaderStage::eVertex),   nullptr);
    ASSERT_NE(a.findStage(vne::sc::ShaderStage::eFragment), nullptr);
    EXPECT_EQ(a.findStage(vne::sc::ShaderStage::eCompute),  nullptr);
    EXPECT_EQ(a.findStage(vne::sc::ShaderStage::eVertex)->spirv[0], 1u);
}

TEST(StageArtifact, FindCrossCompiledReturnsCorrectEntry) {
    vne::sc::StageArtifact s;
    vne::sc::CrossCompiledSource msl;
    msl.target      = vne::sc::CrossTarget::eMSL;
    msl.source      = "// msl";
    msl.entry_point = "main0";
    s.cross_compiled.push_back(msl);

    ASSERT_NE(s.findCrossCompiled(vne::sc::CrossTarget::eMSL),  nullptr);
    EXPECT_EQ(s.findCrossCompiled(vne::sc::CrossTarget::eWGSL), nullptr);
    EXPECT_EQ(s.findCrossCompiled(vne::sc::CrossTarget::eMSL)->entry_point, "main0");
}

// ─────────────────────────────────────────────────────────────────────────────
// ShaderArtifactCache
// ─────────────────────────────────────────────────────────────────────────────

TEST(ArtifactCache, MakeKeyIsDeterministic) {
    vne::sc::CompileRequest req;
    req.source      = "void main() {}";
    req.entry_point = "main";
    req.stage       = vne::sc::ShaderStage::eVertex;
    std::vector<vne::sc::CrossTarget> targets = {vne::sc::CrossTarget::eMSL};

    auto k1 = vne::sc::ShaderArtifactCache::makeKey(req, targets);
    auto k2 = vne::sc::ShaderArtifactCache::makeKey(req, targets);
    EXPECT_EQ(k1, k2);
    EXPECT_EQ(k1.size(), 16u);  // 64-bit value as 16 hex digits
}

TEST(ArtifactCache, DifferentSourceProducesDifferentKey) {
    vne::sc::CompileRequest a, b;
    a.source = "void main() { int x = 1; }";
    b.source = "void main() { int x = 2; }";
    std::vector<vne::sc::CrossTarget> targets;
    EXPECT_NE(vne::sc::ShaderArtifactCache::makeKey(a, targets),
              vne::sc::ShaderArtifactCache::makeKey(b, targets));
}

TEST(ArtifactCache, StoreAndLookupRoundTrip) {
    namespace fs = std::filesystem;
    auto tmp = fs::temp_directory_path() / "vnesc_cache_test_roundtrip";
    fs::remove_all(tmp);

    vne::sc::ShaderArtifactCache cache(tmp.string());

    vne::sc::StageArtifact artifact;
    artifact.stage           = vne::sc::ShaderStage::eVertex;
    artifact.entry_point     = "main";
    artifact.spirv           = {0x07230203u, 1u, 2u, 3u};
    artifact.reflection_json = R"({"name":"vertex","bindings":[]})";

    vne::sc::CrossCompiledSource cc;
    cc.target      = vne::sc::CrossTarget::eMSL;
    cc.source      = "// msl source";
    cc.entry_point = "main0";
    artifact.cross_compiled.push_back(cc);

    const std::string key = "abcdef0123456789";
    cache.store(key, artifact);

    auto found = cache.lookup(key);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->spirv,           artifact.spirv);
    EXPECT_EQ(found->reflection_json, artifact.reflection_json);
    ASSERT_EQ(found->cross_compiled.size(), 1u);
    EXPECT_EQ(found->cross_compiled[0].source,      cc.source);
    EXPECT_EQ(found->cross_compiled[0].entry_point, cc.entry_point);

    fs::remove_all(tmp);
}

TEST(ArtifactCache, LookupMissReturnsNullopt) {
    namespace fs = std::filesystem;
    auto tmp = fs::temp_directory_path() / "vnesc_cache_test_miss";
    fs::remove_all(tmp);
    vne::sc::ShaderArtifactCache cache(tmp.string());
    EXPECT_FALSE(cache.lookup("0000000000000000").has_value());
    fs::remove_all(tmp);
}

TEST(ArtifactCache, ClearRemovesAllEntries) {
    namespace fs = std::filesystem;
    auto tmp = fs::temp_directory_path() / "vnesc_cache_test_clear";
    fs::remove_all(tmp);

    vne::sc::ShaderArtifactCache cache(tmp.string());
    vne::sc::StageArtifact a;
    a.spirv = {1u};
    cache.store("aaaaaaaaaaaaaaaa", a);
    cache.store("bbbbbbbbbbbbbbbb", a);
    EXPECT_TRUE(cache.lookup("aaaaaaaaaaaaaaaa").has_value());

    cache.clear();
    EXPECT_FALSE(cache.lookup("aaaaaaaaaaaaaaaa").has_value());
    EXPECT_FALSE(cache.lookup("bbbbbbbbbbbbbbbb").has_value());

    fs::remove_all(tmp);
}

// ─────────────────────────────────────────────────────────────────────────────
// Factory — stub availability
// ─────────────────────────────────────────────────────────────────────────────

TEST(Factory, HlslFrontEndIsNotAvailable) {
    auto fe = vne::sc::ShaderCompilerFactory::createFrontEnd(vne::sc::SourceLang::eHLSL);
    ASSERT_NE(fe, nullptr);
    EXPECT_FALSE(fe->isAvailable());
    auto r = fe->compile({});
    EXPECT_FALSE(r.ok());
    EXPECT_EQ(r.code, vne::sc::ResultCode::eUnavailable);
}

TEST(Factory, SlangFrontEndIsNotAvailable) {
    auto fe = vne::sc::ShaderCompilerFactory::createFrontEnd(vne::sc::SourceLang::eSlang);
    ASSERT_NE(fe, nullptr);
    EXPECT_FALSE(fe->isAvailable());
}

TEST(Factory, CrossCompilerIsAvailable) {
    auto cc = vne::sc::ShaderCompilerFactory::createCrossCompiler();
    ASSERT_NE(cc, nullptr);
    EXPECT_TRUE(cc->isAvailable());
}

// ─────────────────────────────────────────────────────────────────────────────
// Glslang compilation (requires VNE_SC_GLSLANG_ENABLED)
// ─────────────────────────────────────────────────────────────────────────────

#ifdef VNE_SC_GLSLANG_ENABLED

static const char* kVertGlsl = R"glsl(
    #version 450
    layout(location = 0) in  vec3 aPos;
    layout(location = 0) out vec4 vColor;
    layout(set = 0, binding = 0) uniform Matrices { mat4 mvp; } ubo;
    void main() {
        gl_Position = ubo.mvp * vec4(aPos, 1.0);
        vColor      = vec4(aPos, 1.0);
    }
)glsl";

static const char* kFragGlsl = R"glsl(
    #version 450
    layout(location = 0) in  vec4 vColor;
    layout(location = 0) out vec4 oColor;
    void main() { oColor = vColor; }
)glsl";

TEST(GlslangCompile, VertexShaderProducesNonEmptySpirv) {
    auto fe = vne::sc::ShaderCompilerFactory::createFrontEnd(vne::sc::SourceLang::eGLSL);
    ASSERT_TRUE(fe->isAvailable());

    vne::sc::CompileRequest req;
    req.source = kVertGlsl;
    req.stage  = vne::sc::ShaderStage::eVertex;

    auto cr = fe->compile(req);
    EXPECT_TRUE(cr.ok()) << (cr.errors.empty() ? "" : cr.errors[0]);
    EXPECT_FALSE(cr.spirv.empty());
}

TEST(GlslangCompile, FragmentShaderProducesNonEmptySpirv) {
    auto fe = vne::sc::ShaderCompilerFactory::createFrontEnd(vne::sc::SourceLang::eGLSL);
    ASSERT_TRUE(fe->isAvailable());

    vne::sc::CompileRequest req;
    req.source = kFragGlsl;
    req.stage  = vne::sc::ShaderStage::eFragment;

    auto cr = fe->compile(req);
    EXPECT_TRUE(cr.ok()) << (cr.errors.empty() ? "" : cr.errors[0]);
    EXPECT_FALSE(cr.spirv.empty());
}

TEST(GlslangCompile, InvalidGlslReturnsCompileFailed) {
    auto fe = vne::sc::ShaderCompilerFactory::createFrontEnd(vne::sc::SourceLang::eGLSL);
    ASSERT_TRUE(fe->isAvailable());

    vne::sc::CompileRequest req;
    req.source = "#version 450\nvoid main() { SYNTAX ERROR HERE }";
    req.stage  = vne::sc::ShaderStage::eVertex;

    auto cr = fe->compile(req);
    EXPECT_FALSE(cr.ok());
    EXPECT_EQ(cr.code, vne::sc::ResultCode::eCompileFailed);
    EXPECT_FALSE(cr.errors.empty());
}

TEST(GlslangCompile, EmptyRequestReturnsInvalidArgument) {
    auto fe = vne::sc::ShaderCompilerFactory::createFrontEnd(vne::sc::SourceLang::eGLSL);
    ASSERT_TRUE(fe->isAvailable());

    auto cr = fe->compile({});
    EXPECT_FALSE(cr.ok());
    EXPECT_EQ(cr.code, vne::sc::ResultCode::eInvalidArgument);
}

TEST(GlslangCompile, MacrosAreInjectedIntoPreamble) {
    auto fe = vne::sc::ShaderCompilerFactory::createFrontEnd(vne::sc::SourceLang::eGLSL);
    ASSERT_TRUE(fe->isAvailable());

    vne::sc::CompileRequest req;
    req.source = R"glsl(
        #version 450
        #ifndef MY_VALUE
        #error MY_VALUE not defined
        #endif
        void main() {}
    )glsl";
    req.stage  = vne::sc::ShaderStage::eVertex;
    req.macros = {{"MY_VALUE", "1"}};

    auto cr = fe->compile(req);
    EXPECT_TRUE(cr.ok()) << (cr.errors.empty() ? "" : cr.errors[0]);
}

// ─────────────────────────────────────────────────────────────────────────────
// Reflection
// ─────────────────────────────────────────────────────────────────────────────

TEST(Reflection, VertexShaderWithUboProducesJson) {
    auto fe = vne::sc::ShaderCompilerFactory::createFrontEnd(vne::sc::SourceLang::eGLSL);
    ASSERT_TRUE(fe->isAvailable());

    vne::sc::CompileRequest req;
    req.source = kVertGlsl;
    req.stage  = vne::sc::ShaderStage::eVertex;
    auto cr = fe->compile(req);
    ASSERT_TRUE(cr.ok());

    auto reflector = vne::sc::ShaderCompilerFactory::createReflector();
    auto rr        = reflector->reflectToJson(cr.spirv, req.stage);

    EXPECT_TRUE(rr.ok()) << rr.error;
    EXPECT_FALSE(rr.json.empty());
    EXPECT_NE(rr.json.find("uniform_buffer"), std::string::npos);
    EXPECT_NE(rr.json.find("Matrices"),       std::string::npos);
}

TEST(Reflection, EmptySpirvReturnsError) {
    auto reflector = vne::sc::ShaderCompilerFactory::createReflector();
    auto rr        = reflector->reflectToJson({}, vne::sc::ShaderStage::eVertex);
    EXPECT_FALSE(rr.ok());
    EXPECT_EQ(rr.code, vne::sc::ResultCode::eReflectionFailed);
}

// ─────────────────────────────────────────────────────────────────────────────
// Cross-compilation
// ─────────────────────────────────────────────────────────────────────────────

TEST(CrossCompile, VertexToMslProducesNonEmptySource) {
    auto fe = vne::sc::ShaderCompilerFactory::createFrontEnd(vne::sc::SourceLang::eGLSL);
    ASSERT_TRUE(fe->isAvailable());

    vne::sc::CompileRequest req;
    req.source = kVertGlsl;
    req.stage  = vne::sc::ShaderStage::eVertex;
    auto cr    = fe->compile(req);
    ASSERT_TRUE(cr.ok());

    auto cc_compiler = vne::sc::ShaderCompilerFactory::createCrossCompiler();
    vne::sc::CrossCompileRequest ccr;
    ccr.spirv  = cr.spirv;
    ccr.target = vne::sc::CrossTarget::eMSL;
    ccr.stage  = vne::sc::ShaderStage::eVertex;

    auto ccres = cc_compiler->crossCompile(ccr);
    EXPECT_TRUE(ccres.ok()) << ccres.error;
    EXPECT_FALSE(ccres.source.empty());
    EXPECT_NE(ccres.source.find("metal"), std::string::npos);
}

TEST(CrossCompile, WgslReturnsUnavailable) {
    auto cc_compiler = vne::sc::ShaderCompilerFactory::createCrossCompiler();
    vne::sc::CrossCompileRequest ccr;
    ccr.spirv  = {0x07230203u};  // minimal non-empty SPIR-V
    ccr.target = vne::sc::CrossTarget::eWGSL;
    ccr.stage  = vne::sc::ShaderStage::eVertex;

    auto ccres = cc_compiler->crossCompile(ccr);
    EXPECT_FALSE(ccres.ok());
    EXPECT_EQ(ccres.code, vne::sc::ResultCode::eUnavailable);
}

// ─────────────────────────────────────────────────────────────────────────────
// Full pipeline
// ─────────────────────────────────────────────────────────────────────────────

TEST(Pipeline, FullBuildProducesValidArtifact) {
    auto builder = vne::sc::ShaderCompilerFactory::createPipelineBuilder(
        vne::sc::SourceLang::eGLSL);

    vne::sc::PipelineBuildDesc desc;
    desc.name      = "test_pipeline";
    desc.validate  = false;
    desc.use_cache = false;
    desc.targets   = {vne::sc::CrossTarget::eMSL};

    vne::sc::CompileRequest vert;
    vert.source = kVertGlsl;
    vert.stage  = vne::sc::ShaderStage::eVertex;
    desc.stages.push_back(vert);

    vne::sc::CompileRequest frag;
    frag.source = kFragGlsl;
    frag.stage  = vne::sc::ShaderStage::eFragment;
    desc.stages.push_back(frag);

    auto result = builder->build(desc);
    EXPECT_TRUE(result.ok()) << result.error;
    EXPECT_TRUE(result.artifact.isValid());
    EXPECT_EQ(result.artifact.stages.size(), 2u);

    const auto* vert_stage = result.artifact.findStage(vne::sc::ShaderStage::eVertex);
    ASSERT_NE(vert_stage, nullptr);
    EXPECT_FALSE(vert_stage->spirv.empty());
    EXPECT_FALSE(vert_stage->reflection_json.empty());

    const auto* msl = vert_stage->findCrossCompiled(vne::sc::CrossTarget::eMSL);
    ASSERT_NE(msl, nullptr);
    EXPECT_FALSE(msl->source.empty());
    EXPECT_FALSE(msl->entry_point.empty());
}

TEST(Pipeline, CacheHitSkipsRecompilation) {
    namespace fs = std::filesystem;
    auto tmp = fs::temp_directory_path() / "vnesc_cache_test_pipeline";
    fs::remove_all(tmp);

    auto builder = vne::sc::ShaderCompilerFactory::createPipelineBuilder(
        vne::sc::SourceLang::eGLSL);

    vne::sc::PipelineBuildDesc desc;
    desc.name      = "cached_pipeline";
    desc.validate  = false;
    desc.use_cache = true;
    desc.cache_dir = tmp.string();
    desc.targets   = {vne::sc::CrossTarget::eMSL};

    vne::sc::CompileRequest req;
    req.source = kVertGlsl;
    req.stage  = vne::sc::ShaderStage::eVertex;
    desc.stages.push_back(req);

    // First build — populates the cache.
    auto r1 = builder->build(desc);
    ASSERT_TRUE(r1.ok()) << r1.error;

    // Second build — should be a cache hit (same artifact, no compile).
    auto r2 = builder->build(desc);
    ASSERT_TRUE(r2.ok()) << r2.error;

    EXPECT_EQ(r1.artifact.stages[0].spirv,
              r2.artifact.stages[0].spirv);

    fs::remove_all(tmp);
}

#endif  // VNE_SC_GLSLANG_ENABLED
