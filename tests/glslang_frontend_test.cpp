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

class GlslangFrontEndTest : public GlslangFrontEndTestFixture {};

TEST_F(GlslangFrontEndTest, VertexShaderProducesNonEmptySpirv) {
    auto fe = requireGlslangFrontEnd();
    ASSERT_NE(fe, nullptr);

    auto cr = fe->compile(makeVertexRequest());
    EXPECT_TRUE(cr.ok()) << (cr.errors.empty() ? "" : cr.errors[0]);
    EXPECT_FALSE(cr.spirv.empty());
}

TEST_F(GlslangFrontEndTest, FragmentShaderProducesNonEmptySpirv) {
    auto fe = requireGlslangFrontEnd();
    ASSERT_NE(fe, nullptr);

    auto cr = fe->compile(makeFragmentRequest());
    EXPECT_TRUE(cr.ok()) << (cr.errors.empty() ? "" : cr.errors[0]);
    EXPECT_FALSE(cr.spirv.empty());
}

TEST_F(GlslangFrontEndTest, InvalidGlslReturnsCompileFailed) {
    auto fe = requireGlslangFrontEnd();
    ASSERT_NE(fe, nullptr);

    CompileRequest req;
    req.source = "#version 450\nvoid main() { SYNTAX ERROR HERE }";
    req.stage = ShaderStage::eVertex;

    auto cr = fe->compile(req);
    EXPECT_FALSE(cr.ok());
    EXPECT_EQ(cr.code, ResultCode::eCompileFailed);
    EXPECT_FALSE(cr.errors.empty());
}

TEST_F(GlslangFrontEndTest, EmptyRequestReturnsInvalidArgument) {
    auto fe = requireGlslangFrontEnd();
    ASSERT_NE(fe, nullptr);

    auto cr = fe->compile({});
    EXPECT_FALSE(cr.ok());
    EXPECT_EQ(cr.code, ResultCode::eInvalidArgument);
}

TEST_F(GlslangFrontEndTest, MacrosAreInjectedIntoPreamble) {
    auto fe = requireGlslangFrontEnd();
    ASSERT_NE(fe, nullptr);

    CompileRequest req;
    req.source = R"glsl(
        #version 450
        #ifndef MY_VALUE
        #error MY_VALUE not defined
        #endif
        void main() {}
    )glsl";
    req.stage = ShaderStage::eVertex;
    req.macros = {{"MY_VALUE", "1"}};

    auto cr = fe->compile(req);
    EXPECT_TRUE(cr.ok()) << (cr.errors.empty() ? "" : cr.errors[0]);
}

}  // namespace vne::sc::test
