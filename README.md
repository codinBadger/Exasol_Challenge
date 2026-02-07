# Exasol Client

A C++ TLS/SSL client for connecting to the Exasol database server with secure encrypted communication.

## Features

- ✅ **Secure TLS/SSL** communication with certificate validation
- ✅ **Cross-platform** support (Windows, Linux, macOS)
- ✅ **OOP Architecture** following SOLID principles
- ✅ **Flexible Configuration** (CLI arguments or config file)
- ✅ **Modular Design** with dependency injection
- ✅ **Easy to Test** with interface-based architecture
- ✅ **Extensible** without modifying existing code

## Architecture

Built with modern C++ (C++17) following industry best practices:
- **SOLID Principles**: All five principles implemented
- **Design Patterns**: Dependency Injection, Pimpl, Factory
- **Interface-Based**: Easy to mock and test
- **Modular Components**: Config loaders, Socket manager, SSL manager

See [SOLID_PRINCIPLES.md](SOLID_PRINCIPLES.md) for detailed architecture documentation.

## Project Structure

- `src/` - Implementation files
- `include/` - Header files with interfaces and classes
- `config/` - Configuration templates
- `bin/` - Compiled executables

## Building

The project supports multiple build systems for maximum cross-platform compatibility:

### Option 1: CMake Helper Scripts (Easiest)

**Windows (MSYS2/MinGW):**
```bash
cmake-build.bat          # Release build
cmake-build.bat Debug    # Debug build
```

**Unix/Linux/macOS:**
```bash
chmod +x cmake-build.sh
./cmake-build.sh         # Release build
./cmake-build.sh Debug   # Debug build
```

These scripts automatically detect OpenSSL and configure everything for you.

### Option 2: CMake (Manual)

**Windows with MSYS2:**
```bash
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DOPENSSL_ROOT_DIR=C:/msys64/ucrt64
cmake --build .
```

**Linux:**
```bash
mkdir build && cd build
cmake ..
cmake --build .
```

**macOS:**
```bash
mkdir build && cd build
cmake .. -DOPENSSL_ROOT_DIR=/opt/homebrew/opt/openssl@3
cmake --build .
```

See [BUILD.md](BUILD.md) for detailed platform-specific instructions.

### Option 3: Makefile (Unix/Linux/macOS/MinGW)

```bash
make                     # Release build
make BUILD_TYPE=Debug    # Debug build
```

### Option 4: Direct g++ Compilation

```bash
g++ -std=c++17 -I include -o bin/ExasolClient \
    src/main.cpp \
    src/FileConfigLoader.cpp \
    src/CLIConfigLoader.cpp \
    src/SocketManager.cpp \
    src/SSLManager.cpp \
    src/ExasolClient.cpp \
    -lssl -lcrypto -lws2_32
```

## Running

### Option 1: Command-Line Arguments

```bash
./bin/ExasolClient <server_address> <port> <ca_cert.pem>
```

Example:
```bash
./bin/ExasolClient 127.0.0.1 8443 "C:\certs\ca_cert.pem"
```

### Option 2: Configuration File

```bash
./bin/ExasolClient --config config/client.conf
```

## Configuration Format

Create a file (e.g., `config/client.conf`):

```properties
# Exasol Client Configuration
server_address=127.0.0.1
port=8443
ca_cert=C:\path\to\ca_cert.pem
```


## Requirements

- **C++17** compiler (g++, MSVC, clang)
- **OpenSSL** libraries (libssl, libcrypto)
- **Platform**: Windows (Winsock2), Linux, or macOS (POSIX sockets)
