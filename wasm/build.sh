#!/bin/bash
# Build script for CBF WASM module
# Requires: Emscripten SDK (emsdk) installed and activated

set -e  # Exit on error

echo "=========================================="
echo "Building CBF WASM Module"
echo "=========================================="

# Check if emcc is available
if ! command -v emcc &> /dev/null; then
    echo "Error: emcc not found. Please install and activate Emscripten SDK:"
    echo ""
    echo "  git clone https://github.com/emscripten-core/emsdk.git"
    echo "  cd emsdk"
    echo "  ./emsdk install latest"
    echo "  ./emsdk activate latest"
    echo "  source ./emsdk_env.sh"
    echo ""
    exit 1
fi

echo "Emscripten version:"
emcc --version
echo ""

# Create build directory
BUILD_DIR="build"
if [ -d "$BUILD_DIR" ]; then
    echo "Cleaning existing build directory..."
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with emcmake
echo "Configuring with CMake (Emscripten)..."
emcmake cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
echo ""
echo "Building WASM module..."
emmake make -j$(nproc)

# Check output
echo ""
echo "=========================================="
echo "Build complete!"
echo "=========================================="
echo ""
echo "Output files:"
ls -lh cbf_wasm.{js,wasm} 2>/dev/null || echo "Warning: Output files not found"

echo ""
echo "To use in frontend:"
echo "  1. Copy cbf_wasm.{js,wasm} to frontend/public/wasm/"
echo "  2. Or run: make install"
echo ""
echo "To test in Node.js:"
echo "  node cbf_wasm.js"
echo ""
