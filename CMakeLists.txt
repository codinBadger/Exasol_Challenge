cmake_minimum_required(VERSION 3.10)
project(ExasolClient VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "System: ${CMAKE_SYSTEM_NAME}")

# Find OpenSSL
find_package(OpenSSL REQUIRED)
if(OPENSSL_FOUND)
    message(STATUS "OpenSSL found: ${OPENSSL_VERSION}")
    message(STATUS "OpenSSL include: ${OPENSSL_INCLUDE_DIR}")
    message(STATUS "OpenSSL libraries: ${OPENSSL_LIBRARIES}")
endif()

# Source files
set(CLIENT_SOURCES
    src/main.cpp
    src/ExasolClient.cpp
    src/FileConfigLoader.cpp
    src/CLIConfigLoader.cpp
    src/SocketManager.cpp
    src/SSLManager.cpp
)

# Header files (for IDE organization)
set(CLIENT_HEADERS
    include/ExasolClient.h
    include/IConfigLoader.h
    include/FileConfigLoader.h
    include/CLIConfigLoader.h
    include/ISocketManager.h
    include/SocketManager.h
    include/ISSLManager.h
    include/SSLManager.h
)

# Create executable
add_executable(ExasolClient ${CLIENT_SOURCES} ${CLIENT_HEADERS})

# Include directories
target_include_directories(ExasolClient 
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${OPENSSL_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(ExasolClient 
    PRIVATE 
        OpenSSL::SSL 
        OpenSSL::Crypto
)

# Platform-specific settings
if(WIN32)
    # Windows: link winsock2
    target_link_libraries(ExasolClient PRIVATE ws2_32)
    message(STATUS "Windows detected: linking ws2_32")
    
    # Set console subsystem
    if(MSVC)
        set_target_properties(ExasolClient PROPERTIES
            LINK_FLAGS "/SUBSYSTEM:CONSOLE"
        )
    endif()
elseif(UNIX AND NOT APPLE)
    # Linux: link pthread if needed
    find_package(Threads)
    if(Threads_FOUND)
        target_link_libraries(ExasolClient PRIVATE Threads::Threads)
    endif()
    message(STATUS "Linux detected")
elseif(APPLE)
    # macOS
    message(STATUS "macOS detected")
endif()

# Compiler warnings
if(MSVC)
    target_compile_options(ExasolClient PRIVATE /W4)
else()
    target_compile_options(ExasolClient PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Set output directories
set_target_properties(ExasolClient PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
)

# Installation rules
install(TARGETS ExasolClient
    RUNTIME DESTINATION bin
)

install(FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/config/client.conf
    DESTINATION config
)

install(FILES
    README.md
    KT_DOCUMENT.md
    SOLID_PRINCIPLES.md
    DESTINATION .
)

# Print summary
message(STATUS "")
message(STATUS "=== Configuration Summary ===")
message(STATUS "Project: ${PROJECT_NAME} ${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "System: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "OpenSSL version: ${OPENSSL_VERSION}")
message(STATUS "Output directory: ${CMAKE_BINARY_DIR}/bin")
message(STATUS "============================")
message(STATUS "")
