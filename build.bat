@echo off
REM Cross-platform build script for Exasol Client (Windows)

echo ==================================
echo Exasol Client Build Script
echo ==================================

REM Check for CMake
where cmake >nul 2>nul
if %errorlevel% neq 0 (
    echo ERROR: CMake not found. Please install CMake first.
    exit /b 1
)

echo CMake found
cmake --version

REM Build type (default: Release)
set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Release
echo Build type: %BUILD_TYPE%

REM Create build directory
set BUILD_DIR=build
if exist "%BUILD_DIR%" (
    echo Cleaning existing build directory...
    rmdir /s /q "%BUILD_DIR%"
)

mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%"

REM Configure
echo Configuring project...

REM Try to detect Visual Studio
where cl >nul 2>nul
if %errorlevel% equ 0 (
    echo Using MSVC compiler
    cmake .. -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
) else (
    REM Try MinGW
    where g++ >nul 2>nul
    if %errorlevel% equ 0 (
        echo Using MinGW compiler
        cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
    ) else (
        echo ERROR: No suitable compiler found (MSVC or MinGW required)
        exit /b 1
    )
)

if %errorlevel% neq 0 (
    echo Configuration failed
    exit /b 1
)

REM Build
echo Building project...
cmake --build . --config %BUILD_TYPE%

if %errorlevel% neq 0 (
    echo Build failed
    exit /b 1
)

REM Verify build
if exist "bin\ExasolClient.exe" (
    echo ==================================
    echo Build successful!
    echo ==================================
    echo.
    echo Executable location: build\bin\ExasolClient.exe
    echo.
    echo Run with:
    echo   build\bin\ExasolClient.exe --help
    echo.
) else (
    echo Build failed: Executable not found
    exit /b 1
)
