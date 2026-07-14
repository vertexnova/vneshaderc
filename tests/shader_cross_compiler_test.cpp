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

class ShaderCrossCompilerTest : public GlslangFrontEndTestFixture {};

TEST_F(ShaderCrossCompilerTest, VertexToMslProducesNonEmptySource) {
    auto fe = requireGlslangFrontEnd();
    ASSERT_NE(fe, nullptr);

    auto cr = fe->compile(makeVertexRequest());
    ASSERT_TRUE(cr.ok());

    auto cc = ShaderCompilerFactory::createCrossCompiler();
    ASSERT_NE(cc, nullptr);
    CrossCompileRequest ccr;
    ccr.spirv = cr.spirv;
    ccr.target = CrossTarget::eMSL;
    ccr.stage = ShaderStage::eVertex;

    auto ccres = cc->crossCompile(ccr);
    EXPECT_TRUE(ccres.ok()) << ccres.error;
    EXPECT_FALSE(ccres.source.empty());
    EXPECT_NE(ccres.source.find("metal"), std::string::npos);
}

TEST_F(ShaderCrossCompilerTest, WgslReturnsUnavailableWithoutTint) {
#ifndef VNE_SC_TINT_ENABLED
    auto cc = ShaderCompilerFactory::createCrossCompiler();
    ASSERT_NE(cc, nullptr);
    CrossCompileRequest ccr;
    ccr.spirv = {0x07230203u};
    ccr.target = CrossTarget::eWGSL;
    ccr.stage = ShaderStage::eVertex;

    auto ccres = cc->crossCompile(ccr);
    EXPECT_FALSE(ccres.ok());
    EXPECT_EQ(ccres.code, ResultCode::eUnavailable);
#else
    GTEST_SKIP() << "Tint enabled - see TintCrossCompilerTest";
#endif
}

}  // namespace vne::sc::test
