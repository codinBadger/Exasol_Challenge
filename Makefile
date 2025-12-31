# Makefile for Exasol Client
# Cross-platform build without CMake

# Compiler
CXX ?= g++

# Compiler flags
CXXFLAGS = -std=c++17 -Wall -Wextra -I include

# Build type (default: Release)
BUILD_TYPE ?= Release

ifeq ($(BUILD_TYPE),Debug)
    CXXFLAGS += -g -O0 -DDEBUG
else ifeq ($(BUILD_TYPE),Release)
    CXXFLAGS += -O3 -DNDEBUG
else
    $(error Invalid BUILD_TYPE. Use Debug or Release)
endif

# Platform detection
PLATFORM := $(shell uname -s 2>/dev/null || echo Windows)

# Libraries
LDFLAGS = -lssl -lcrypto

# Windows-specific
ifeq ($(findstring Windows,$(PLATFORM)),Windows)
    LDFLAGS += -lws2_32
    EXE_EXT = .exe
    RM = del /Q
    MKDIR = if not exist bin mkdir bin
else ifeq ($(findstring MINGW,$(PLATFORM)),MINGW)
    LDFLAGS += -lws2_32
    EXE_EXT = .exe
    RM = rm -f
    MKDIR = mkdir -p bin
else ifeq ($(findstring MSYS,$(PLATFORM)),MSYS)
    LDFLAGS += -lws2_32
    EXE_EXT = .exe
    RM = rm -f
    MKDIR = mkdir -p bin
else
    # Unix/Linux/macOS
    EXE_EXT =
    RM = rm -f
    MKDIR = mkdir -p bin
    
    # macOS: Add OpenSSL paths from Homebrew
    ifeq ($(PLATFORM),Darwin)
        ifneq ($(wildcard /opt/homebrew/opt/openssl@3),)
            CXXFLAGS += -I/opt/homebrew/opt/openssl@3/include
            LDFLAGS += -L/opt/homebrew/opt/openssl@3/lib
        else ifneq ($(wildcard /usr/local/opt/openssl@3),)
            CXXFLAGS += -I/usr/local/opt/openssl@3/include
            LDFLAGS += -L/usr/local/opt/openssl@3/lib
        endif
    endif
endif

# Target executable
TARGET = bin/ExasolClient$(EXE_EXT)

# Source files
SOURCES = \
    src/main.cpp \
    src/ExasolClient.cpp \
    src/FileConfigLoader.cpp \
    src/CLIConfigLoader.cpp \
    src/SocketManager.cpp \
    src/SSLManager.cpp

# Object files
OBJECTS = $(SOURCES:.cpp=.o)

# Header files
HEADERS = \
    include/ExasolClient.h \
    include/IConfigLoader.h \
    include/FileConfigLoader.h \
    include/CLIConfigLoader.h \
    include/ISocketManager.h \
    include/SocketManager.h \
    include/ISSLManager.h \
    include/SSLManager.h

# Default target
.PHONY: all
all: info $(TARGET)

# Display build information
.PHONY: info
info:
	@echo "==================================="
	@echo "Building Exasol Client"
	@echo "==================================="
	@echo "Platform: $(PLATFORM)"
	@echo "Compiler: $(CXX)"
	@echo "Build Type: $(BUILD_TYPE)"
	@echo "Flags: $(CXXFLAGS)"
	@echo "==================================="

# Create bin directory
bin:
	@$(MKDIR)

# Link executable
$(TARGET): $(OBJECTS) | bin
	@echo "Linking $(TARGET)..."
	$(CXX) $(OBJECTS) -o $(TARGET) $(LDFLAGS)
	@echo "Build successful!"
	@echo "Run: $(TARGET) --help"

# Compile source files
%.o: %.cpp $(HEADERS)
	@echo "Compiling $<..."
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean build artifacts
.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	$(RM) $(OBJECTS) $(TARGET)
	@echo "Clean complete"

# Clean everything including bin directory
.PHONY: distclean
distclean: clean
	@echo "Removing bin directory..."
ifeq ($(findstring Windows,$(PLATFORM)),Windows)
	@if exist bin rmdir /Q /S bin
else
	@rm -rf bin
endif
	@echo "Distclean complete"

# Rebuild
.PHONY: rebuild
rebuild: clean all

# Install (copy to system location)
.PHONY: install
install: $(TARGET)
	@echo "Installing $(TARGET)..."
ifeq ($(findstring Windows,$(PLATFORM)),Windows)
	@echo "Manual installation required on Windows"
	@echo "Copy $(TARGET) to desired location"
else
	install -m 755 $(TARGET) /usr/local/bin/ExasolClient
	@echo "Installed to /usr/local/bin/ExasolClient"
endif

# Run help
.PHONY: run-help
run-help: $(TARGET)
	$(TARGET) --help

# Display help
.PHONY: help
help:
	@echo "Exasol Client Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all          - Build the project (default)"
	@echo "  clean        - Remove object files and executable"
	@echo "  distclean    - Remove all build artifacts"
	@echo "  rebuild      - Clean and build"
	@echo "  install      - Install to system (Unix/Linux/macOS)"
	@echo "  run-help     - Build and run --help"
	@echo "  help         - Show this help message"
	@echo ""
	@echo "Variables:"
	@echo "  BUILD_TYPE   - Debug or Release (default: Release)"
	@echo "  CXX          - C++ compiler (default: g++)"
	@echo ""
	@echo "Examples:"
	@echo "  make                          # Build release"
	@echo "  make BUILD_TYPE=Debug         # Build debug"
	@echo "  make CXX=clang++              # Use clang++"
	@echo "  make clean                    # Clean build"
	@echo "  make rebuild BUILD_TYPE=Debug # Rebuild debug"
