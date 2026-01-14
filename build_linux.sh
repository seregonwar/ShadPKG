#!/bin/bash
set -e

# Add ~/.local/bin to PATH for conan
export PATH=$PATH:$HOME/.local/bin

# Check if conan is available
if ! command -v conan &> /dev/null; then
    echo "Conan not found in PATH. Trying default location..."
    export PATH=$PATH:$HOME/.local/bin
    if ! command -v conan &> /dev/null; then
        echo "Conan could not be found. Please install it."
        exit 1
    fi
fi

echo "Using Conan version: $(conan --version)"

# Detect profile if not exists
if [ ! -f ~/.conan2/profiles/default ]; then
    echo "Detecting default Conan profile..."
    conan profile detect --force
fi

# Build directory
BUILD_DIR="build_linux"
mkdir -p $BUILD_DIR

# Install dependencies
echo "Installing dependencies with Conan..."
conan install . --output-folder=$BUILD_DIR --build=missing -s build_type=Release

# Find toolchain
TOOLCHAIN_FILE="$BUILD_DIR/build/Release/generators/conan_toolchain.cmake"

if [ ! -f "$TOOLCHAIN_FILE" ]; then
    echo "Toolchain file not found at $TOOLCHAIN_FILE"
    exit 1
fi

# Configure CMake
echo "Configuring CMake..."
cmake -S . -B $BUILD_DIR -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" -DCMAKE_BUILD_TYPE=Release

# Build
echo "Building..."
cmake --build $BUILD_DIR --config=Release

echo "Build complete. Executable should be in $BUILD_DIR"
