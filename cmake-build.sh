#!/bin/bash
# CMake build script with platform-specific OpenSSL detection

set -e

echo "=================================="
echo "Exasol Client - CMake Build"
echo "=================================="

# Check for CMake
if ! command -v cmake &> /dev/null; then
    echo "ERROR: CMake not found. Please install CMake first."
    exit 1
fi

echo "CMake version: $(cmake --version | head -n1)"

# Build type
BUILD_TYPE="${1:-Release}"
echo "Build type: $BUILD_TYPE"

# Clean/create build directory
if [ -d "build" ]; then
    echo "Cleaning build directory..."
    rm -rf build
fi
mkdir build
cd build

# Detect platform and configure
OS="$(uname -s)"
echo "Platform: $OS"

CMAKE_ARGS="-DCMAKE_BUILD_TYPE=$BUILD_TYPE"

if [ "$OS" == "Darwin" ]; then
    # macOS - detect Homebrew OpenSSL
    if [ -d "/opt/homebrew/opt/openssl@3" ]; then
        OPENSSL_PATH="/opt/homebrew/opt/openssl@3"
        echo "Found OpenSSL at: $OPENSSL_PATH (Homebrew Apple Silicon)"
    elif [ -d "/usr/local/opt/openssl@3" ]; then
        OPENSSL_PATH="/usr/local/opt/openssl@3"
        echo "Found OpenSSL at: $OPENSSL_PATH (Homebrew Intel)"
    fi
    
    if [ -n "$OPENSSL_PATH" ]; then
        CMAKE_ARGS="$CMAKE_ARGS -DOPENSSL_ROOT_DIR=$OPENSSL_PATH"
    fi
fi

# Configure
echo ""
echo "Configuring with CMake..."
cmake .. $CMAKE_ARGS

# Build with parallel jobs
if [ "$OS" == "Linux" ]; then
    JOBS=$(nproc)
elif [ "$OS" == "Darwin" ]; then
    JOBS=$(sysctl -n hw.ncpu)
else
    JOBS=4
fi

echo ""
echo "Building with $JOBS jobs..."
cmake --build . -j$JOBS

# Success
echo ""
echo "=================================="
echo "Build successful!"
echo "=================================="
echo ""
echo "Executable: build/bin/ExasolClient"
echo ""
echo "Run with:"
echo "  ./build/bin/ExasolClient --help"
echo ""
