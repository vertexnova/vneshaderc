#pragma once
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

/**
 * @file fixtures/glslang_frontend_test_fixture.h — base fixture for GlslangFrontEnd tests.
 */

#include "fixtures/shader_sources_fixture.h"

namespace vne::sc::test {

/**
 * @brief Base for tests that require @ref GlslangFrontEnd.
 * @details Skips when @c VNE_SC_GLSLANG_ENABLED is absent.
 */
class GlslangFrontEndTestFixture : public ShaderSourcesFixture {
   protected:
    void SetUp() override {
#ifndef VNE_SC_GLSLANG_ENABLED
        GTEST_SKIP() << "glslang front-end not enabled (configure with -DVNE_SC_GLSLANG=ON)";
#endif
    }

    static std::shared_ptr<IShaderFrontEnd> requireGlslangFrontEnd() {
        auto fe = ShaderCompilerFactory::createFrontEnd(SourceLang::eGLSL);
        if (!fe || !fe->isAvailable()) {
            ADD_FAILURE() << "GlslangFrontEnd unavailable";
            return nullptr;
        }
        return fe;
    }
};

}  // namespace vne::sc::test
