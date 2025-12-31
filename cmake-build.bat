@echo off
REM CMake build script for Windows with MSYS2/MinGW

echo ==================================
echo Exasol Client - CMake Build
echo ==================================

REM Check for CMake
where cmake >nul 2>nul
if %errorlevel% neq 0 (
    echo ERROR: CMake not found. Please install CMake first.
    exit /b 1
)

REM Build type (default: Release)
set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Release

echo Build type: %BUILD_TYPE%

REM Create/clean build directory
if exist build (
    echo Cleaning build directory...
    rmdir /s /q build
)
mkdir build
cd build

REM Configure with MinGW and MSYS2 OpenSSL
echo Configuring with CMake...
cmake .. -G "MinGW Makefiles" ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DOPENSSL_ROOT_DIR=C:/msys64/ucrt64

if %errorlevel% neq 0 (
    echo Configuration failed!
    exit /b 1
)

REM Build
echo.
echo Building project...
cmake --build . --config %BUILD_TYPE%

if %errorlevel% neq 0 (
    echo Build failed!
    exit /b 1
)

REM Success
echo.
echo ==================================
echo Build successful!
echo ==================================
echo.
echo Executable: build\bin\ExasolClient.exe
echo.
echo Run with:
echo   build\bin\ExasolClient.exe --help
echo.
