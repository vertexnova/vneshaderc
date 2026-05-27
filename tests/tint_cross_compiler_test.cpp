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

class TintCrossCompilerTest : public GlslangFrontEndTestFixture {};

TEST_F(TintCrossCompilerTest, WgslProducesNonEmptySource) {
#ifndef VNE_SC_TINT_ENABLED
    GTEST_SKIP() << "Tint not enabled (configure with -DVNE_SC_TINT=ON)";
#endif

    auto fe = requireGlslangFrontEnd();
    ASSERT_NE(fe, nullptr);

    CompileRequest req;
    req.source = kTexturedFragGlsl;
    req.stage = ShaderStage::eFragment;
    auto cr = fe->compile(req);
    ASSERT_TRUE(cr.ok()) << (cr.errors.empty() ? "" : cr.errors[0]);

    auto cc = ShaderCompilerFactory::createCrossCompiler();
    CrossCompileRequest ccr;
    ccr.spirv = cr.spirv;
    ccr.target = CrossTarget::eWGSL;
    ccr.stage = ShaderStage::eFragment;
    auto ccres = cc->crossCompile(ccr);
    EXPECT_TRUE(ccres.ok()) << ccres.error;
    EXPECT_FALSE(ccres.source.empty());
    EXPECT_NE(ccres.source.find("@group"), std::string::npos);
}

}  // namespace vne::sc::test
