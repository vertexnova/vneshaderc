#!/bin/bash

#==============================================================================
# vnesc Windows Build Script (Git Bash / MSYS)
#==============================================================================
# Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
# Licensed under the Apache License, Version 2.0 (the "License")
#
# Run from a Visual Studio Developer Command Prompt.
#==============================================================================

set -e

JOBS=10
ARGS=()

while [[ $# -gt 0 ]]; do
    case $1 in
        -j|--jobs) [[ -n "$2" && "$2" =~ ^[0-9]+$ ]] && { JOBS="$2"; shift 2; } || { echo "Invalid jobs: $2"; exit 1; } ;;
        -j*) JOBS="${1#-j}"; [[ "$JOBS" =~ ^[0-9]+$ ]] || { echo "Invalid jobs: $JOBS"; exit 1; }; shift ;;
        *) ARGS+=("$1"); shift ;;
    esac
done
set -- "${ARGS[@]}"

usage() {
  echo "Usage: $0 [-t <build_type>] [-a <action>] [-clean] [-j <jobs>] [vnesc options]"
  echo "  -t <build_type>  Debug|Release|RelWithDebInfo|MinSizeRel"
  echo "  -a <action>      configure|build|configure_and_build|test"
  echo "  --dev --with-tests --no-tests --with-examples --with-tint --werror ..."
  exit 1
}

BUILD_TYPE="Debug"
ACTION="configure_and_build"
CLEAN_BUILD=false
WITH_DEV=true
WITH_TESTS=
WITH_EXAMPLES=
WITH_TINT=false
WITH_SPIRVTOOLS=false
WITH_GLSLANG=true
WITH_JSON=true
WARNINGS_AS_ERRORS=false

while [[ $# -gt 0 ]]; do
  case $1 in
    -t|--build-type) BUILD_TYPE="$2"; shift 2 ;;
    -a|--action) ACTION="$2"; shift 2 ;;
    -clean|--clean) CLEAN_BUILD=true; shift ;;
    --dev) WITH_DEV=true; shift ;;
    --no-dev) WITH_DEV=false; shift ;;
    --with-tests) WITH_TESTS=true; shift ;;
    --no-tests) WITH_TESTS=false; shift ;;
    --with-examples) WITH_EXAMPLES=true; shift ;;
    --no-examples) WITH_EXAMPLES=false; shift ;;
    --with-tint) WITH_TINT=true; shift ;;
    --with-spirvtools) WITH_SPIRVTOOLS=true; shift ;;
    --no-glslang) WITH_GLSLANG=false; shift ;;
    --no-json) WITH_JSON=false; shift ;;
    --werror) WARNINGS_AS_ERRORS=true; shift ;;
    -h|--help) usage ;;
    *) echo "Unknown option: $1"; usage ;;
  esac
done

if [[ -z "${WITH_TESTS:-}" ]]; then
  [[ "$WITH_DEV" == true ]] && WITH_TESTS=true || WITH_TESTS=false
fi
if [[ -z "${WITH_EXAMPLES:-}" ]]; then
  [[ "$WITH_DEV" == true ]] && WITH_EXAMPLES=true || WITH_EXAMPLES=false
fi

if ! command -v cl &> /dev/null; then
  echo "Error: Visual Studio compiler 'cl' not found."
  echo "Open a Visual Studio Developer Command Prompt, then re-run this script."
  exit 1
fi
if ! command -v cmake &> /dev/null; then
  echo "Error: CMake not found in PATH"
  exit 1
fi

echo "Windows :: msvc"

PROJECT_ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
BUILD_DIR="$PROJECT_ROOT/build/${BUILD_TYPE}/build-windows-msvc"

vnesc_cmake_flags() {
  echo \
    -DVNE_SC_DEV="$([ "$WITH_DEV" = true ] && echo ON || echo OFF)" \
    -DVNE_SC_TESTS="$([ "$WITH_TESTS" = true ] && echo ON || echo OFF)" \
    -DVNE_SC_EXAMPLES="$([ "$WITH_EXAMPLES" = true ] && echo ON || echo OFF)" \
    -DVNE_SC_GLSLANG="$([ "$WITH_GLSLANG" = true ] && echo ON || echo OFF)" \
    -DVNE_SC_JSON="$([ "$WITH_JSON" = true ] && echo ON || echo OFF)" \
    -DVNE_SC_TINT="$([ "$WITH_TINT" = true ] && echo ON || echo OFF)" \
    -DVNE_SC_SPIRVTOOLS="$([ "$WITH_SPIRVTOOLS" = true ] && echo ON || echo OFF)" \
    -DWARNINGS_AS_ERRORS="$([ "$WARNINGS_AS_ERRORS" = true ] && echo ON || echo OFF)"
}

[[ "$CLEAN_BUILD" == true ]] && rm -rf "$BUILD_DIR"

configure() {
  # shellcheck disable=SC2046
  cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" \
    -G "Visual Studio 17 2022" -A x64 \
    $(vnesc_cmake_flags)
}

case $ACTION in
  configure) configure ;;
  build) configure; cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" --parallel "$JOBS" ;;
  configure_and_build) configure; cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" --parallel "$JOBS" ;;
  test)
    configure
    cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" --parallel "$JOBS"
    ctest --test-dir "$BUILD_DIR" -C "$BUILD_TYPE" --output-on-failure
    ;;
  *) usage ;;
esac

echo ""
echo "=== Build completed successfully ==="
echo "Build directory: $BUILD_DIR"
