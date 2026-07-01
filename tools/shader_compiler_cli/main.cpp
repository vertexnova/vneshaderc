#include "vertexnova/sc/shader_pipeline_spec.h"
#include "vertexnova/sc/shader_bundle.h"
#include "vertexnova/sc/shader_compiler_factory.h"
#include "vertexnova/sc/shader_pipeline_builder.h"
#include "vertexnova/sc/gpu_layout_tools.h"

#include <filesystem>
#include <iostream>
#include <string>

namespace {

void printUsage(const char* argv0) {
    std::cerr << "Usage: " << argv0 << " --manifest <path> --output <bundle_dir> [--cache <dir>]\n"
              << "\n"
              << "  --manifest <path>    JSON manifest describing the pipeline build job\n"
              << "  --output  <dir>      Output bundle directory\n"
              << "  --cache   <dir>      Optional shader artifact cache directory\n";
}

}  // namespace

int main(int argc, char** argv) {
    std::string manifest_path;
    std::string output_dir;
    std::string cache_dir;

    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if ((arg == "--manifest" || arg == "-m") && i + 1 < argc) {
            manifest_path = argv[++i];
        } else if ((arg == "--output" || arg == "-o") && i + 1 < argc) {
            output_dir = argv[++i];
        } else if ((arg == "--cache" || arg == "-c") && i + 1 < argc) {
            cache_dir = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    if (manifest_path.empty()) {
        std::cerr << "Error: --manifest is required\n";
        printUsage(argv[0]);
        return 1;
    }
    if (output_dir.empty()) {
        std::cerr << "Error: --output is required\n";
        printUsage(argv[0]);
        return 1;
    }

    auto manifest = vne::sc::loadShaderPipelineSpec(manifest_path);
    if (!manifest.has_value()) {
        std::cerr << "Error: failed to parse manifest: " << manifest_path << "\n";
        return 1;
    }
    if (!manifest->errors.empty()) {
        for (const auto& e : manifest->errors) {
            std::cerr << "Manifest error: " << e << "\n";
        }
        return 1;
    }

    const std::filesystem::path spec_dir = std::filesystem::path(manifest_path).parent_path();

    auto builder = vne::sc::ShaderCompilerFactory::createPipelineBuilder(manifest->source_lang);
    if (!builder) {
        std::cerr << "Error: no pipeline builder available for the requested source language\n";
        return 1;
    }

    vne::sc::PipelineBuildDesc desc = manifest->toBuildDesc(spec_dir);
    if (!cache_dir.empty()) {
        desc.use_cache = true;
        desc.cache_dir = cache_dir;
    }

    auto result = builder->build(desc);
    if (!result.ok()) {
        std::cerr << "Error: shader build failed:\n" << result.error << "\n";
        return 1;
    }

    if (manifest->validate_layout) {
        std::vector<std::filesystem::path> registry_paths;
        registry_paths.reserve(manifest->layout_registries.size());
        for (const auto& rel : manifest->layout_registries) {
            registry_paths.push_back(spec_dir / rel);
        }

        vne::sc::GpuLayoutRegistry registry;
        std::string layout_error;
        if (!vne::sc::mergeGpuLayoutRegistries(registry_paths, manifest->uniform_buffers, registry, layout_error)) {
            std::cerr << "Error: " << layout_error << "\n";
            return 1;
        }
        if (registry.uniform_buffers.empty()) {
            std::cerr << "Error: validate_layout enabled but layout registry is empty\n";
            return 1;
        }
        if (!vne::sc::validateGpuLayouts(result.artifact, registry, layout_error)) {
            std::cerr << "Error: " << layout_error << "\n";
            return 1;
        }
    }

    const std::filesystem::path bundle_path(output_dir);
    if (!vne::sc::writeShaderBundle(result.artifact, bundle_path)) {
        std::cerr << "Error: failed to write bundle to " << output_dir << "\n";
        return 1;
    }

    if (!manifest->emit_binding_decls.empty()) {
        vne::sc::EmitBindingDeclOptions emit_options;
        emit_options.skip_blocks = manifest->emit_binding_decls_skip;
        emit_options.include_blocks = manifest->emit_binding_decls_include;
        emit_options.compose_includes = manifest->emit_binding_decls_compose;
        emit_options.bindings_stage = manifest->emit_bindings_stage;

        const auto binding_path = spec_dir / manifest->emit_binding_decls;
        const std::string binding_glsl = vne::sc::buildBindingDeclsGlsl(result.artifact, emit_options);
        std::string write_error;
        if (!vne::sc::writeBindingDeclsFile(binding_path, binding_glsl, write_error)) {
            std::cerr << "Error: " << write_error << "\n";
            return 1;
        }
        std::cout << "Binding decls written to: " << binding_path << "\n";
    }

    std::cout << "Bundle written to: " << bundle_path << "\n";
    std::cout << "  Stages: " << result.artifact.stages.size() << "\n";
    for (const auto& stage : result.artifact.stages) {
        std::cout << "  - stage " << static_cast<int>(stage.stage) << " | spirv=" << stage.spirv.size() << " words"
                  << " | cross=" << stage.cross_compiled.size() << " target(s)\n";
    }
    return 0;
}
