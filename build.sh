#!/bin/bash
# Cross-platform build script for Exasol Client

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}==================================${NC}"
echo -e "${GREEN}Exasol Client Build Script${NC}"
echo -e "${GREEN}==================================${NC}"

# Detect OS
OS="unknown"
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="linux"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macos"
elif [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "cygwin" ]]; then
    OS="windows"
fi

echo -e "${YELLOW}Detected OS: $OS${NC}"

# Check for CMake
if ! command -v cmake &> /dev/null; then
    echo -e "${RED}ERROR: CMake not found. Please install CMake first.${NC}"
    exit 1
fi

echo -e "${GREEN}CMake version: $(cmake --version | head -n1)${NC}"

# Build type (default: Release)
BUILD_TYPE="${1:-Release}"
echo -e "${YELLOW}Build type: $BUILD_TYPE${NC}"

# Create build directory
BUILD_DIR="build"
if [ -d "$BUILD_DIR" ]; then
    echo -e "${YELLOW}Cleaning existing build directory...${NC}"
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure
echo -e "${GREEN}Configuring project...${NC}"

if [ "$OS" == "macos" ]; then
    # macOS: Try to find OpenSSL from Homebrew
    if [ -d "/opt/homebrew/opt/openssl@3" ]; then
        OPENSSL_PATH="/opt/homebrew/opt/openssl@3"
    elif [ -d "/usr/local/opt/openssl@3" ]; then
        OPENSSL_PATH="/usr/local/opt/openssl@3"
    fi
    
    if [ -n "$OPENSSL_PATH" ]; then
        echo -e "${YELLOW}Using OpenSSL from: $OPENSSL_PATH${NC}"
        cmake .. -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DOPENSSL_ROOT_DIR="$OPENSSL_PATH"
    else
        cmake .. -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
    fi
else
    cmake .. -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
fi

# Build
echo -e "${GREEN}Building project...${NC}"

# Determine number of cores
if [ "$OS" == "linux" ]; then
    CORES=$(nproc)
elif [ "$OS" == "macos" ]; then
    CORES=$(sysctl -n hw.ncpu)
else
    CORES=4
fi

echo -e "${YELLOW}Building with $CORES cores...${NC}"
cmake --build . -j"$CORES"

# Verify build
if [ -f "bin/ExasolClient" ] || [ -f "bin/ExasolClient.exe" ]; then
    echo -e "${GREEN}==================================${NC}"
    echo -e "${GREEN}Build successful!${NC}"
    echo -e "${GREEN}==================================${NC}"
    echo ""
    echo -e "Executable location: ${YELLOW}build/bin/ExasolClient${NC}"
    echo ""
    echo "Run with:"
    echo "  ./build/bin/ExasolClient --help"
    echo ""
else
    echo -e "${RED}Build failed: Executable not found${NC}"
    exit 1
fi
