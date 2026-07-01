/* ---------------------------------------------------------------------
 * Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License")
 * ---------------------------------------------------------------------- */

#include <gtest/gtest.h>

#include "vertexnova/sc/gpu_layout_tools.h"
#include "vertexnova/sc/shader_pipeline_spec.h"

#include <fstream>

namespace vne::sc {

class GpuLayoutToolsTest : public ::testing::Test {
   protected:
    std::filesystem::path temp_dir;

    void SetUp() override {
        temp_dir = std::filesystem::temp_directory_path() / "vnesc_gpu_layout_test";
        std::filesystem::create_directories(temp_dir);
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

TEST_F(GpuLayoutToolsTest, BuildBindingDeclsComposeOnly) {
    ShaderArtifact artifact;
    EmitBindingDeclOptions options;
    options.compose_includes = {"gen/common/bindings/clip_planes_ubo.glsl"};

    const std::string glsl = buildBindingDeclsGlsl(artifact, options);
    EXPECT_NE(glsl.find("AUTO-GENERATED"), std::string::npos);
    EXPECT_NE(glsl.find("#include \"gen/common/bindings/clip_planes_ubo.glsl\""), std::string::npos);
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

#else

TEST_F(GpuLayoutToolsTest, SkippedWithoutJson) {
    GTEST_SKIP() << "JSON support not enabled";
}

#endif

}  // namespace vne::sc
