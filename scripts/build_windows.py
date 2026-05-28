#!/usr/bin/env python3
"""
vnesc Windows Build Script

Configure, build, and test vnesc from a Visual Studio Developer Command Prompt.
"""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path


def check_visual_studio_env() -> bool:
    try:
        subprocess.run(["cl"], capture_output=True, check=False)
        return True
    except FileNotFoundError:
        return False


def check_cmake() -> bool:
    try:
        subprocess.run(["cmake", "--version"], capture_output=True, check=True)
        return True
    except (subprocess.CalledProcessError, FileNotFoundError):
        return False


def vnesc_cmake_args(args: argparse.Namespace) -> list[str]:
    dev = args.dev
    tests = args.with_tests if args.with_tests is not None else dev
    examples = args.with_examples if args.with_examples is not None else dev
    return [
        f"-DVNE_SC_DEV={'ON' if dev else 'OFF'}",
        f"-DVNE_SC_TESTS={'ON' if tests else 'OFF'}",
        f"-DVNE_SC_EXAMPLES={'ON' if examples else 'OFF'}",
        f"-DVNE_SC_GLSLANG={'OFF' if args.no_glslang else 'ON'}",
        f"-DVNE_SC_JSON={'OFF' if args.no_json else 'ON'}",
        f"-DVNE_SC_TINT={'OFF' if args.no_tint else 'ON'}",
        f"-DVNE_SC_SPIRVTOOLS={'OFF' if args.no_spirvtools else 'ON'}",
        f"-DWARNINGS_AS_ERRORS={'ON' if args.werror else 'OFF'}",
    ]


def main() -> None:
    parser = argparse.ArgumentParser(description="Build vnesc for Windows")
    parser.add_argument("-t", "--build-type", default="Debug",
                        choices=["Debug", "Release", "RelWithDebInfo", "MinSizeRel"])
    parser.add_argument("-a", "--action", default="configure_and_build",
                        choices=["configure", "build", "configure_and_build", "test"])
    parser.add_argument("-j", "--jobs", type=int, default=10)
    parser.add_argument("--clean", action="store_true")
    parser.add_argument("--dev", action="store_true", default=True,
                        help="VNE_SC_DEV=ON (default)")
    parser.add_argument("--no-dev", action="store_false", dest="dev")
    parser.add_argument("--with-tests", action="store_true", default=None)
    parser.add_argument("--no-tests", action="store_false", dest="with_tests")
    parser.add_argument("--with-examples", action="store_true", default=None)
    parser.add_argument("--no-examples", action="store_false", dest="with_examples")
    parser.add_argument("--with-tint", action="store_true",
                        help="VNE_SC_TINT=ON (enabled by default)")
    parser.add_argument("--with-spirvtools", action="store_true",
                        help="VNE_SC_SPIRVTOOLS=ON (enabled by default)")
    parser.add_argument("--no-tint", action="store_true", help="VNE_SC_TINT=OFF")
    parser.add_argument("--no-spirvtools", action="store_true", help="VNE_SC_SPIRVTOOLS=OFF")
    parser.add_argument("--no-glslang", action="store_true")
    parser.add_argument("--no-json", action="store_true")
    parser.add_argument("--werror", action="store_true")
    cli = parser.parse_args()

    if not check_visual_studio_env():
        print("Error: 'cl' not found. Run from a Visual Studio Developer Command Prompt.")
        sys.exit(1)
    if not check_cmake():
        print("Error: CMake not found in PATH")
        sys.exit(1)

    project_root = Path(__file__).resolve().parent.parent
    build_dir = project_root / "build" / cli.build_type / "build-windows-msvc"

    if cli.clean and build_dir.exists():
        shutil.rmtree(build_dir)

    configure_cmd = [
        "cmake", "-S", str(project_root), "-B", str(build_dir),
        "-G", "Visual Studio 17 2022", "-A", "x64",
        *vnesc_cmake_args(cli),
    ]

    print("Windows :: msvc")
    print("Configure:", " ".join(configure_cmd))

    if cli.action in ("configure", "build", "configure_and_build", "test"):
        subprocess.run(configure_cmd, check=True)

    if cli.action in ("build", "configure_and_build", "test"):
        subprocess.run(
            ["cmake", "--build", str(build_dir), "--config", cli.build_type,
             "--parallel", str(cli.jobs)],
            check=True,
        )

    if cli.action == "test":
        subprocess.run(
            ["ctest", "--test-dir", str(build_dir), "-C", cli.build_type,
             "--output-on-failure"],
            check=True,
        )

    print("\n=== Build completed successfully ===")
    print(f"Build directory: {build_dir}")


if __name__ == "__main__":
    main()
