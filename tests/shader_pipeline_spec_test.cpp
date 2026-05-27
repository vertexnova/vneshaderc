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

#include "vertexnova/sc/vnesc.h"

namespace vne::sc {

class ShaderPipelineSpecTest : public ::testing::Test {};

TEST_F(ShaderPipelineSpecTest, ParsesBasicJson) {
    const char* json = R"({
        "name": "scene",
        "source_lang": "GLSL",
        "targets": ["MSL"],
        "stages": [
            { "stage": "vertex", "file": "scene.vert", "entry_point": "main" }
        ]
    })";
#ifdef VNE_SC_JSON_ENABLED
    auto spec = parseShaderPipelineSpecJson(json);
    ASSERT_TRUE(spec.has_value());
    EXPECT_EQ(spec->name, "scene");
    EXPECT_EQ(spec->stages.size(), 1u);
    EXPECT_EQ(spec->targets.size(), 1u);
    EXPECT_EQ(spec->stages[0].entry_point, "main");
#else
    (void)json;
    GTEST_SKIP() << "JSON support not enabled";
#endif
}

TEST_F(ShaderPipelineSpecTest, AcceptsLegacyEntryKey) {
    const char* json = R"({
        "name": "legacy",
        "source_lang": "GLSL",
        "stages": [
            { "stage": "vertex", "file": "legacy.vert", "entry": "vs_main" }
        ]
    })";
#ifdef VNE_SC_JSON_ENABLED
    auto spec = parseShaderPipelineSpecJson(json);
    ASSERT_TRUE(spec.has_value());
    EXPECT_EQ(spec->stages[0].entry_point, "vs_main");
#else
    (void)json;
    GTEST_SKIP() << "JSON support not enabled";
#endif
}

TEST_F(ShaderPipelineSpecTest, ParsesIncludePaths) {
    const char* json = R"({
        "name": "includes_test",
        "source_lang": "GLSL",
        "include_paths": ["../common/glsl", "../../shared"],
        "stages": [
            { "stage": "vertex", "file": "test.vert" }
        ]
    })";
#ifdef VNE_SC_JSON_ENABLED
    auto spec = parseShaderPipelineSpecJson(json);
    ASSERT_TRUE(spec.has_value());
    ASSERT_EQ(spec->include_paths.size(), 2u);
    EXPECT_EQ(spec->include_paths[0], "../common/glsl");
    EXPECT_EQ(spec->include_paths[1], "../../shared");
#else
    (void)json;
    GTEST_SKIP() << "JSON support not enabled";
#endif
}

TEST_F(ShaderPipelineSpecTest, RejectsMissingName) {
#ifdef VNE_SC_JSON_ENABLED
    const char* json = R"({
        "stages": [{ "stage": "vertex", "file": "test.vert" }]
    })";
    auto spec = parseShaderPipelineSpecJson(json);
    EXPECT_FALSE(spec.has_value());
#else
    GTEST_SKIP() << "JSON support not enabled";
#endif
}

TEST_F(ShaderPipelineSpecTest, RejectsMissingStages) {
#ifdef VNE_SC_JSON_ENABLED
    const char* json = R"({ "name": "no_stages" })";
    auto spec = parseShaderPipelineSpecJson(json);
    EXPECT_FALSE(spec.has_value());
#else
    GTEST_SKIP() << "JSON support not enabled";
#endif
}

}  // namespace vne::sc
