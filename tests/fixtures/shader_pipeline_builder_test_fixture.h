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
 * @file fixtures/shader_pipeline_builder_test_fixture.h — helpers for ShaderPipelineBuilder tests.
 */

#include <filesystem>

#include "fixtures/glslang_frontend_test_fixture.h"

namespace vne::sc::test {

/** Helpers for @ref ShaderPipelineBuilder integration tests. */
class ShaderPipelineBuilderTestFixture : public GlslangFrontEndTestFixture {
   protected:
    static std::shared_ptr<IShaderPipelineBuilder> makeGlslPipelineBuilder() {
        return ShaderCompilerFactory::createPipelineBuilder(SourceLang::eGLSL);
    }

    static PipelineBuildDesc makeVertMslDesc(const char* name, bool use_cache, const std::string& cache_dir = {}) {
        PipelineBuildDesc desc;
        desc.name = name;
        desc.validate = false;
        desc.use_cache = use_cache;
        if (!cache_dir.empty()) {
            desc.cache_dir = cache_dir;
        }
        desc.targets = {CrossTarget::eMSL};
        desc.stages.push_back(makeVertexRequest());
        return desc;
    }

    static PipelineBuildDesc makeVertFragMslDesc(const char* name) {
        PipelineBuildDesc desc;
        desc.name = name;
        desc.validate = false;
        desc.use_cache = false;
        desc.targets = {CrossTarget::eMSL};
        desc.stages.push_back(makeVertexRequest());
        desc.stages.push_back(makeFragmentRequest());
        return desc;
    }

    static std::filesystem::path tempDir(const char* prefix) {
        auto path = std::filesystem::temp_directory_path() / prefix;
        std::filesystem::remove_all(path);
        return path;
    }

    static void removeTempDir(const std::filesystem::path& path) { std::filesystem::remove_all(path); }
};

}  // namespace vne::sc::test
