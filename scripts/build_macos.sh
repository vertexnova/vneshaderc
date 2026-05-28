#!/bin/bash

#==============================================================================
# vnesc macOS Build Script
#==============================================================================
# Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
# Licensed under the Apache License, Version 2.0 (the "License")
#
# Builds vnesc on macOS (Ninja/Make or optional Xcode project).
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

PLATFORM="Darwin"
COMPILER="clang"

usage() {
  echo "Usage: $0 [-t <build_type>] [-a <action>] [-clean] [-j <jobs>] [-xcode] [options]"
  echo "  -t <build_type>  Debug|Release|RelWithDebInfo|MinSizeRel"
  echo "  -a <action>      configure|build|configure_and_build|test|xcode|xcode_build"
  echo "  -xcode           Generate Xcode project (vnesc.xcodeproj)"
  echo "  -xcode-only      Configure Xcode project only"
  echo ""
  echo "vnesc options: --dev --with-tests --no-tests --with-examples --no-examples"
  echo "               --no-tint --no-spirvtools --no-glslang --no-json --werror"
  echo "               (Tint and SPIRV-Tools are ON by default)"
  exit 1
}

BUILD_TYPE="Debug"
ACTION="configure_and_build"
CLEAN_BUILD=false
GENERATE_XCODE=false
WITH_DEV=true
WITH_TESTS=true
WITH_EXAMPLES=false
TESTS_EXPLICIT=false
EXAMPLES_EXPLICIT=false
WITH_TINT=true
WITH_SPIRVTOOLS=true
WITH_GLSLANG=true
WITH_JSON=true
WARNINGS_AS_ERRORS=false

while [[ $# -gt 0 ]]; do
  case $1 in
    -t|--build-type) BUILD_TYPE="$2"; shift 2 ;;
    -a|--action) ACTION="$2"; shift 2 ;;
    -clean|--clean) CLEAN_BUILD=true; shift ;;
    -xcode|--xcode) GENERATE_XCODE=true; shift ;;
    -xcode-only|--xcode-only) GENERATE_XCODE=true; ACTION="xcode"; shift ;;
    --dev) WITH_DEV=true; shift ;;
    --with-tests) WITH_TESTS=true; TESTS_EXPLICIT=true; shift ;;
    --no-tests) WITH_TESTS=false; TESTS_EXPLICIT=true; shift ;;
    --with-examples) WITH_EXAMPLES=true; EXAMPLES_EXPLICIT=true; shift ;;
    --no-examples) WITH_EXAMPLES=false; EXAMPLES_EXPLICIT=true; shift ;;
    --with-tint) WITH_TINT=true; shift ;;
    --with-spirvtools) WITH_SPIRVTOOLS=true; shift ;;
    --no-tint) WITH_TINT=false; shift ;;
    --no-spirvtools) WITH_SPIRVTOOLS=false; shift ;;
    --no-glslang) WITH_GLSLANG=false; shift ;;
    --no-json) WITH_JSON=false; shift ;;
    --werror) WARNINGS_AS_ERRORS=true; shift ;;
    -h|--help) usage ;;
    *) echo "Unknown option: $1"; usage ;;
  esac
done

if [[ "$WITH_DEV" == true ]]; then
  [[ "$TESTS_EXPLICIT" == false ]] && WITH_TESTS=true
  [[ "$EXAMPLES_EXPLICIT" == false ]] && WITH_EXAMPLES=true
fi
[[ "$GENERATE_XCODE" == true && "$ACTION" == "configure_and_build" ]] && ACTION="xcode_build"

COMPILER_VERSION=$(clang --version | head -n 1 | awk '{print $4}' | sed 's/(.*)//')
[[ "$COMPILER_VERSION" == "version" ]] && COMPILER_VERSION=$(clang --version | head -n 1 | awk '{print $3}')

echo "$PLATFORM :: $COMPILER-${COMPILER_VERSION}"

PROJECT_ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
if [[ "$GENERATE_XCODE" == true ]]; then
  BUILD_DIR="$PROJECT_ROOT/build/${BUILD_TYPE}/xcode-macos-$COMPILER-${COMPILER_VERSION}"
else
  BUILD_DIR="$PROJECT_ROOT/build/${BUILD_TYPE}/build-macos-$COMPILER-${COMPILER_VERSION}"
fi

vnesc_cmake_flags() {
  echo \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15 \
    -DVNE_SC_DEV="$([ "$WITH_DEV" = true ] && echo ON || echo OFF)" \
    -DVNE_SC_TESTS="$([ "$WITH_TESTS" = true ] && echo ON || echo OFF)" \
    -DVNE_SC_EXAMPLES="$([ "$WITH_EXAMPLES" = true ] && echo ON || echo OFF)" \
    -DVNE_SC_GLSLANG="$([ "$WITH_GLSLANG" = true ] && echo ON || echo OFF)" \
    -DVNE_SC_JSON="$([ "$WITH_JSON" = true ] && echo ON || echo OFF)" \
    -DVNE_SC_TINT="$([ "$WITH_TINT" = true ] && echo ON || echo OFF)" \
    -DVNE_SC_SPIRVTOOLS="$([ "$WITH_SPIRVTOOLS" = true ] && echo ON || echo OFF)" \
    -DWARNINGS_AS_ERRORS="$([ "$WARNINGS_AS_ERRORS" = true ] && echo ON || echo OFF)"
}

GENERATOR_FLAGS=()
[[ "$GENERATE_XCODE" == true ]] && GENERATOR_FLAGS=(-G Xcode)

[[ "$CLEAN_BUILD" == true ]] && rm -rf "$BUILD_DIR"

configure() {
  # shellcheck disable=SC2046
  cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" "${GENERATOR_FLAGS[@]}" $(vnesc_cmake_flags)
}

case $ACTION in
  configure) configure ;;
  build) configure; cmake --build "$BUILD_DIR" --parallel "$JOBS" ;;
  configure_and_build) configure; cmake --build "$BUILD_DIR" --parallel "$JOBS" ;;
  test) configure; cmake --build "$BUILD_DIR" --parallel "$JOBS"; ctest --test-dir "$BUILD_DIR" --output-on-failure ;;
  xcode) configure; echo "Xcode project: $BUILD_DIR/vnesc.xcodeproj" ;;
  xcode_build)
    configure
    xcodebuild -project "$BUILD_DIR/vnesc.xcodeproj" -configuration "$BUILD_TYPE" -parallelizeTargets -jobs "$JOBS"
    ;;
  *) usage ;;
esac

echo ""
echo "=== Build completed successfully ==="
echo "Build directory: $BUILD_DIR"
