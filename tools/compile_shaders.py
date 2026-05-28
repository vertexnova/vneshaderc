#!/usr/bin/env python3
"""vnesc offline shader compiler — wrapper around vnesc_shader_compiler with naga WGSL fallback.

Usage:
    compile_shaders.py --manifest <glob-or-path> --output <dir>
                       [--cache <dir>] [--compiler <path>]
                       [--naga <path>] [--no-wgsl]
                       [--parallel <N>] [--dry-run] [--verbose]

WGSL fallback:
    When the compiled bundle is missing WGSL shader files (i.e. vnesc was built
    without Tint/Dawn), compile_shaders.py automatically runs naga to convert
    each stage's SPIR-V to WGSL and patches bundle.header with the new paths.

    naga is searched in order:
      1. --naga flag
      2. PATH (naga or naga-cli)
      3. <script-dir>/macos/naga  (pre-built binary from download_naga.py)
      4. <script-dir>/macos/aarch64/naga

    Install naga via cargo:  cargo install naga-cli
    Or download a pre-built binary:  python3 tools/download_naga.py
"""

import argparse
import glob
import io
import os
import shutil
import struct
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path


# ---------------------------------------------------------------------------
# Bundle-header parsing / patching
# ---------------------------------------------------------------------------

_BUNDLE_MAGIC = b"VNSH"
_BUNDLE_VERSION = 2


def _read_u8(buf: io.BytesIO) -> int:
    return struct.unpack("<B", buf.read(1))[0]


def _read_u32(buf: io.BytesIO) -> int:
    return struct.unpack("<I", buf.read(4))[0]


def _read_str(buf: io.BytesIO) -> str:
    length = _read_u32(buf)
    if length == 0:
        return ""
    return buf.read(length).decode("utf-8")


def _write_u8(buf: io.BytesIO, v: int) -> None:
    buf.write(struct.pack("<B", v))


def _write_u32(buf: io.BytesIO, v: int) -> None:
    buf.write(struct.pack("<I", v))


def _write_str(buf: io.BytesIO, s: str) -> None:
    b = s.encode("utf-8")
    _write_u32(buf, len(b))
    buf.write(b)


def _parse_bundle_header(data: bytes) -> tuple[str, int, list[dict]]:
    """Parse bundle.header.  Returns (pkg_name, source_lang, stages).

    Each stage dict has keys: stage_type, entry, spirv_file, msl_file,
    msl_entry, wgsl_file, wgsl_entry.
    """
    buf = io.BytesIO(data)
    magic = buf.read(4)
    if magic != _BUNDLE_MAGIC:
        raise ValueError(f"Invalid bundle.header magic: {magic!r}")
    version = _read_u32(buf)
    if version != _BUNDLE_VERSION:
        raise ValueError(f"Unsupported bundle.header version {version} (expected {_BUNDLE_VERSION})")
    pkg_name = _read_str(buf)
    source_lang = _read_u8(buf)
    stage_count = _read_u32(buf)
    stages = []
    for _ in range(stage_count):
        stage: dict = {
            "stage_type": _read_u8(buf),
            "entry":      _read_str(buf),
            "spirv_file": _read_str(buf),
            "msl_file":   _read_str(buf),
            "msl_entry":  _read_str(buf),
            "wgsl_file":  _read_str(buf),
            "wgsl_entry": _read_str(buf),
        }
        stages.append(stage)
    return pkg_name, source_lang, stages


def _write_bundle_header(pkg_name: str, source_lang: int, stages: list[dict]) -> bytes:
    buf = io.BytesIO()
    buf.write(_BUNDLE_MAGIC)
    _write_u32(buf, _BUNDLE_VERSION)
    _write_str(buf, pkg_name)
    _write_u8(buf, source_lang)
    _write_u32(buf, len(stages))
    for s in stages:
        _write_u8(buf, s["stage_type"])
        _write_str(buf, s["entry"])
        _write_str(buf, s["spirv_file"])
        _write_str(buf, s["msl_file"])
        _write_str(buf, s["msl_entry"])
        _write_str(buf, s["wgsl_file"])
        _write_str(buf, s["wgsl_entry"])
    return buf.getvalue()


def _bundle_missing_wgsl(bundle_dir: Path) -> list[int]:
    """Return indices of stages whose wgsl_file field is empty."""
    header_path = bundle_dir / "bundle.header"
    if not header_path.is_file():
        return []
    try:
        _, _, stages = _parse_bundle_header(header_path.read_bytes())
    except (ValueError, struct.error):
        return []
    return [i for i, s in enumerate(stages) if not s.get("wgsl_file")]


def patch_bundle_with_wgsl(bundle_dir: Path, wgsl_map: dict[int, tuple[str, str]], verbose: bool) -> bool:
    """Patch bundle.header to record WGSL file+entry for the given stage indices.

    wgsl_map: { stage_index: (wgsl_filename, wgsl_entry) }
    Files must already exist inside bundle_dir.
    """
    header_path = bundle_dir / "bundle.header"
    try:
        pkg_name, source_lang, stages = _parse_bundle_header(header_path.read_bytes())
    except (ValueError, struct.error) as exc:
        sys.stderr.write(f"error: failed to parse bundle.header in {bundle_dir}: {exc}\n")
        return False
    for idx, (wgsl_file, wgsl_entry) in wgsl_map.items():
        if idx >= len(stages):
            sys.stderr.write(f"error: stage index {idx} out of range\n")
            return False
        stages[idx]["wgsl_file"] = wgsl_file
        stages[idx]["wgsl_entry"] = wgsl_entry
        if verbose:
            print(f"  patched stage {idx}: wgsl_file={wgsl_file!r}, wgsl_entry={wgsl_entry!r}")
    header_path.write_bytes(_write_bundle_header(pkg_name, source_lang, stages))
    return True


# ---------------------------------------------------------------------------
# Naga: SPIR-V → WGSL
# ---------------------------------------------------------------------------

_SCRIPT_DIR = Path(__file__).resolve().parent


def find_naga(override: str | None) -> Path | None:
    if override:
        p = Path(override)
        if p.is_file() and os.access(p, os.X_OK):
            return p
        sys.stderr.write(f"warning: --naga path not found or not executable: {override}\n")
        return None
    for name in ("naga", "naga-cli"):
        found = shutil.which(name)
        if found:
            return Path(found)
    candidates = [
        _SCRIPT_DIR / "macos" / "naga",
        _SCRIPT_DIR / "macos" / "aarch64" / "naga",
        _SCRIPT_DIR / "macos" / "x86_64" / "naga",
    ]
    for c in candidates:
        if c.is_file() and os.access(c, os.X_OK):
            return c
    return None


def _naga_wgsl_entry(stage_type: int) -> str:
    """naga preserves GLSL entry point names — all our GLSL shaders use 'main'."""
    return "main"


def generate_wgsl_for_bundle(bundle_dir: Path, naga: Path, verbose: bool, dry_run: bool) -> bool:
    """Run naga on SPIR-V files where WGSL is missing; patch bundle.header."""
    missing = _bundle_missing_wgsl(bundle_dir)
    if not missing:
        return True

    header_path = bundle_dir / "bundle.header"
    try:
        _, _, stages = _parse_bundle_header(header_path.read_bytes())
    except (ValueError, struct.error) as exc:
        sys.stderr.write(f"error: {bundle_dir}: bundle.header parse failed: {exc}\n")
        return False

    wgsl_map: dict[int, tuple[str, str]] = {}

    for idx in missing:
        stage = stages[idx]
        spv_file = stage.get("spirv_file", "")
        if not spv_file:
            sys.stderr.write(f"error: stage {idx} has no SPIR-V file — cannot generate WGSL\n")
            return False

        spv_path = bundle_dir / spv_file
        if not spv_path.is_file():
            sys.stderr.write(f"error: SPIR-V not found: {spv_path}\n")
            return False

        # Derive WGSL filename from SPV filename: foo.vert.spv → foo.vert.wgsl
        stem = spv_path.stem  # e.g. "teapot.vert"
        wgsl_name = stem + ".wgsl"
        wgsl_path = bundle_dir / wgsl_name
        entry = _naga_wgsl_entry(stage["stage_type"])

        cmd = [str(naga), str(spv_path), str(wgsl_path)]
        if dry_run:
            print(f"[dry-run] {' '.join(cmd)}")
        else:
            if verbose:
                print(f"  naga: {spv_file} → {wgsl_name}")
            result = subprocess.run(cmd, capture_output=True, text=True)
            if result.returncode != 0:
                sys.stderr.write(f"error: naga failed for {spv_path}:\n{result.stderr}\n")
                return False

        wgsl_map[idx] = (wgsl_name, entry)

    if dry_run:
        return True
    return patch_bundle_with_wgsl(bundle_dir, wgsl_map, verbose)


# ---------------------------------------------------------------------------
# Compiler discovery
# ---------------------------------------------------------------------------

def find_compiler(override: str | None) -> Path | None:
    if override:
        p = Path(override)
        if p.is_file():
            return p
        sys.stderr.write(f"error: --compiler path not found: {override}\n")
        return None

    for name in ("vnesc_shader_compiler", "shader_compiler_cli"):
        found = shutil.which(name)
        if found:
            return Path(found)

    # Common build-tree locations relative to CWD or this script
    candidates = [
        Path("build") / "bin" / "vnesc_shader_compiler",
        Path("build") / "vnesc_shader_compiler",
        # Debug/shared layouts that vnesc uses
        *[
            Path(f"build/{config}/build-macos-clang-17.0.0/bin/vnesc_shader_compiler")
            for config in ("Debug", "Release", "shared/Debug", "shared/Release")
        ],
        Path("../vnesc/build/bin") / "vnesc_shader_compiler",
        Path("../vnesc/build") / "vnesc_shader_compiler",
        # Relative to script location
        _SCRIPT_DIR.parent / "build" / "bin" / "vnesc_shader_compiler",
        *[
            _SCRIPT_DIR.parent / f"build/{config}/build-macos-clang-17.0.0/bin/vnesc_shader_compiler"
            for config in ("Debug", "Release", "shared/Debug", "shared/Release")
        ],
    ]
    for c in candidates:
        if c.is_file():
            return c
    return None


# ---------------------------------------------------------------------------
# Core compilation
# ---------------------------------------------------------------------------

def compile_one(
    compiler: Path,
    manifest: Path,
    output: Path,
    cache: Path | None,
    naga: Path | None,
    dry_run: bool,
    verbose: bool,
) -> tuple[bool, str, str]:
    """Compile one manifest; run naga WGSL fallback if needed."""
    cmd = [str(compiler), "--manifest", str(manifest), "--output", str(output)]
    if cache:
        cmd += ["--cache", str(cache)]

    if dry_run:
        print(f"[dry-run] {' '.join(cmd)}")
        return True, str(manifest), ""

    if verbose:
        print(f"compiling: {manifest}")

    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        return False, str(manifest), result.stderr.strip()

    if naga and output.is_dir() and _bundle_missing_wgsl(output):
        if verbose:
            print(f"  WGSL missing — running naga fallback for: {output.name}")
        if not generate_wgsl_for_bundle(output, naga, verbose=verbose, dry_run=False):
            return False, str(manifest), f"naga WGSL generation failed for {output}"

    return True, str(manifest), ""


# ---------------------------------------------------------------------------
# Manifest expansion
# ---------------------------------------------------------------------------

def expand_manifests(patterns: list[str]) -> list[Path]:
    paths: list[Path] = []
    for pattern in patterns:
        expanded = glob.glob(pattern, recursive=True)
        if expanded:
            paths.extend(Path(p) for p in expanded)
        else:
            p = Path(pattern)
            if p.is_file():
                paths.append(p)
            else:
                sys.stderr.write(f"warning: no files matched: {pattern}\n")
    return paths


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main() -> int:
    parser = argparse.ArgumentParser(
        description="vnesc offline shader compiler — wrapper with naga WGSL fallback",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "WGSL fallback:\n"
            "  When the bundle is missing WGSL files, naga is used to convert SPIR-V→WGSL\n"
            "  and bundle.header is patched automatically.\n"
            "  Install naga: cargo install naga-cli\n"
            "  Or download:  python3 tools/download_naga.py\n"
        ),
    )
    parser.add_argument("--manifest", "-m", nargs="+", required=True, metavar="GLOB",
                        help="One or more manifest paths or glob patterns")
    parser.add_argument("--output", "-o", required=True, metavar="DIR",
                        help="Output root; each manifest writes to <dir>/<manifest-stem>/")
    parser.add_argument("--cache", "-c", metavar="DIR",
                        help="Optional shader artifact cache directory")
    parser.add_argument("--compiler", metavar="PATH",
                        help="Explicit path to vnesc_shader_compiler")
    parser.add_argument("--naga", metavar="PATH",
                        help="Explicit path to naga binary for WGSL fallback (auto-detected if omitted)")
    parser.add_argument("--no-wgsl", action="store_true",
                        help="Skip WGSL fallback even if naga is available")
    parser.add_argument("--parallel", "-j", type=int,
                        default=max(1, (os.cpu_count() or 2) // 2), metavar="N",
                        help="Parallel jobs (default: half CPU count)")
    parser.add_argument("--dry-run", action="store_true", help="Print commands without running")
    parser.add_argument("--verbose", "-v", action="store_true")

    args = parser.parse_args()
    if args.parallel < 1:
        parser.error("--parallel must be >= 1")

    compiler = find_compiler(args.compiler)
    if compiler is None:
        sys.stderr.write(
            "error: vnesc_shader_compiler not found.\n"
            "  Build: cmake --build <build-dir> --target vnesc_shader_compiler\n"
            "  Or pass --compiler <path>.\n"
        )
        return 1

    naga: Path | None = None
    if not args.no_wgsl:
        naga = find_naga(args.naga)
        if naga and args.verbose:
            print(f"naga: {naga}")
        elif not naga:
            print(
                "info: naga not found — WGSL will be skipped "
                "(install via: cargo install naga-cli  or  python3 tools/download_naga.py)"
            )

    manifests = expand_manifests(args.manifest)
    if not manifests:
        sys.stderr.write("error: no manifest files found\n")
        return 1

    output_root = Path(args.output)
    cache_dir = Path(args.cache) if args.cache else None

    if args.verbose or args.dry_run:
        print(f"compiler: {compiler}")
        print(f"manifests: {len(manifests)}")
        print(f"output:   {output_root}")

    failures: list[tuple[str, str]] = []

    with ThreadPoolExecutor(max_workers=args.parallel) as pool:
        futures = {
            pool.submit(
                compile_one,
                compiler,
                m,
                output_root / m.stem,
                cache_dir,
                naga,
                args.dry_run,
                args.verbose,
            ): m
            for m in manifests
        }
        for future in as_completed(futures):
            ok, manifest_str, error = future.result()
            if not ok:
                failures.append((manifest_str, error))
                sys.stderr.write(f"FAILED: {manifest_str}\n{error}\n")

    if failures:
        sys.stderr.write(f"\n{len(failures)} of {len(manifests)} manifest(s) failed.\n")
        return 1

    if not args.dry_run:
        print(f"compiled {len(manifests)} manifest(s) → {output_root}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
