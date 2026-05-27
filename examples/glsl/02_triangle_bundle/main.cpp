/* ---------------------------------------------------------------------
 * Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License")
 * ----------------------------------------------------------------------
 */

#include "shader_loader.h"

#include <vertexnova/sc/vnesc.h>

#include <filesystem>
#include <iostream>

int main() {
    const std::filesystem::path shader_dir = std::filesystem::path(__FILE__).parent_path() / "shaders";

    auto builder = vne::sc::ShaderCompilerFactory::createPipelineBuilder(vne::sc::SourceLang::eGLSL);
    if (!builder) {
        std::cerr << "No pipeline builder\n";
        return 1;
    }

    vne::sc::PipelineBuildDesc desc;
    desc.name = "triangle";
    desc.targets = {vne::sc::CrossTarget::eMSL};
    desc.validate = false;
    desc.use_cache = false;

    vne::sc::CompileRequest vert;
    vert.file_path = (shader_dir / "triangle.vert.glsl").string();
    vert.stage = vne::sc::ShaderStage::eVertex;

    vne::sc::CompileRequest frag;
    frag.file_path = (shader_dir / "triangle.frag.glsl").string();
    frag.stage = vne::sc::ShaderStage::eFragment;

    desc.stages = {vert, frag};

    auto result = builder->build(desc);
    if (!result.ok()) {
        std::cerr << "Build failed: " << result.error << "\n";
        return 1;
    }

    for (const auto& stage : result.artifact.stages) {
        std::cout << "Stage " << static_cast<int>(stage.stage) << ": " << stage.reflection.bindings.size()
                  << " binding(s)\n";
        for (const auto& b : stage.reflection.bindings) {
            std::cout << "  " << b.name << " set=" << b.set << " binding=" << b.binding;
            if (b.slots.metal) {
                std::cout << " metal={buf:" << b.slots.metal->buffer << ",tex:" << b.slots.metal->texture
                          << ",smp:" << b.slots.metal->sampler << "}";
            }
            std::cout << "\n";
        }
    }

    const std::filesystem::path out_dir = std::filesystem::current_path() / "output" / "triangle_bundle";
    std::filesystem::create_directories(out_dir);
    if (!vne::sc::writeShaderBundle(result.artifact, out_dir)) {
        std::cerr << "Failed to write bundle\n";
        return 1;
    }
    std::cout << "Bundle written to: " << out_dir << "\n";
    return 0;
}
