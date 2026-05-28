# vnesc tools

Command-line utilities for offline shader compilation.

## vnesc_shader_compiler

C++ CLI that runs the full pipeline from a JSON pipeline spec (`.pipeline.json`).

### Usage

```text
vnesc_shader_compiler --manifest <path> --output <bundle_dir> [--cache <dir>]
```

| Flag | Description |
|------|-------------|
| `--manifest`, `-m` | Path to a `.pipeline.json` manifest (required) |
| `--output`, `-o` | Output bundle directory (required); written as-is, no suffix added |
| `--cache`, `-c` | Optional artifact cache directory |
| `--help`, `-h` | Print usage |

### Example

```bash
vnesc_shader_compiler \
    --manifest shaders/src/mesh_phong.pipeline.json \
    --output   build/bundles/mesh_phong \
    --cache    .shader_cache
```

## compile_shaders.py

Python batch wrapper around `vnesc_shader_compiler` with an automatic WGSL fallback via **naga**.
Expands glob patterns, compiles manifests in parallel, and writes each bundle to `<output>/<manifest-stem>/`.

When `vnesc_shader_compiler` was built **without** Tint (`VNE_SC_TINT=OFF`) — which is the default and
avoids the large Dawn/Chromium dependency — the compiled bundle will have SPIR-V and MSL but no WGSL.
`compile_shaders.py` detects this automatically and runs `naga` (SPIR-V → WGSL) as a post-processing
step, then patches `bundle.header` so `loadShaderBundle` finds the WGSL files at runtime.

### Quick-start (no Tint needed)

```bash
# 1. Get naga — either via cargo or the download helper
cargo install naga-cli          # option A
python3 tools/download_naga.py  # option B: downloads pre-built macOS binary to tools/macos/

# 2. Build the C++ compiler (one-time; no Tint required)
cmake --build build/Debug/build-macos-clang-17.0.0 --target vnesc_shader_compiler

# 3. Compile shaders — WGSL is generated automatically via naga
python3 tools/compile_shaders.py \
    --manifest "shaders/src/teapot.manifest.json" \
    --output   shaders/gen
```

### Usage

```text
compile_shaders.py --manifest <glob>... --output <dir> [options]
```

| Flag | Description |
|------|-------------|
| `--manifest`, `-m` | One or more manifest paths or globs (required) |
| `--output`, `-o` | Output root directory (required) |
| `--cache`, `-c` | Optional cache directory passed to the compiler |
| `--compiler` | Path to `vnesc_shader_compiler` (auto-detected if omitted) |
| `--naga` | Path to `naga` binary (auto-detected from PATH and `tools/macos/`) |
| `--no-wgsl` | Skip WGSL fallback |
| `--parallel`, `-j` | Parallel jobs (default: half CPU count; must be ≥ 1) |
| `--dry-run` | Print commands without running |
| `--verbose`, `-v` | Log each manifest and naga invocation |

### WGSL fallback details

`compile_shaders.py` reads `bundle.header` after each compile, detects stages whose
`wgsl_file` field is empty, runs `naga <stage.spv> <stage.wgsl>` for each such stage,
then re-writes `bundle.header` with the new WGSL filenames.  The existing `reflection.bin`
is unchanged — WGSL generation is purely additive.

naga is searched in order:
1. `--naga <path>` flag
2. `naga` or `naga-cli` in `PATH`
3. `tools/macos/naga` (placed there by `download_naga.py`)

---

## download_naga.py

Downloads a pre-built `naga` binary for the current macOS architecture (aarch64 or x86_64)
from the wgpu GitHub releases and places it in `tools/macos/`.

```bash
python3 tools/download_naga.py [--version v24.0.0] [--output tools/macos/]
```

After this, `compile_shaders.py` auto-detects the binary — no `--naga` flag needed.
