/* ---------------------------------------------------------------------
 * Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License")
 * ----------------------------------------------------------------------
 */

#include <vertexnova/sc/vnesc.h>

#include <filesystem>
#include <iostream>

namespace {

const char* kVertGlsl = R"(#version 450
layout(location = 0) in vec3 aPos;
void main() {
    gl_Position = vec4(aPos, 1.0);
}
)";

const char* kFragGlsl = R"(#version 450
layout(location = 0) out vec4 FragColor;
void main() {
    FragColor = vec4(1.0, 0.4, 0.2, 1.0);
}
)";

}  // namespace

int main() {
    auto builder = vne::sc::ShaderCompilerFactory::createPipelineBuilder(vne::sc::SourceLang::eGLSL);
    if (!builder) {
        std::cerr << "No pipeline builder (is VNE_SC_GLSLANG enabled?)\n";
        return 1;
    }

    vne::sc::PipelineBuildDesc desc;
    desc.name = "hello_compiler";
    desc.targets = {vne::sc::CrossTarget::eMSL};
    desc.validate = false;
    desc.use_cache = false;

    vne::sc::CompileRequest vert;
    vert.source = kVertGlsl;
    vert.stage = vne::sc::ShaderStage::eVertex;

    vne::sc::CompileRequest frag;
    frag.source = kFragGlsl;
    frag.stage = vne::sc::ShaderStage::eFragment;

    desc.stages = {vert, frag};

    auto result = builder->build(desc);
    if (!result.ok()) {
        std::cerr << "Build failed: " << result.error << "\n";
        return 1;
    }

    for (const auto& stage : result.artifact.stages) {
        for (const auto& cc : stage.cross_compiled) {
            if (cc.target == vne::sc::CrossTarget::eMSL) {
                std::cout << "--- MSL (stage " << static_cast<int>(stage.stage) << ") ---\n";
                std::cout << cc.source << "\n";
            }
        }
    }

    const std::filesystem::path bundle_dir =
        std::filesystem::temp_directory_path() / "hello_compiler_bundle";
    if (!vne::sc::writeShaderBundle(result.artifact, bundle_dir)) {
        std::cerr << "Failed to write bundle to " << bundle_dir << "\n";
        return 1;
    }
    std::cout << "Bundle written to: " << bundle_dir << "\n";
    return 0;
}
