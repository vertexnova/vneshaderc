/* ---------------------------------------------------------------------
 * Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License")
 *
 * Author:    Ajeet Singh Yadav
 * Created:   July 2026
 *
 * Autodoc:   yes
 * ----------------------------------------------------------------------
 */

#include <gtest/gtest.h>

#include "vertexnova/sc/metal_binding_allocator.h"
#include "vertexnova/sc/vnesc.h"

#include <string>
#include <vector>

#if defined(VNE_SC_GLSLANG_ENABLED)

namespace vne::sc {
namespace {

std::vector<uint32_t> compileGlsl(const char* source, ShaderStage stage) {
    auto fe = ShaderCompilerFactory::createFrontEnd(SourceLang::eGLSL);
    CompileRequest req;
    req.source = source;
    req.stage = stage;
    auto cr = fe->compile(req);
    EXPECT_TRUE(cr.ok()) << (cr.errors.empty() ? "" : cr.errors.front());
    return cr.spirv;
}

}  // namespace

TEST(MetalBindingAllocatorTest, ProgramWideUnionIsDenseAndConsistent) {
    const char* vert = R"glsl(
        #version 450
        layout(set = 0, binding = 0) uniform A { float a; } ua;
        layout(set = 2, binding = 1) uniform B { float b; } ub;
        void main() { gl_Position = vec4(ua.a + ub.b, 0.0, 0.0, 1.0); }
    )glsl";
    const char* frag = R"glsl(
        #version 450
        layout(location = 0) out vec4 o;
        layout(set = 0, binding = 0) uniform A { float a; } ua;
        layout(set = 1, binding = 0) uniform C { float c; } uc;
        void main() { o = vec4(ua.a + uc.c); }
    )glsl";

    auto vs = compileGlsl(vert, ShaderStage::eVertex);
    auto fs = compileGlsl(frag, ShaderStage::eFragment);
    ASSERT_FALSE(vs.empty());
    ASSERT_FALSE(fs.empty());

    MetalBindingLayout layout;
    auto program = MetalBindingAllocator::fromProgram({vs, fs}, layout);
    EXPECT_TRUE(program.overflowError().empty()) << program.overflowError();

    // Shared (0,0) must match stage-local union assignment.
    EXPECT_EQ(program.buffer(0, 0), layout.buffer_base);
    // (1,0) and (2,1) are packed densely after (0,0) - not flatten indices.
    EXPECT_EQ(program.buffer(1, 0), layout.buffer_base + 1u);
    EXPECT_EQ(program.buffer(2, 1), layout.buffer_base + 2u);
    EXPECT_NE(program.buffer(2, 1), layout.buffer_base + 2u * layout.flatten_stride + 1u);

    auto again = MetalBindingAllocator::fromProgram({vs, fs}, layout);
    EXPECT_EQ(program.fingerprint(), again.fingerprint());
}

TEST(MetalBindingAllocatorTest, OverflowReportsSamplerLimit) {
    // 17 separate samplers in set 0 will overflow Metal's 16-sampler table.
    std::string src = "#version 450\nlayout(location=0) out vec4 o;\n";
    for (int i = 0; i < 17; ++i) {
        src += "layout(set=0, binding=" + std::to_string(i) + ") uniform sampler s" + std::to_string(i) + ";\n";
    }
    src += "void main(){ o = vec4(1.0); }\n";

    auto spirv = compileGlsl(src.c_str(), ShaderStage::eFragment);
    ASSERT_FALSE(spirv.empty());
    auto alloc = MetalBindingAllocator::fromProgram({spirv}, {});
    const std::string err = alloc.overflowError();
    EXPECT_FALSE(err.empty());
    EXPECT_NE(err.find("sampler"), std::string::npos);
}

}  // namespace vne::sc

#endif  // VNE_SC_GLSLANG_ENABLED
