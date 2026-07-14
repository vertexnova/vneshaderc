#!/bin/bash

#==============================================================================
# vnesc Documentation Generation Script
# Drives Doxygen API docs via CMake target vnesc_doc_doxygen.
#==============================================================================
# Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
# Licensed under the Apache License, Version 2.0 (the "License")
#==============================================================================

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build/docs"
DOXYGEN_HTML="$BUILD_DIR/docs/html"

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

check_prerequisites() {
    log_info "Checking prerequisites..."

    if [[ ! -f "$PROJECT_ROOT/CMakeLists.txt" ]]; then
        log_error "Not in vnesc project root."
        exit 1
    fi

    if ! command -v cmake &> /dev/null; then
        log_error "CMake not found."
        exit 1
    fi

    if ! command -v doxygen &> /dev/null; then
        log_warning "Doxygen not found - API docs will be skipped."
        DOXYGEN_AVAILABLE=false
    else
        DOXYGEN_AVAILABLE=true
    fi
}

generate_api_docs() {
    if [[ "$DOXYGEN_AVAILABLE" != true ]]; then
        return
    fi

    log_info "Generating vnesc API documentation..."
    mkdir -p "$BUILD_DIR"
    cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" \
        -DENABLE_DOXYGEN=ON \
        -DVNE_SC_TESTS=OFF \
        -DVNE_SC_EXAMPLES=OFF
    cmake --build "$BUILD_DIR" --target vnesc_doc_doxygen

    if [[ -f "$DOXYGEN_HTML/index.html" ]]; then
        log_success "API docs: $DOXYGEN_HTML/index.html"
    else
        log_warning "Expected output not found at $DOXYGEN_HTML/index.html"
    fi
}

main() {
    echo "vnesc Documentation Generator"
    echo "=============================="
    check_prerequisites
    generate_api_docs
    log_success "Done."
}

main "$@"
