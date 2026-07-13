/* ---------------------------------------------------------------------
 * Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License")
 * ---------------------------------------------------------------------- */

#include <gtest/gtest.h>

#include "vertexnova/sc/gpu_layout_tools.h"
#include "vertexnova/sc/shader_pipeline_spec.h"

#include <chrono>
#include <fstream>
#include <string>

namespace vne::sc {

class GpuLayoutToolsTest : public ::testing::Test {
   protected:
    std::filesystem::path temp_dir;

    void SetUp() override {
        const auto stamp = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        temp_dir = std::filesystem::temp_directory_path()
                   / ("vnesc_gpu_layout_test_" + stamp + "_" + std::to_string(reinterpret_cast<uintptr_t>(this)));
        std::filesystem::create_directories(temp_dir);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(temp_dir, ec);
    }

    static StageArtifact makeFragmentStageWithUbo(const std::string& name, uint32_t size) {
        StageArtifact stage;
        stage.stage = ShaderStage::eFragment;
        stage.reflection.stage = ShaderStage::eFragment;
        ReflectedBindingInfo ubo;
        ubo.name = name;
        ubo.type = ReflectedResourceType::eUniformBuffer;
        ubo.set = 0;
        ubo.binding = 0;
        ReflectedStructMember member;
        member.name = "data";
        member.offset = 0;
        member.size = size;
        member.array_count = 1;
        ubo.struct_members.push_back(member);
        stage.reflection.bindings.push_back(std::move(ubo));
        return stage;
    }
};

#ifdef VNE_SC_JSON_ENABLED

TEST_F(GpuLayoutToolsTest, MergeRegistriesDetectsDuplicates) {
    const auto path_a = temp_dir / "a.layout.json";
    const auto path_b = temp_dir / "b.layout.json";
    {
        std::ofstream a{path_a};
        a << R"({"uniform_buffers":[{"name":"ClipPlanes","size":112}]})";
    }
    {
        std::ofstream b{path_b};
        b << R"({"uniform_buffers":[{"name":"ClipPlanes","size":112}]})";
    }

    GpuLayoutRegistry merged;
    std::string error;
    EXPECT_FALSE(mergeGpuLayoutRegistries({path_a, path_b}, {}, merged, error));
    EXPECT_NE(error.find("duplicate"), std::string::npos);
}

TEST_F(GpuLayoutToolsTest, MergeInlineAndFileRegistry) {
    const auto path = temp_dir / "shared.layout.json";
    {
        std::ofstream f{path};
        f << R"({"uniform_buffers":[{"name":"LightBuffer","size":2464}]})";
    }

    GpuLayoutRegistry merged;
    std::string error;
    ExpectedUniformBufferLayout pass_ubo;
    pass_ubo.block_name = "PhongMaterial";
    pass_ubo.total_size = 64;
    ASSERT_TRUE(mergeGpuLayoutRegistries({path}, {pass_ubo}, merged, error));
    ASSERT_EQ(merged.uniform_buffers.size(), 2u);
}

TEST_F(GpuLayoutToolsTest, LoadRegistryRejectsMalformedEntries) {
    const auto path = temp_dir / "bad.layout.json";
    {
        std::ofstream f{path};
        f << R"({"uniform_buffers":[{"name":"NoSize"}]})";
    }
    GpuLayoutRegistry registry;
    std::string error;
    EXPECT_FALSE(loadGpuLayoutRegistry(path, registry, error));
    EXPECT_FALSE(error.empty());
    EXPECT_NE(error.find("size"), std::string::npos);
}

TEST_F(GpuLayoutToolsTest, LoadRegistryAcceptsEmptyUniformBuffers) {
    const auto path = temp_dir / "empty.layout.json";
    {
        std::ofstream f{path};
        f << R"({"uniform_buffers":[]})";
    }
    GpuLayoutRegistry registry;
    std::string error;
    EXPECT_TRUE(loadGpuLayoutRegistry(path, registry, error));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(registry.uniform_buffers.empty());
}

TEST_F(GpuLayoutToolsTest, BuildBindingDeclsComposeOnly) {
    ShaderArtifact artifact;
    EmitBindingDeclOptions options;
    options.compose_includes = {"gen/common/bindings/clip_planes_ubo.glsl"};

    const std::string glsl = buildBindingDeclsGlsl(artifact, options);
    EXPECT_NE(glsl.find("AUTO-GENERATED"), std::string::npos);
    EXPECT_NE(glsl.find("#include \"gen/common/bindings/clip_planes_ubo.glsl\""), std::string::npos);
}

TEST_F(GpuLayoutToolsTest, EmitPreservesReflectedArrayExtents) {
    ShaderArtifact artifact;
    StageArtifact stage;
    stage.stage = ShaderStage::eFragment;
    stage.reflection.stage = ShaderStage::eFragment;

    ReflectedBindingInfo ubo;
    ubo.name = "MaterialUBO";
    ubo.type = ReflectedResourceType::eUniformBuffer;
    ubo.set = 0;
    ubo.binding = 0;
    {
        ReflectedStructMember scalar;
        scalar.name = "color";
        scalar.type_name = "vec4";
        scalar.array_count = 1;
        ubo.struct_members.push_back(scalar);

        ReflectedStructMember fixed;
        fixed.name = "lights";
        fixed.type_name = "vec4";
        fixed.array_count = 4;
        ubo.struct_members.push_back(fixed);
    }
    stage.reflection.bindings.push_back(ubo);

    ReflectedBindingInfo ssbo;
    ssbo.name = "ParticleSSBO";
    ssbo.type = ReflectedResourceType::eStorageBuffer;
    ssbo.set = 0;
    ssbo.binding = 1;
    {
        ReflectedStructMember fixed;
        fixed.name = "positions";
        fixed.type_name = "vec4";
        fixed.array_count = 8;
        ssbo.struct_members.push_back(fixed);

        ReflectedStructMember unsized;
        unsized.name = "data";
        unsized.type_name = "float";
        unsized.array_count = 0;
        ssbo.struct_members.push_back(unsized);

        ReflectedStructMember scalar;
        scalar.name = "count";
        scalar.type_name = "float";
        scalar.array_count = 1;
        ssbo.struct_members.push_back(scalar);
    }
    stage.reflection.bindings.push_back(ssbo);
    artifact.stages.push_back(std::move(stage));

    const std::string glsl = buildBindingDeclsGlsl(artifact, {});
    EXPECT_NE(glsl.find("vec4 color;"), std::string::npos);
    EXPECT_NE(glsl.find("vec4 lights[4];"), std::string::npos);
    EXPECT_NE(glsl.find("vec4 positions[8];"), std::string::npos);
    EXPECT_NE(glsl.find("float data[];"), std::string::npos);
    EXPECT_NE(glsl.find("float count;"), std::string::npos);
    EXPECT_EQ(glsl.find("positions[]"), std::string::npos);
    EXPECT_EQ(glsl.find("lights[]"), std::string::npos);
}

TEST_F(GpuLayoutToolsTest, ValidateGpuLayoutsReportsMissingUbo) {
    ShaderArtifact artifact;
    artifact.stages.push_back(makeFragmentStageWithUbo("PresentUBO", 16));

    GpuLayoutRegistry registry;
    ExpectedUniformBufferLayout expected;
    expected.block_name = "MissingUBO";
    expected.total_size = 16;
    registry.uniform_buffers.push_back(expected);

    std::string error;
    EXPECT_FALSE(validateGpuLayouts(artifact, registry, error));
    EXPECT_NE(error.find("missing UBO"), std::string::npos);
    EXPECT_NE(error.find("MissingUBO"), std::string::npos);
}

TEST_F(GpuLayoutToolsTest, ValidateGpuLayoutsReportsSizeMismatch) {
    ShaderArtifact artifact;
    artifact.stages.push_back(makeFragmentStageWithUbo("PhongMaterial", 64));

    GpuLayoutRegistry registry;
    ExpectedUniformBufferLayout expected;
    expected.block_name = "PhongMaterial";
    expected.total_size = 128;
    registry.uniform_buffers.push_back(expected);

    std::string error;
    EXPECT_FALSE(validateGpuLayouts(artifact, registry, error));
    EXPECT_NE(error.find("size mismatch"), std::string::npos);
}

TEST_F(GpuLayoutToolsTest, ValidateGpuLayoutsFindsUboInNonFragmentStage) {
    ShaderArtifact artifact;
    StageArtifact vertex;
    vertex.stage = ShaderStage::eVertex;
    vertex.reflection.stage = ShaderStage::eVertex;
    ReflectedBindingInfo ubo;
    ubo.name = "VertexOnlyUBO";
    ubo.type = ReflectedResourceType::eUniformBuffer;
    ubo.set = 0;
    ubo.binding = 0;
    ReflectedStructMember member;
    member.name = "mvp";
    member.offset = 0;
    member.size = 64;
    member.array_count = 1;
    ubo.struct_members.push_back(member);
    vertex.reflection.bindings.push_back(std::move(ubo));
    artifact.stages.push_back(std::move(vertex));

    StageArtifact fragment;
    fragment.stage = ShaderStage::eFragment;
    fragment.reflection.stage = ShaderStage::eFragment;
    artifact.stages.push_back(std::move(fragment));

    GpuLayoutRegistry registry;
    ExpectedUniformBufferLayout expected;
    expected.block_name = "VertexOnlyUBO";
    expected.total_size = 64;
    registry.uniform_buffers.push_back(expected);

    std::string error;
    EXPECT_TRUE(validateGpuLayouts(artifact, registry, error)) << error;
}

TEST_F(GpuLayoutToolsTest, BuildBindingDeclsRespectsSkipAndInclude) {
    ShaderArtifact artifact;
    StageArtifact stage;
    stage.stage = ShaderStage::eFragment;
    stage.reflection.stage = ShaderStage::eFragment;

    ReflectedBindingInfo keep;
    keep.name = "KeepUBO";
    keep.type = ReflectedResourceType::eUniformBuffer;
    keep.set = 0;
    keep.binding = 0;
    stage.reflection.bindings.push_back(keep);

    ReflectedBindingInfo skip;
    skip.name = "SkipUBO";
    skip.type = ReflectedResourceType::eUniformBuffer;
    skip.set = 0;
    skip.binding = 1;
    stage.reflection.bindings.push_back(skip);

    ReflectedBindingInfo sampler;
    sampler.name = "albedo_map";
    sampler.type = ReflectedResourceType::eSampledImage;
    sampler.set = 0;
    sampler.binding = 2;
    stage.reflection.bindings.push_back(sampler);
    artifact.stages.push_back(std::move(stage));

    EmitBindingDeclOptions skip_opts;
    skip_opts.skip_blocks = {"SkipUBO"};
    const std::string skipped = buildBindingDeclsGlsl(artifact, skip_opts);
    EXPECT_NE(skipped.find("KeepUBO"), std::string::npos);
    EXPECT_EQ(skipped.find("SkipUBO"), std::string::npos);
    EXPECT_NE(skipped.find("albedo_map"), std::string::npos);

    EmitBindingDeclOptions include_opts;
    include_opts.include_blocks = {"KeepUBO", "AlbedoMap"};
    const std::string included = buildBindingDeclsGlsl(artifact, include_opts);
    EXPECT_NE(included.find("KeepUBO"), std::string::npos);
    EXPECT_EQ(included.find("SkipUBO"), std::string::npos);
    EXPECT_NE(included.find("albedo_map"), std::string::npos);
}

TEST_F(GpuLayoutToolsTest, ParsesNewManifestFields) {
    const char* json = R"({
        "name": "test",
        "source_lang": "glsl",
        "validate_layout": true,
        "layout_registries": ["shared.layout.json"],
        "uniform_buffers": [{"name": "PhongMaterial", "size": 64}],
        "emit_binding_decls": "../gen/test_bindings.glsl",
        "emit_binding_decls_compose": ["gen/common/bindings/clip_planes_ubo.glsl"],
        "emit_binding_decls_include": ["PhongMaterial"],
        "emit_bindings_stage": "fragment",
        "stages": [{"stage": "fragment", "file": "test.frag.glsl"}]
    })";

    auto spec = parseShaderPipelineSpecJson(json);
    ASSERT_TRUE(spec.has_value());
    EXPECT_TRUE(spec->validate_layout);
    ASSERT_EQ(spec->layout_registries.size(), 1u);
    ASSERT_EQ(spec->uniform_buffers.size(), 1u);
    ASSERT_EQ(spec->emit_binding_decls_compose.size(), 1u);
    ASSERT_EQ(spec->emit_binding_decls_include.size(), 1u);
    EXPECT_EQ(spec->emit_bindings_stage, "fragment");
}

TEST_F(GpuLayoutToolsTest, SpecRejectsInvalidUniformBufferSize) {
    const char* json = R"({
        "name": "test",
        "uniform_buffers": [{"name": "Bad", "size": -1}],
        "stages": [{"stage": "fragment", "file": "test.frag.glsl"}]
    })";

    auto spec = parseShaderPipelineSpecJson(json);
    ASSERT_TRUE(spec.has_value());
    EXPECT_TRUE(spec->uniform_buffers.empty());
    ASSERT_FALSE(spec->errors.empty());
    EXPECT_NE(spec->errors[0].find("invalid size"), std::string::npos);
}

TEST_F(GpuLayoutToolsTest, SpecRejectsUnknownBindingsStage) {
    const char* json = R"({
        "name": "test",
        "emit_bindings_stage": "geometry",
        "stages": [{"stage": "fragment", "file": "test.frag.glsl"}]
    })";

    auto spec = parseShaderPipelineSpecJson(json);
    ASSERT_TRUE(spec.has_value());
    EXPECT_EQ(spec->emit_bindings_stage, "fragment");
    ASSERT_FALSE(spec->errors.empty());
    EXPECT_NE(spec->errors[0].find("emit_bindings_stage"), std::string::npos);
}

TEST_F(GpuLayoutToolsTest, SpecWarnsOnDeprecatedLayoutRegistry) {
    const char* json = R"({
        "name": "test",
        "layout_registry": "legacy.layout.json",
        "stages": [{"stage": "fragment", "file": "test.frag.glsl"}]
    })";

    auto spec = parseShaderPipelineSpecJson(json);
    ASSERT_TRUE(spec.has_value());
    ASSERT_EQ(spec->layout_registries.size(), 1u);
    EXPECT_EQ(spec->layout_registries[0], "legacy.layout.json");
    ASSERT_FALSE(spec->errors.empty());
    EXPECT_NE(spec->errors[0].find("deprecated"), std::string::npos);
}

#else

TEST_F(GpuLayoutToolsTest, SkippedWithoutJson) {
    GTEST_SKIP() << "JSON support not enabled";
}

#endif

}  // namespace vne::sc
