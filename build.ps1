# Conway's Game of Life - Build Script for Windows
# Run this script with: .\build.ps1

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Conway's Game of Life - CUDA Build Script" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Check if CUDA is installed
$cudaPath = $env:CUDA_PATH
if (-not $cudaPath) {
    Write-Host "ERROR: CUDA_PATH environment variable not found!" -ForegroundColor Red
    Write-Host "Please install CUDA Toolkit from: https://developer.nvidia.com/cuda-downloads" -ForegroundColor Yellow
    exit 1
}
Write-Host "CUDA found at: $cudaPath" -ForegroundColor Green

# Create build directory
if (Test-Path "build") {
    Write-Host "Cleaning existing build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force build
}

New-Item -ItemType Directory -Path "build" | Out-Null
Set-Location build

Write-Host ""
Write-Host "Running CMake configuration..." -ForegroundColor Cyan

# Configure with CMake
# If using vcpkg, uncomment and set the path:
# $vcpkgPath = "C:/path/to/vcpkg"
# cmake .. -DCMAKE_TOOLCHAIN_FILE="$vcpkgPath/scripts/buildsystems/vcpkg.cmake"

cmake ..

if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "ERROR: CMake configuration failed!" -ForegroundColor Red
    Write-Host ""
    Write-Host "If OpenCV is not found, try:" -ForegroundColor Yellow
    Write-Host "  1. Install with vcpkg: vcpkg install opencv:x64-windows" -ForegroundColor Yellow
    Write-Host "  2. Or specify OpenCV path: cmake .. -DOpenCV_DIR=C:/path/to/opencv/build" -ForegroundColor Yellow
    Set-Location ..
    exit 1
}

Write-Host ""
Write-Host "Building project..." -ForegroundColor Cyan

# Build
cmake --build . --config Release

if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "ERROR: Build failed!" -ForegroundColor Red
    Set-Location ..
    exit 1
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "Build completed successfully!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""
Write-Host "Executable location: build\Release\gameoflife.exe" -ForegroundColor Cyan
Write-Host ""
Write-Host "To run:" -ForegroundColor Cyan
Write-Host "  cd build\Release" -ForegroundColor White
Write-Host "  .\gameoflife.exe              # Visualization mode" -ForegroundColor White
Write-Host "  .\gameoflife.exe --benchmark  # Benchmark mode" -ForegroundColor White
Write-Host "  .\gameoflife.exe --both       # Both modes" -ForegroundColor White
Write-Host ""

Set-Location ..
