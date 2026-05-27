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

namespace vne::sc::test {

class SpirvCrossReflectorTest : public GlslangFrontEndTestFixture {};

TEST_F(SpirvCrossReflectorTest, VertexShaderWithUboProducesBindings) {
    auto fe = requireGlslangFrontEnd();
    ASSERT_NE(fe, nullptr);

    auto cr = fe->compile(makeVertexRequest());
    ASSERT_TRUE(cr.ok());

    auto reflector = ShaderCompilerFactory::createReflector();
    auto rr = reflector->reflect(cr.spirv, ShaderStage::eVertex, {CrossTarget::eMSL});

    EXPECT_TRUE(rr.ok()) << rr.error;
    EXPECT_FALSE(rr.reflection.bindings.empty());
    bool found_ubo = false;
    for (const auto& b : rr.reflection.bindings) {
        if (b.type == ReflectedResourceType::eUniformBuffer && b.name == "Matrices") {
            found_ubo = true;
            EXPECT_EQ(b.set, 0u);
            EXPECT_EQ(b.binding, 0u);
            EXPECT_TRUE(b.slots.metal.has_value());
        }
    }
    EXPECT_TRUE(found_ubo);
}

TEST_F(SpirvCrossReflectorTest, EmptySpirvReturnsError) {
    auto reflector = ShaderCompilerFactory::createReflector();
    auto rr = reflector->reflect({}, ShaderStage::eVertex);
    EXPECT_FALSE(rr.ok());
    EXPECT_EQ(rr.code, ResultCode::eReflectionFailed);
}

}  // namespace vne::sc::test
