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

#include <gtest/gtest.h>

#include "fixtures/glslang_frontend_test_fixture.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace vne::sc::test {

class ShaderPipelineSpecIncludesTest : public GlslangFrontEndTestFixture {};

// ── include_paths → toBuildDesc wiring ───────────────────────────────────────

TEST_F(ShaderPipelineSpecIncludesTest, IncludePathsResolvedRelativeToSpecDir) {
#ifndef VNE_SC_JSON_ENABLED
    GTEST_SKIP() << "JSON support not enabled";
#else
    const char* json = R"({
        "name": "inc_wiring",
        "source_lang": "GLSL",
        "include_paths": ["../common/glsl", "../../shared"],
        "stages": [
            { "stage": "vertex", "file": "test.vert" }
        ]
    })";
    auto spec = parseShaderPipelineSpecJson(json);
    ASSERT_TRUE(spec.has_value());
    ASSERT_EQ(spec->include_paths.size(), 2u);

    // Use an absolute fake spec_dir so we can check the resolved path
    const std::filesystem::path spec_dir = "/tmp/shaders/src";
    auto desc = spec->toBuildDesc(spec_dir);

    ASSERT_EQ(desc.stages.size(), 1u);
    const auto& dirs = desc.stages[0].include_dirs;
    ASSERT_EQ(dirs.size(), 2u);
    EXPECT_EQ(dirs[0], (spec_dir / "../common/glsl").string());
    EXPECT_EQ(dirs[1], (spec_dir / "../../shared").string());
#endif
}

TEST_F(ShaderPipelineSpecIncludesTest, EmptyIncludePathsProducesNoIncludeDirs) {
#ifndef VNE_SC_JSON_ENABLED
    GTEST_SKIP() << "JSON support not enabled";
#else
    const char* json = R"({
        "name": "no_includes",
        "source_lang": "GLSL",
        "stages": [
            { "stage": "vertex", "file": "test.vert" }
        ]
    })";
    auto spec = parseShaderPipelineSpecJson(json);
    ASSERT_TRUE(spec.has_value());
    EXPECT_TRUE(spec->include_paths.empty());

    auto desc = spec->toBuildDesc();
    ASSERT_EQ(desc.stages.size(), 1u);
    EXPECT_TRUE(desc.stages[0].include_dirs.empty());
#endif
}

// ── end-to-end: glslang compiles a shader that uses #include ─────────────────

TEST_F(ShaderPipelineSpecIncludesTest, GlslangCompilesShaderWithIncludedFile) {
    auto fe = requireGlslangFrontEnd();
    ASSERT_NE(fe, nullptr);

    // Write a tiny helper into a temp directory
    const auto inc_dir = std::filesystem::temp_directory_path() / "vnesc_inc_test";
    std::filesystem::create_directories(inc_dir);
    {
        std::ofstream f(inc_dir / "lighting.glsl");
        f << "vec3 applyDiffuse(vec3 n, vec3 l) { return max(dot(n, l), 0.0) * l; }\n";
    }

    const std::string vert_src = R"(
        #version 450
        #extension GL_GOOGLE_include_directive : require
        #include "lighting.glsl"
        layout(location = 0) in vec3 aPos;
        void main() {
            vec3 lit = applyDiffuse(normalize(aPos), vec3(0.0, 1.0, 0.0));
            gl_Position = vec4(lit, 1.0);
        }
    )";

    // Should succeed with the include dir
    CompileRequest req;
    req.source = vert_src;
    req.stage = ShaderStage::eVertex;
    req.include_dirs = {inc_dir.string()};

    auto cr = fe->compile(req);
    EXPECT_TRUE(cr.ok()) << (cr.errors.empty() ? "" : cr.errors[0]);
    EXPECT_FALSE(cr.spirv.empty());

    // Should fail without the include dir
    req.include_dirs.clear();
    auto cr_fail = fe->compile(req);
    EXPECT_FALSE(cr_fail.ok());
}

}  // namespace vne::sc::test
