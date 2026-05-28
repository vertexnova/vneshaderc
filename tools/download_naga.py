#!/usr/bin/env python3
"""Download a pre-built naga binary for the current macOS architecture.

naga converts SPIR-V to WGSL and is used by compile_shaders.py when vnesc
was built without Tint/Dawn (i.e. VNE_SC_TINT=OFF).

Source: https://github.com/gfx-rs/wgpu/releases  (naga-cli is part of wgpu)

Usage:
    python3 tools/download_naga.py [--version <tag>] [--output <dir>]
"""

import argparse
import hashlib
import os
import platform
import stat
import subprocess
import sys
import tarfile
import urllib.request
import zipfile
from pathlib import Path

_SCRIPT_DIR = Path(__file__).resolve().parent

# wgpu GitHub release tag that ships a naga binary.
# Update when a newer stable release is available.
_DEFAULT_VERSION = "v24.0.0"

# Release asset names on GitHub for macOS aarch64 and x86_64.
# Pattern: wgpu-<version>-<target>.tar.gz  or .zip
_TARGETS = {
    ("Darwin", "arm64"):  "naga-cli-{version}-aarch64-apple-darwin.tar.gz",
    ("Darwin", "x86_64"): "naga-cli-{version}-x86_64-apple-darwin.tar.gz",
}

_GITHUB_BASE = "https://github.com/gfx-rs/wgpu/releases/download/{version}/{asset}"


def _detect_target() -> tuple[str, str]:
    system = platform.system()
    machine = platform.machine()
    if machine == "arm64":
        machine = "arm64"
    return system, machine


def _sha256(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def download_naga(version: str, output_dir: Path, verbose: bool) -> int:
    system, machine = _detect_target()
    key = (system, machine)
    if key not in _TARGETS:
        sys.stderr.write(
            f"error: no pre-built naga binary for {system}/{machine}.\n"
            f"  Install via cargo:  cargo install naga-cli\n"
        )
        return 1

    asset_name = _TARGETS[key].format(version=version)
    url = _GITHUB_BASE.format(version=version, asset=asset_name)
    output_dir.mkdir(parents=True, exist_ok=True)
    archive_path = output_dir / asset_name

    print(f"Downloading {url}")
    try:
        urllib.request.urlretrieve(url, archive_path)
    except Exception as exc:
        sys.stderr.write(f"error: download failed: {exc}\n")
        return 1

    print(f"Extracting to {output_dir}/")
    try:
        if asset_name.endswith(".tar.gz"):
            with tarfile.open(archive_path, "r:gz") as tar:
                # Extract only the naga (or naga-cli) binary
                for member in tar.getmembers():
                    name = Path(member.name).name
                    if name in ("naga", "naga-cli") and member.isfile():
                        member.name = "naga"  # normalise to 'naga'
                        tar.extract(member, path=output_dir)
                        if verbose:
                            print(f"  extracted: {member.name}")
                        break
                else:
                    sys.stderr.write("error: naga binary not found in archive\n")
                    archive_path.unlink(missing_ok=True)
                    return 1
        elif asset_name.endswith(".zip"):
            with zipfile.ZipFile(archive_path) as zf:
                for name in zf.namelist():
                    base = Path(name).name
                    if base in ("naga", "naga-cli", "naga.exe"):
                        dest = output_dir / "naga"
                        with zf.open(name) as src, open(dest, "wb") as dst:
                            dst.write(src.read())
                        if verbose:
                            print(f"  extracted: {name} → naga")
                        break
                else:
                    sys.stderr.write("error: naga binary not found in zip\n")
                    archive_path.unlink(missing_ok=True)
                    return 1
    except Exception as exc:
        sys.stderr.write(f"error: extraction failed: {exc}\n")
        archive_path.unlink(missing_ok=True)
        return 1

    archive_path.unlink(missing_ok=True)

    naga_path = output_dir / "naga"
    current_mode = naga_path.stat().st_mode
    naga_path.chmod(current_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)

    # Quick sanity check
    try:
        result = subprocess.run([str(naga_path), "--version"], capture_output=True, text=True, timeout=10)
        version_str = (result.stdout + result.stderr).strip().splitlines()[0] if result.returncode == 0 else "(unknown)"
        print(f"OK: {naga_path}  ({version_str})")
    except Exception:
        print(f"OK: {naga_path}  (version check skipped)")

    return 0


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Download a pre-built naga binary for macOS (used by compile_shaders.py)"
    )
    parser.add_argument("--version", default=_DEFAULT_VERSION,
                        help=f"wgpu release tag (default: {_DEFAULT_VERSION})")
    parser.add_argument("--output", "-o", metavar="DIR",
                        help="Output directory (default: tools/macos/)")
    parser.add_argument("--verbose", "-v", action="store_true")
    args = parser.parse_args()

    output_dir = Path(args.output) if args.output else _SCRIPT_DIR / "macos"
    return download_naga(args.version, output_dir, args.verbose)


if __name__ == "__main__":
    sys.exit(main())
