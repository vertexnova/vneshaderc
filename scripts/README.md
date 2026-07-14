# vnesc Scripts

Build helpers for **desktop platforms only**: Linux, macOS, and Windows.

vnesc is a static shader-compiler library. Scripts pass `VNE_SC_*` CMake options (not `VNE_TEMPLATE_*` or `VNE_RHI_*`).

## Quick start

```bash
# Linux / macOS - dev build with tests (default)
./scripts/build_linux.sh --dev -a test
./scripts/build_macos.sh --dev -a test

# Windows (Developer Command Prompt)
python scripts/build_windows.py --dev -a test
```

## Build layout

```
build/<BuildType>/build-<platform>-<compiler>/
```

Examples:

- `build/Debug/build-linux-gcc-13.2.0/`
- `build/Release/build-macos-clang-16.0.0/`
- `build/Debug/build-windows-msvc/`

## Common CMake flags (via script options)

| Script flag | CMake option |
|-------------|----------------|
| `--dev` | `VNE_SC_DEV=ON` (also enables tests + examples) |
| `--with-tests` / `--no-tests` | `VNE_SC_TESTS` |
| `--with-examples` / `--no-examples` | `VNE_SC_EXAMPLES` |
| `--with-tint` | `VNE_SC_TINT=ON` (Dawn; slow first configure) |
| `--with-spirvtools` | `VNE_SC_SPIRVTOOLS=ON` |
| `--no-glslang` | `VNE_SC_GLSLANG=OFF` |
| `--no-json` | `VNE_SC_JSON=OFF` |
| `--werror` | `WARNINGS_AS_ERRORS=ON` |

CI uses `-DVNE_SC_CI=ON` directly in GitHub Actions (see `.github/workflows/`).

## Platform scripts

### Linux (`build_linux.sh`)

```bash
./scripts/build_linux.sh -t Release -c clang -j 16 --dev -a test
./scripts/build_linux.sh --with-tint -t Debug
```

### macOS (`build_macos.sh`)

```bash
./scripts/build_macos.sh -t Release --dev -a test
./scripts/build_macos.sh -xcode -t Debug    # generates vnesc.xcodeproj
```

### Windows

**Python** (recommended):

```bash
python scripts/build_windows.py -t Release --dev -a test -j 8
```

**Git Bash:** `build_windows.sh`  
**PowerShell:** `build_windows.ps1`

Run all Windows scripts from a **Visual Studio Developer Command Prompt**.

## Documentation

```bash
./scripts/generate-docs.sh   # requires Doxygen; target vnesc_doc_doxygen
```

## Other utilities

- `clang_formatter.py` - format `src/`, `include/`, `tests/` with project `.clang-format`
