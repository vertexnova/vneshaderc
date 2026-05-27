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

Python batch wrapper around `vnesc_shader_compiler`. Expands glob patterns, compiles manifests in parallel, and writes each manifest to `<output>/<manifest-stem>/`.

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
| `--parallel`, `-j` | Parallel jobs (default: half CPU count; must be ≥ 1) |
| `--dry-run` | Print commands without running |
| `--verbose`, `-v` | Log each manifest |

### Example

```bash
python3 tools/compile_shaders.py \
    --manifest "shaders/src/**/*.pipeline.json" \
    --output   output/bundles \
    --cache    .shader_cache \
    --parallel 8
```

Each `foo.pipeline.json` writes to `output/bundles/foo.pipeline/`.
