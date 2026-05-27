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
 * @file fixtures/shader_sources_fixture.h — shared GLSL sources for integration tests.
 */

#include <gtest/gtest.h>

#include "vertexnova/sc/vnesc.h"

namespace vne::sc::test {

/** Shared shader sources used by compile, reflection, pipeline, and bundle tests. */
class ShaderSourcesFixture : public ::testing::Test {
   protected:
    static constexpr const char* kVertGlsl = R"glsl(
        #version 450
        layout(location = 0) in  vec3 aPos;
        layout(location = 0) out vec4 vColor;
        layout(set = 0, binding = 0) uniform Matrices { mat4 mvp; } ubo;
        void main() {
            gl_Position = ubo.mvp * vec4(aPos, 1.0);
            vColor      = vec4(aPos, 1.0);
        }
    )glsl";

    static constexpr const char* kFragGlsl = R"glsl(
        #version 450
        layout(location = 0) in  vec4 vColor;
        layout(location = 0) out vec4 oColor;
        void main() { oColor = vColor; }
    )glsl";

    static constexpr const char* kTexturedFragGlsl = R"glsl(
        #version 450
        layout(location = 0) in vec2 inUV;
        layout(location = 0) out vec4 outColor;
        layout(set = 0, binding = 0) uniform texture2D albedoTex;
        layout(set = 0, binding = 1) uniform sampler albedoSampler;
        void main() {
            outColor = texture(sampler2D(albedoTex, albedoSampler), inUV);
        }
    )glsl";

    static CompileRequest makeVertexRequest() {
        CompileRequest req;
        req.source = kVertGlsl;
        req.stage = ShaderStage::eVertex;
        return req;
    }

    static CompileRequest makeFragmentRequest() {
        CompileRequest req;
        req.source = kFragGlsl;
        req.stage = ShaderStage::eFragment;
        return req;
    }
};

}  // namespace vne::sc::test
