# Exasol Client Build Instructions

This document provides detailed build instructions for all supported platforms.

## Prerequisites

### All Platforms
- **CMake** 3.10 or higher
- **C++17** compatible compiler
- **OpenSSL** development libraries

### Platform-Specific Requirements

#### Windows
- **Visual Studio 2019+** or **MinGW-w64** (with g++)
- **OpenSSL**: Install via vcpkg or download pre-built binaries
  ```bash
  # Using vcpkg
  vcpkg install openssl:x64-windows
  ```

#### Linux (Ubuntu/Debian)
```bash
sudo apt-get update
sudo apt-get install cmake g++ libssl-dev
```

#### Linux (Fedora/RHEL/CentOS)
```bash
sudo dnf install cmake gcc-c++ openssl-devel
```

#### macOS
```bash
# Install Homebrew if not already installed
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install cmake openssl
```

---

## Building

### Quick Start (All Platforms)

```bash
# 1. Create build directory
mkdir build
cd build

# 2. Configure with CMake
cmake ..

# 3. Build
cmake --build .

# 4. Run
./bin/ExasolClient --help
```

---

## Detailed Platform Instructions

### Windows with Visual Studio

```powershell
# Create build directory
mkdir build
cd build

# Configure (64-bit)
cmake .. -G "Visual Studio 17 2022" -A x64

# Build (Release)
cmake --build . --config Release

# Build (Debug)
cmake --build . --config Debug

# Run
bin\Release\ExasolClient.exe --help
```

### Windows with MinGW

```bash
# Create build directory
mkdir build
cd build

# Configure
cmake .. -G "MinGW Makefiles"

# Build
cmake --build .

# Run
bin/ExasolClient.exe --help
```

### Linux

```bash
# Create build directory
mkdir build
cd build

# Configure
cmake ..

# Build with multiple cores
cmake --build . -j$(nproc)

# Run
./bin/ExasolClient --help
```

### macOS

```bash
# Create build directory
mkdir build
cd build

# Configure (may need to specify OpenSSL path)
cmake .. -DOPENSSL_ROOT_DIR=/usr/local/opt/openssl

# Or for Apple Silicon with Homebrew
cmake .. -DOPENSSL_ROOT_DIR=/opt/homebrew/opt/openssl

# Build
cmake --build . -j$(sysctl -n hw.ncpu)

# Run
./bin/ExasolClient --help
```

---

## Build Options

### Build Types

```bash
# Debug build (with symbols)
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Release build (optimized)
cmake .. -DCMAKE_BUILD_TYPE=Release

# Release with debug info
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo

# Minimum size release
cmake .. -DCMAKE_BUILD_TYPE=MinSizeRel
```

### Custom OpenSSL Location

```bash
# Specify custom OpenSSL path
cmake .. -DOPENSSL_ROOT_DIR=/path/to/openssl

# Or set include and lib separately
cmake .. \
  -DOPENSSL_INCLUDE_DIR=/path/to/openssl/include \
  -DOPENSSL_LIBRARIES=/path/to/openssl/lib
```

### Install

```bash
# Build and install
cmake --build . --target install

# Or with custom prefix
cmake .. -DCMAKE_INSTALL_PREFIX=/custom/path
cmake --build . --target install
```

---

## Troubleshooting

### OpenSSL Not Found

**Windows:**
```powershell
# Set environment variable
$env:OPENSSL_ROOT_DIR = "C:\path\to\openssl"
cmake ..
```

**Linux/macOS:**
```bash
export OPENSSL_ROOT_DIR=/path/to/openssl
cmake ..
```

### Windows: SOCKET Type Issues

If you encounter `SOCKET` type errors on Windows:
- Ensure `winsock2.h` is included before other headers
- The CMakeLists.txt handles this automatically

### macOS: OpenSSL from Homebrew

```bash
# Apple Silicon
cmake .. -DOPENSSL_ROOT_DIR=/opt/homebrew/opt/openssl@3

# Intel Mac
cmake .. -DOPENSSL_ROOT_DIR=/usr/local/opt/openssl@3
```

### Linux: Missing libssl-dev

```bash
# Ubuntu/Debian
sudo apt-get install libssl-dev

# Fedora/RHEL
sudo dnf install openssl-devel
```

---

## Verification

After building, verify the executable:

```bash
# Show help
./bin/ExasolClient --help

# Check linked libraries (Linux)
ldd ./bin/ExasolClient

# Check linked libraries (macOS)
otool -L ./bin/ExasolClient

# Check dependencies (Windows with MinGW)
ldd bin/ExasolClient.exe
```

---

## Clean Build

```bash
# Remove build directory
cd ..
rm -rf build

# Or on Windows
cd ..
rmdir /s /q build

# Start fresh
mkdir build
cd build
cmake ..
cmake --build .
```

---

## IDE Support

### Visual Studio Code

Install the **CMake Tools** extension, then:
1. Open project folder
2. Press `Ctrl+Shift+P` → "CMake: Configure"
3. Press `F7` to build

### CLion

1. Open project folder (CLion auto-detects CMakeLists.txt)
2. Click "Build" → "Build Project"

### Visual Studio

1. Open CMakeLists.txt as a CMake project
2. Select target and build configuration
3. Build → Build All

---

## CI/CD Integration

### GitHub Actions Example

```yaml
name: Build

on: [push, pull_request]

jobs:
  build:
    strategy:
      matrix:
        os: [ubuntu-latest, windows-latest, macos-latest]
    
    runs-on: ${{ matrix.os }}
    
    steps:
    - uses: actions/checkout@v2
    
    - name: Install dependencies (Ubuntu)
      if: matrix.os == 'ubuntu-latest'
      run: sudo apt-get install libssl-dev
    
    - name: Install dependencies (macOS)
      if: matrix.os == 'macos-latest'
      run: brew install openssl
    
    - name: Configure
      run: cmake -B build
    
    - name: Build
      run: cmake --build build
    
    - name: Test
      run: ./build/bin/ExasolClient --help
```

---

## Build Output

After successful build:

```
build/
└── bin/
    └── ExasolClient[.exe]    # Executable
```

Configuration files remain in:
```
config/
└── client.conf               # Configuration template
```

---

## Next Steps

After building:
1. Configure your connection: `config/client.conf`
2. Run the client: See [README.md](README.md)
3. Review architecture: See [SOLID_PRINCIPLES.md](SOLID_PRINCIPLES.md)
