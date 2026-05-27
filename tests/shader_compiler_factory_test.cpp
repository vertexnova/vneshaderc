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

class ShaderCompilerFactoryTest : public ::testing::Test {};

TEST_F(ShaderCompilerFactoryTest, GlslangFrontEndAvailability) {
    auto fe = ShaderCompilerFactory::createFrontEnd(SourceLang::eGLSL);
    ASSERT_NE(fe, nullptr);
#ifdef VNE_SC_GLSLANG_ENABLED
    EXPECT_TRUE(fe->isAvailable());
#else
    EXPECT_FALSE(fe->isAvailable());
#endif
}

TEST_F(ShaderCompilerFactoryTest, DxcFrontEndIsNotAvailable) {
    auto fe = ShaderCompilerFactory::createFrontEnd(SourceLang::eHLSL);
    ASSERT_NE(fe, nullptr);
    EXPECT_FALSE(fe->isAvailable());
    auto r = fe->compile({});
    EXPECT_FALSE(r.ok());
    EXPECT_EQ(r.code, ResultCode::eUnavailable);
}

TEST_F(ShaderCompilerFactoryTest, SlangFrontEndIsNotAvailable) {
    auto fe = ShaderCompilerFactory::createFrontEnd(SourceLang::eSlang);
    ASSERT_NE(fe, nullptr);
    EXPECT_FALSE(fe->isAvailable());
}

TEST_F(ShaderCompilerFactoryTest, CrossCompilerIsAvailable) {
    auto cc = ShaderCompilerFactory::createCrossCompiler();
    ASSERT_NE(cc, nullptr);
    EXPECT_TRUE(cc->isAvailable());
}

}  // namespace vne::sc
