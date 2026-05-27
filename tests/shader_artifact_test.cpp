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

class ShaderArtifactTest : public ::testing::Test {};

TEST_F(ShaderArtifactTest, InvalidWhenEmpty) {
    ShaderArtifact a;
    EXPECT_FALSE(a.isValid());
}

TEST_F(ShaderArtifactTest, InvalidWhenStageHasNoSpirv) {
    ShaderArtifact a;
    StageArtifact s;
    s.stage = ShaderStage::eVertex;
    a.stages.push_back(s);
    EXPECT_FALSE(a.isValid());
}

TEST_F(ShaderArtifactTest, ValidWhenAllStagesHaveSpirv) {
    ShaderArtifact a;
    StageArtifact s;
    s.stage = ShaderStage::eVertex;
    s.spirv = {0x07230203u, 0u};
    a.stages.push_back(s);
    EXPECT_TRUE(a.isValid());
}

TEST_F(ShaderArtifactTest, FindStageReturnsCorrectEntry) {
    ShaderArtifact a;
    StageArtifact vert, frag;
    vert.stage = ShaderStage::eVertex;
    vert.spirv = {1u};
    frag.stage = ShaderStage::eFragment;
    frag.spirv = {2u};
    a.stages.push_back(vert);
    a.stages.push_back(frag);

    ASSERT_NE(a.findStage(ShaderStage::eVertex), nullptr);
    ASSERT_NE(a.findStage(ShaderStage::eFragment), nullptr);
    EXPECT_EQ(a.findStage(ShaderStage::eCompute), nullptr);
    EXPECT_EQ(a.findStage(ShaderStage::eVertex)->spirv[0], 1u);
}

class StageArtifactTest : public ::testing::Test {};

TEST_F(StageArtifactTest, FindCrossCompiledReturnsCorrectEntry) {
    StageArtifact s;
    CrossCompiledSource msl;
    msl.target = CrossTarget::eMSL;
    msl.source = "// msl";
    msl.entry_point = "main0";
    s.cross_compiled.push_back(msl);

    ASSERT_NE(s.findCrossCompiled(CrossTarget::eMSL), nullptr);
    EXPECT_EQ(s.findCrossCompiled(CrossTarget::eWGSL), nullptr);
    EXPECT_EQ(s.findCrossCompiled(CrossTarget::eMSL)->entry_point, "main0");
}

}  // namespace vne::sc
