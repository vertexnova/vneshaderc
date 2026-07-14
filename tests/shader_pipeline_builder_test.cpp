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
#include "vertexnova/sc/shader_artifact_cache.h"
#include "vertexnova/sc/shader_compiler_factory.h"
#include "vertexnova/sc/shader_frontend.h"
#include "vertexnova/sc/shader_pipeline_builder.h"

#include <memory>
#include <optional>
#include <string>

namespace vne::sc::test {
namespace {

class CountingFrontEnd final : public IShaderFrontEnd {
   public:
    explicit CountingFrontEnd(std::shared_ptr<IShaderFrontEnd> inner)
        : inner_(std::move(inner)) {}

    [[nodiscard]] bool isAvailable() const noexcept override { return inner_ && inner_->isAvailable(); }

    CompileResult compile(const CompileRequest& req) override {
        ++compile_count_;
        return inner_->compile(req);
    }

    [[nodiscard]] int compileCount() const noexcept { return compile_count_; }

   private:
    std::shared_ptr<IShaderFrontEnd> inner_;
    int compile_count_ = 0;
};

}  // namespace

class ShaderPipelineBuilderTest : public ShaderPipelineBuilderTestFixture {};

TEST_F(ShaderPipelineBuilderTest, FullBuildProducesValidArtifact) {
    auto builder = makeGlslPipelineBuilder();
    ASSERT_NE(builder, nullptr);

    auto desc = makeVertFragMslDesc("test_pipeline");
    auto result = builder->build(desc);
    EXPECT_TRUE(result.ok()) << result.error;
    EXPECT_TRUE(result.artifact.isValid());
    EXPECT_EQ(result.artifact.stages.size(), 2u);

    const auto* vert_stage = result.artifact.findStage(ShaderStage::eVertex);
    ASSERT_NE(vert_stage, nullptr);
    EXPECT_FALSE(vert_stage->spirv.empty());
    EXPECT_FALSE(vert_stage->reflection.bindings.empty());

    const auto* msl = vert_stage->findCrossCompiled(CrossTarget::eMSL);
    ASSERT_NE(msl, nullptr);
    EXPECT_FALSE(msl->source.empty());
    EXPECT_FALSE(msl->entry_point.empty());
}

TEST_F(ShaderPipelineBuilderTest, CacheHitSkipsRecompilation) {
    auto tmp = tempDir("vnesc_shader_pipeline_builder_cache");
    auto frontend = std::make_shared<CountingFrontEnd>(ShaderCompilerFactory::createFrontEnd(SourceLang::eGLSL));
    auto builder = std::make_shared<ShaderPipelineBuilder>(frontend,
                                                           ShaderCompilerFactory::createCrossCompiler(),
                                                           ShaderCompilerFactory::createReflector(),
                                                           ShaderCompilerFactory::createValidator());

    auto desc = makeVertMslDesc("cached_pipeline", true, tmp.string());
    auto r1 = builder->build(desc);
    ASSERT_TRUE(r1.ok()) << r1.error;
    EXPECT_EQ(frontend->compileCount(), 1);

    auto r2 = builder->build(desc);
    ASSERT_TRUE(r2.ok()) << r2.error;
    EXPECT_EQ(frontend->compileCount(), 1) << "cache hit must not re-invoke the front-end";
    ASSERT_FALSE(r1.artifact.stages.empty());
    ASSERT_FALSE(r2.artifact.stages.empty());
    EXPECT_EQ(r1.artifact.stages[0].spirv, r2.artifact.stages[0].spirv);

    removeTempDir(tmp);
}

TEST_F(ShaderPipelineBuilderTest, SiblingStageChangeInvalidatesProgramCacheKeys) {
    CompileRequest vert;
    vert.stage = ShaderStage::eVertex;
    vert.source = R"glsl(
        #version 450
        layout(set = 0, binding = 0) uniform A { float a; } ua;
        void main() { gl_Position = vec4(ua.a); }
    )glsl";

    CompileRequest frag_a;
    frag_a.stage = ShaderStage::eFragment;
    frag_a.source = R"glsl(
        #version 450
        layout(location = 0) out vec4 o;
        layout(set = 0, binding = 0) uniform A { float a; } ua;
        void main() { o = vec4(ua.a); }
    )glsl";

    CompileRequest frag_b = frag_a;
    frag_b.source = R"glsl(
        #version 450
        layout(location = 0) out vec4 o;
        layout(set = 0, binding = 0) uniform A { float a; } ua;
        layout(set = 1, binding = 0) uniform B { float b; } ub;
        void main() { o = vec4(ua.a + ub.b); }
    )glsl";

    MetalBindingLayout layout;
    const auto fp_a = ShaderArtifactCache::makeProgramFingerprint({vert, frag_a}, layout);
    const auto fp_b = ShaderArtifactCache::makeProgramFingerprint({vert, frag_b}, layout);
    EXPECT_NE(fp_a, fp_b);

    const auto key_vert_a = ShaderArtifactCache::makeKey(vert, {CrossTarget::eMSL}, layout, fp_a);
    const auto key_vert_b = ShaderArtifactCache::makeKey(vert, {CrossTarget::eMSL}, layout, fp_b);
    EXPECT_NE(key_vert_a, key_vert_b);
}

TEST_F(ShaderPipelineBuilderTest, MultiSetMetalSlotsAgreeAcrossStages) {
    auto builder = makeGlslPipelineBuilder();
    ASSERT_NE(builder, nullptr);

    PipelineBuildDesc desc;
    desc.name = "multi_set_program";
    desc.validate = false;
    desc.use_cache = false;
    desc.targets = {CrossTarget::eMSL};

    CompileRequest vert;
    vert.stage = ShaderStage::eVertex;
    vert.source = R"glsl(
        #version 450
        layout(location = 0) in vec3 aPos;
        layout(set = 0, binding = 0) uniform Frame { mat4 mvp; } frame;
        layout(set = 2, binding = 0) uniform Object { mat4 model; } object;
        void main() {
            gl_Position = frame.mvp * object.model * vec4(aPos, 1.0);
        }
    )glsl";

    CompileRequest frag;
    frag.stage = ShaderStage::eFragment;
    frag.source = R"glsl(
        #version 450
        layout(location = 0) out vec4 oColor;
        layout(set = 0, binding = 0) uniform Frame { mat4 mvp; } frame;
        layout(set = 1, binding = 0) uniform Material { vec4 color; } material;
        void main() {
            oColor = material.color * frame.mvp[0];
        }
    )glsl";

    desc.stages = {vert, frag};
    auto result = builder->build(desc);
    ASSERT_TRUE(result.ok()) << result.error;
    ASSERT_EQ(result.artifact.stages.size(), 2u);

    const auto* vs = result.artifact.findStage(ShaderStage::eVertex);
    const auto* fs = result.artifact.findStage(ShaderStage::eFragment);
    ASSERT_NE(vs, nullptr);
    ASSERT_NE(fs, nullptr);

    auto findMetalBuffer = [](const StageReflection& sr, uint32_t set, uint32_t binding) -> std::optional<uint32_t> {
        for (const auto& b : sr.bindings) {
            if (b.set == set && b.binding == binding && b.slots.metal.has_value()) {
                return b.slots.metal->buffer;
            }
        }
        return std::nullopt;
    };

    // set0 binding0 is shared - must get the same Metal buffer slot in both stages.
    auto vs_frame = findMetalBuffer(vs->reflection, 0, 0);
    auto fs_frame = findMetalBuffer(fs->reflection, 0, 0);
    ASSERT_TRUE(vs_frame.has_value());
    ASSERT_TRUE(fs_frame.has_value());
    EXPECT_EQ(*vs_frame, *fs_frame);

    // Dense packing: set2 must not use the old flatten formula (16 + 2*32 + 0 = 80).
    auto vs_object = findMetalBuffer(vs->reflection, 2, 0);
    ASSERT_TRUE(vs_object.has_value());
    EXPECT_LE(*vs_object, MetalIndexLimits::kMaxBufferIndex);
    EXPECT_NE(*vs_object, 80u);

    // Vulkan identity preserved on reflection metadata.
    bool saw_set1 = false;
    for (const auto& b : fs->reflection.bindings) {
        if (b.set == 1 && b.binding == 0) {
            saw_set1 = true;
            EXPECT_EQ(b.set, 1u);
            EXPECT_EQ(b.binding, 0u);
        }
    }
    EXPECT_TRUE(saw_set1);

    // Emitted MSL must reference dense buffer indices (not out-of-range flatten values).
    const auto* vs_msl = vs->findCrossCompiled(CrossTarget::eMSL);
    ASSERT_NE(vs_msl, nullptr);
    EXPECT_EQ(vs_msl->source.find("[[buffer(80)]]"), std::string::npos);
}

TEST_F(ShaderPipelineBuilderTest, WgslLeavesVulkanSetBindingIdentity) {
    auto builder = makeGlslPipelineBuilder();
    ASSERT_NE(builder, nullptr);

    PipelineBuildDesc desc;
    desc.name = "wgsl_identity";
    desc.validate = false;
    desc.use_cache = false;
    desc.targets = {CrossTarget::eWGSL};
    desc.stages.push_back(makeVertexRequest());

    auto result = builder->build(desc);
    ASSERT_TRUE(result.ok()) << result.error;
    const auto* vs = result.artifact.findStage(ShaderStage::eVertex);
    ASSERT_NE(vs, nullptr);
    ASSERT_FALSE(vs->reflection.bindings.empty());
    EXPECT_EQ(vs->reflection.bindings[0].set, 0u);
    EXPECT_EQ(vs->reflection.bindings[0].binding, 0u);
}

}  // namespace vne::sc::test
