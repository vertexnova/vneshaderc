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

#include "fixtures/shader_pipeline_builder_test_fixture.h"

namespace vne::sc::test {

class ShaderPipelineBuilderTest : public ShaderPipelineBuilderTestFixture {};

TEST_F(ShaderPipelineBuilderTest, FullBuildProducesValidArtifact) {
    auto builder = makeGlslPipelineBuilder();
    ASSERT_NE(builder, nullptr);

    auto desc = makeVertFragMslDesc("test_pipeline");
    auto result = builder->build(desc);
    EXPECT_TRUE(result.ok()) << result.error;
    EXPECT_TRUE(result.artifact.isValid());
    EXPECT_EQ(result.artifact.stages.size(), 2u);

    const auto* vert_stage = result.artifact.findStage(ShaderStage::eVertex);
    ASSERT_NE(vert_stage, nullptr);
    EXPECT_FALSE(vert_stage->spirv.empty());
    EXPECT_FALSE(vert_stage->reflection.bindings.empty());

    const auto* msl = vert_stage->findCrossCompiled(CrossTarget::eMSL);
    ASSERT_NE(msl, nullptr);
    EXPECT_FALSE(msl->source.empty());
    EXPECT_FALSE(msl->entry_point.empty());
}

TEST_F(ShaderPipelineBuilderTest, CacheHitSkipsRecompilation) {
    auto tmp = tempDir("vnesc_shader_pipeline_builder_cache");
    auto builder = makeGlslPipelineBuilder();
    ASSERT_NE(builder, nullptr);

    auto desc = makeVertMslDesc("cached_pipeline", true, tmp.string());
    auto r1 = builder->build(desc);
    ASSERT_TRUE(r1.ok()) << r1.error;

    auto r2 = builder->build(desc);
    ASSERT_TRUE(r2.ok()) << r2.error;
    ASSERT_FALSE(r1.artifact.stages.empty());
    ASSERT_FALSE(r2.artifact.stages.empty());
    EXPECT_EQ(r1.artifact.stages[0].spirv, r2.artifact.stages[0].spirv);

    removeTempDir(tmp);
}

}  // namespace vne::sc::test
