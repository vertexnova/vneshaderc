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

class ShaderBundleTest : public ShaderPipelineBuilderTestFixture {};

TEST_F(ShaderBundleTest, WriteAndReadHeaderRoundTrip) {
    auto builder = makeGlslPipelineBuilder();
    ASSERT_NE(builder, nullptr);

    auto desc = makeVertMslDesc("bundle_test", false);
    auto result = builder->build(desc);
    ASSERT_TRUE(result.ok()) << result.error;

    auto tmp = tempDir("vnesc_shader_bundle_test");
    ASSERT_TRUE(writeShaderBundle(result.artifact, tmp));

    auto header = readShaderBundleHeader(tmp);
    ASSERT_TRUE(header.has_value());
    EXPECT_EQ(header->name, "bundle_test");
    ASSERT_EQ(header->stages.size(), 1u);
    EXPECT_FALSE(header->stages[0].spirv_file.empty());

    auto reflection = readShaderBundleReflection(tmp);
    ASSERT_TRUE(reflection.has_value());
    EXPECT_FALSE(reflection->stages.empty());

    removeTempDir(tmp);
}

}  // namespace vne::sc::test
