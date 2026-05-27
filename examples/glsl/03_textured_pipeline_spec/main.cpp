/* ---------------------------------------------------------------------
 * Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License")
 * ----------------------------------------------------------------------
 */

#include <vertexnova/sc/vnesc.h>

#include <filesystem>
#include <iostream>

int main() {
    const std::filesystem::path spec_path =
        std::filesystem::path(__FILE__).parent_path() / "shaders" / "textured.pipeline.json";

    auto spec = vne::sc::loadShaderPipelineSpec(spec_path.string());
    if (!spec) {
        std::cerr << "Failed to load spec: " << spec_path << "\n";
        return 1;
    }

    const auto spec_dir = spec_path.parent_path();
    auto desc = spec->toBuildDesc(spec_dir);

    // Example: non-default Metal layout (buffer_base = 0 instead of vnerhi default 16)
    desc.metal_layout.buffer_base = 0;

    auto builder = vne::sc::ShaderCompilerFactory::createPipelineBuilder(spec->source_lang);
    if (!builder) {
        std::cerr << "No pipeline builder\n";
        return 1;
    }

    auto result = builder->build(desc);
    if (!result.ok()) {
        std::cerr << "Build failed: " << result.error << "\n";
        return 1;
    }

    for (const auto& stage : result.artifact.stages) {
        std::cout << "Stage " << static_cast<int>(stage.stage) << " bindings:\n";
        for (const auto& b : stage.reflection.bindings) {
            std::cout << "  " << b.name << " type=" << static_cast<int>(b.type) << " set=" << b.set
                      << " binding=" << b.binding;
            if (b.slots.metal) {
                std::cout << " metal tex=" << b.slots.metal->texture << " smp=" << b.slots.metal->sampler;
            }
            std::cout << "\n";
        }
    }

    const std::filesystem::path out_dir = std::filesystem::current_path() / "output" / "textured_bundle";
    std::filesystem::create_directories(out_dir);
    if (!vne::sc::writeShaderBundle(result.artifact, out_dir)) {
        std::cerr << "Failed to write bundle\n";
        return 1;
    }
    std::cout << "Bundle written to: " << out_dir << "\n";
    return 0;
}
