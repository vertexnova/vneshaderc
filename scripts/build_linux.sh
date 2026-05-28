#!/bin/bash

#==============================================================================
# vnesc Linux Build Script
#==============================================================================
# Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
# Licensed under the Apache License, Version 2.0 (the "License")
#
# Builds vnesc on Linux (GCC or Clang). Desktop only — no mobile/web targets.
#==============================================================================

set -e

JOBS=10
ARGS=()

while [[ $# -gt 0 ]]; do
    case $1 in
        -j|--jobs)
            if [[ -n "$2" && "$2" =~ ^[0-9]+$ ]]; then
                JOBS="$2"
                shift 2
            else
                echo "Invalid number of jobs: $2"
                exit 1
            fi
            ;;
        -j*)
            JOBS="${1#-j}"
            if [[ ! "$JOBS" =~ ^[0-9]+$ ]]; then
                echo "Invalid number of jobs: $JOBS"
                exit 1
            fi
            shift
            ;;
        *)
            ARGS+=("$1")
            shift
            ;;
    esac
done
set -- "${ARGS[@]}"

PLATFORM="Linux"
COMPILER="gcc"

usage() {
  echo "Usage: $0 [-t <build_type>] [-a <action>] [-c <compiler>] [-clean] [-interactive] [-j <jobs>] [options]"
  echo ""
  echo "  -t <build_type>   Debug|Release|RelWithDebInfo|MinSizeRel (default: Debug)"
  echo "  -a <action>       configure|build|configure_and_build|test"
  echo "  -c <compiler>     gcc|clang (default: gcc)"
  echo "  -clean            Remove build directory before configure"
  echo "  -interactive      Prompt for options"
  echo "  -j <jobs>         Parallel build jobs (default: 10)"
  echo ""
  echo "vnesc CMake options:"
  echo "  --dev             VNE_SC_DEV=ON (tests + examples)"
  echo "  --with-tests      VNE_SC_TESTS=ON (default with --dev)"
  echo "  --no-tests        VNE_SC_TESTS=OFF"
  echo "  --with-examples   VNE_SC_EXAMPLES=ON (default with --dev)"
  echo "  --no-examples     VNE_SC_EXAMPLES=OFF"
  echo "  --no-tint         VNE_SC_TINT=OFF (default: ON; first configure is slow)"
  echo "  --no-spirvtools   VNE_SC_SPIRVTOOLS=OFF (default: ON)"
  echo "  --with-tint       VNE_SC_TINT=ON (redundant; enabled by default)"
  echo "  --with-spirvtools VNE_SC_SPIRVTOOLS=ON (redundant; enabled by default)"
  echo "  --no-glslang      VNE_SC_GLSLANG=OFF"
  echo "  --no-json         VNE_SC_JSON=OFF"
  echo "  --werror          WARNINGS_AS_ERRORS=ON"
  echo ""
  echo "Examples:"
  echo "  $0 --dev -t Release -a test"
  echo "  $0 -c clang -j 16 --with-tint"
  exit 1
}

interactive_mode() {
  echo "=== vnesc Interactive Build (Linux) ==="
  echo ""
  read -p "Build type [Debug]: " bt
  BUILD_TYPE="${bt:-Debug}"
  read -p "Compiler (gcc/clang) [gcc]: " cc
  COMPILER="${cc:-gcc}"
  read -p "Action (configure|build|configure_and_build|test) [configure_and_build]: " act
  ACTION="${act:-configure_and_build}"
  read -p "Enable --dev (tests+examples)? [Y/n]: " dev_choice
  if [[ ! "$dev_choice" =~ ^[Nn]$ ]]; then
    WITH_DEV=true
  else
    WITH_DEV=false
    WITH_TESTS=false
    WITH_EXAMPLES=false
    TESTS_EXPLICIT=true
    EXAMPLES_EXPLICIT=true
  fi
}

BUILD_TYPE="Debug"
ACTION="configure_and_build"
COMPILER="gcc"
CLEAN_BUILD=false
INTERACTIVE_MODE=false
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
    -c|--compiler) COMPILER="$2"; shift 2 ;;
    -clean|--clean) CLEAN_BUILD=true; shift ;;
    -interactive|--interactive) INTERACTIVE_MODE=true; shift ;;
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

[[ "$INTERACTIVE_MODE" == true ]] && interactive_mode

if [[ "$WITH_DEV" == true ]]; then
  [[ "$TESTS_EXPLICIT" == false ]] && WITH_TESTS=true
  [[ "$EXAMPLES_EXPLICIT" == false ]] && WITH_EXAMPLES=true
fi

if [[ "$COMPILER" != "gcc" && "$COMPILER" != "clang" ]]; then
  echo "Unsupported compiler: $COMPILER (use gcc or clang)"
  exit 1
fi

if [[ "$COMPILER" == "gcc" ]]; then
  COMPILER_VERSION=$(gcc --version | head -n 1 | awk '{print $4}')
else
  COMPILER_VERSION=$(clang --version | head -n 1 | awk '{print $3}')
fi

echo "$PLATFORM :: $COMPILER-${COMPILER_VERSION}"

PROJECT_ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
BUILD_DIR="$PROJECT_ROOT/build/${BUILD_TYPE}/build-linux-$COMPILER-${COMPILER_VERSION}"

vnesc_cmake_flags() {
  local flags=(
    "-DCMAKE_BUILD_TYPE=$BUILD_TYPE"
    "-DVNE_SC_DEV=$([ "$WITH_DEV" = true ] && echo ON || echo OFF)"
    "-DVNE_SC_TESTS=$([ "$WITH_TESTS" = true ] && echo ON || echo OFF)"
    "-DVNE_SC_EXAMPLES=$([ "$WITH_EXAMPLES" = true ] && echo ON || echo OFF)"
    "-DVNE_SC_GLSLANG=$([ "$WITH_GLSLANG" = true ] && echo ON || echo OFF)"
    "-DVNE_SC_JSON=$([ "$WITH_JSON" = true ] && echo ON || echo OFF)"
    "-DVNE_SC_TINT=$([ "$WITH_TINT" = true ] && echo ON || echo OFF)"
    "-DVNE_SC_SPIRVTOOLS=$([ "$WITH_SPIRVTOOLS" = true ] && echo ON || echo OFF)"
    "-DWARNINGS_AS_ERRORS=$([ "$WARNINGS_AS_ERRORS" = true ] && echo ON || echo OFF)"
  )
  if [[ "$COMPILER" == "gcc" ]]; then
    flags+=("-DCMAKE_C_COMPILER=gcc" "-DCMAKE_CXX_COMPILER=g++")
  else
    flags+=("-DCMAKE_C_COMPILER=clang" "-DCMAKE_CXX_COMPILER=clang++" "-DENABLE_IPO=OFF")
  fi
  echo "${flags[@]}"
}

CONFIGURE_COMMAND=(cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" $(vnesc_cmake_flags))
BUILD_COMMAND=(cmake --build "$BUILD_DIR" --parallel "$JOBS")
TEST_COMMAND=(ctest --test-dir "$BUILD_DIR" --output-on-failure)

if [[ "$CLEAN_BUILD" == true ]]; then
  rm -rf "$BUILD_DIR"
fi

case $ACTION in
  configure)
    "${CONFIGURE_COMMAND[@]}"
    ;;
  build)
    "${CONFIGURE_COMMAND[@]}"
    "${BUILD_COMMAND[@]}"
    ;;
  configure_and_build)
    "${CONFIGURE_COMMAND[@]}"
    "${BUILD_COMMAND[@]}"
    ;;
  test)
    "${CONFIGURE_COMMAND[@]}"
    "${BUILD_COMMAND[@]}"
    "${TEST_COMMAND[@]}"
    ;;
  *)
    usage
    ;;
esac

echo ""
echo "=== Build completed successfully ==="
echo "Build directory: $BUILD_DIR"
