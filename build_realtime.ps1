# Conway's Game of Life - Real-time Visualization Build Script
# Builds the version with OpenCV real-time display

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Conway's Game of Life - Real-time Build" -ForegroundColor Cyan
Write-Host "(OpenCV Real-time Visualization)" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Set environment variables
$env:PATH += ";C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.0\bin"
$env:CUDA_PATH = "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.0"

# Check CUDA
$cudaPath = $env:CUDA_PATH
if (-not $cudaPath) {
    Write-Host "ERROR: CUDA not found!" -ForegroundColor Red
    exit 1
}

$nvccPath = Join-Path $cudaPath "bin\nvcc.exe"
if (-not (Test-Path $nvccPath)) {
    Write-Host "ERROR: nvcc.exe not found!" -ForegroundColor Red
    exit 1
}

Write-Host "CUDA found at: $cudaPath" -ForegroundColor Green

# Check Visual Studio
$vsPath = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
if (-not (Test-Path $vsPath)) {
    Write-Host "ERROR: Visual Studio 2022 Build Tools not found!" -ForegroundColor Red
    exit 1
}

Write-Host "Visual Studio found at: $vsPath" -ForegroundColor Green

# Initialize VS environment
$vcvarsPath = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
if (Test-Path $vcvarsPath) {
    Write-Host "Initializing VS environment..." -ForegroundColor Yellow
    cmd /c "call `"$vcvarsPath`" && set" | ForEach-Object {
        if ($_ -match "^(.*?)=(.*)$") {
            Set-Content "env:\$($matches[1])" $matches[2]
        }
    }
}

# Check for OpenCV
Write-Host ""
Write-Host "Checking for OpenCV..." -ForegroundColor Cyan

$opencvFound = $false
$opencvPath = ""

# Check vcpkg installation
if (Test-Path "C:\vcpkg\installed\x64-windows\include\opencv2") {
    $opencvPath = "C:\vcpkg\installed\x64-windows"
    $opencvFound = $true
    Write-Host "OpenCV found via vcpkg at: $opencvPath" -ForegroundColor Green
}
# Check user profile
elseif (Test-Path "$env:USERPROFILE\opencv\build") {
    $opencvPath = "$env:USERPROFILE\opencv\build"
    $opencvFound = $true
    Write-Host "OpenCV found at: $opencvPath" -ForegroundColor Green
}
# Check C:\opencv
elseif (Test-Path "C:\opencv\build") {
    $opencvPath = "C:\opencv\build"
    $opencvFound = $true
    Write-Host "OpenCV found at: $opencvPath" -ForegroundColor Green
}

if (-not $opencvFound) {
    Write-Host ""
    Write-Host "ERROR: OpenCV not found!" -ForegroundColor Red
    Write-Host ""
    Write-Host "OpenCV is required for real-time visualization." -ForegroundColor Yellow
    Write-Host "Please install OpenCV using one of these methods:" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Method 1: Using vcpkg (recommended)" -ForegroundColor Cyan
    Write-Host "  C:\vcpkg\vcpkg.exe install opencv:x64-windows" -ForegroundColor White
    Write-Host ""
    Write-Host "Method 2: Manual download" -ForegroundColor Cyan
    Write-Host "  Download from: https://opencv.org/releases/" -ForegroundColor White
    Write-Host "  Extract to C:\opencv" -ForegroundColor White
    Write-Host ""
    Write-Host "Alternatively, use the console version:" -ForegroundColor Yellow
    Write-Host "  .\build_console.ps1" -ForegroundColor White
    Write-Host ""
    exit 1
}

# Create build directory
if (Test-Path "build_realtime") {
    Write-Host "Cleaning existing build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force build_realtime
}

New-Item -ItemType Directory -Path "build_realtime" | Out-Null
Set-Location build_realtime

Write-Host ""
Write-Host "Running CMake configuration..." -ForegroundColor Cyan

# Copy source files
Copy-Item ..\gameoflife.cu .
Copy-Item ..\main_realtime.cpp .
Copy-Item ..\utils.h .
Copy-Item ..\CMakeLists_realtime.txt .\CMakeLists.txt

# Configure with CMake
if (Test-Path "C:\vcpkg\scripts\buildsystems\vcpkg.cmake") {
    Write-Host "Using vcpkg toolchain..." -ForegroundColor Yellow
    cmake . -G Ninja -DCMAKE_CUDA_COMPILER="$nvccPath" -DCMAKE_BUILD_TYPE=Release `
            -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
} else {
    Write-Host "Using manual OpenCV path..." -ForegroundColor Yellow
    cmake . -G Ninja -DCMAKE_CUDA_COMPILER="$nvccPath" -DCMAKE_BUILD_TYPE=Release `
            -DOpenCV_DIR="$opencvPath"
}

if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "ERROR: CMake configuration failed!" -ForegroundColor Red
    Set-Location ..
    exit 1
}

Write-Host ""
Write-Host "Building project..." -ForegroundColor Cyan

# Build
ninja

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
Write-Host "Executable: build_realtime\gameoflife_realtime.exe" -ForegroundColor Cyan
Write-Host ""
Write-Host "To run:" -ForegroundColor Cyan
Write-Host "  cd build_realtime" -ForegroundColor White
Write-Host "  .\gameoflife_realtime.exe           # Run with default settings" -ForegroundColor White
Write-Host "  .\gameoflife_realtime.exe -g 5000   # Run for 5000 generations" -ForegroundColor White
Write-Host ""
Write-Host "Controls:" -ForegroundColor Cyan
Write-Host "  'q' or ESC - Quit" -ForegroundColor White
Write-Host "  'r' - Reset grid" -ForegroundColor White
Write-Host "  SPACE - Pause/Resume" -ForegroundColor White
Write-Host ""

Set-Location ..
