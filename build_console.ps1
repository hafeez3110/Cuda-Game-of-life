# Conway's Game of Life - Console Build Script for Windows
# This builds the console version WITHOUT OpenCV dependency

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Conway's Game of Life - Console Build" -ForegroundColor Cyan
Write-Host "(No OpenCV required)" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Set environment variables
$env:PATH += ";C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.0\bin"
$env:CUDA_PATH = "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.0"

# Check if CUDA is installed
$cudaPath = $env:CUDA_PATH
if (-not $cudaPath) {
    Write-Host "ERROR: CUDA_PATH environment variable not found!" -ForegroundColor Red
    exit 1
}

$nvccPath = Join-Path $cudaPath "bin\nvcc.exe"
if (-not (Test-Path $nvccPath)) {
    Write-Host "ERROR: nvcc.exe not found at $nvccPath" -ForegroundColor Red
    exit 1
}

Write-Host "CUDA found at: $cudaPath" -ForegroundColor Green

# Find Visual Studio 2022 Build Tools
$vsPath = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
if (-not (Test-Path $vsPath)) {
    Write-Host "ERROR: Visual Studio 2022 Build Tools not found!" -ForegroundColor Red
    Write-Host "Searched in: $vsPath" -ForegroundColor Yellow
    exit 1
}

Write-Host "Visual Studio found at: $vsPath" -ForegroundColor Green

# Initialize Visual Studio environment
$vcvarsPath = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
if (Test-Path $vcvarsPath) {
    Write-Host "Initializing VS environment..." -ForegroundColor Yellow
    cmd /c "call `"$vcvarsPath`" && set" | ForEach-Object {
        if ($_ -match "^(.*?)=(.*)$") {
            Set-Content "env:\$($matches[1])" $matches[2]
        }
    }
}

# Create build directory
if (Test-Path "build_console") {
    Write-Host "Cleaning existing build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force build_console
}

New-Item -ItemType Directory -Path "build_console" | Out-Null
Set-Location build_console

Write-Host ""
Write-Host "Running CMake configuration..." -ForegroundColor Cyan

# Copy source files to build directory
Copy-Item ..\gameoflife.cu .
Copy-Item ..\main_console.cpp .
Copy-Item ..\utils.h .
Copy-Item ..\CMakeLists_console.txt .\CMakeLists.txt

# Configure with CMake using Ninja generator (works better with CUDA than VS generator)
cmake . -G Ninja -DCMAKE_CUDA_COMPILER="$nvccPath" -DCMAKE_BUILD_TYPE=Release

if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "ERROR: CMake configuration failed!" -ForegroundColor Red
    Set-Location ..
    exit 1
}

Write-Host ""
Write-Host "Building project..." -ForegroundColor Cyan

# Build with Ninja
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
Write-Host "Executable location: build_console\gameoflife_console.exe" -ForegroundColor Cyan
Write-Host ""
Write-Host "To run:" -ForegroundColor Cyan
Write-Host "  cd build_console" -ForegroundColor White
Write-Host "  .\gameoflife_console.exe                # Run benchmarks" -ForegroundColor White
Write-Host "  .\gameoflife_console.exe --benchmark    # Run benchmarks" -ForegroundColor White
Write-Host "  .\gameoflife_console.exe --simulate 1000  # Simulate 1000 generations" -ForegroundColor White
Write-Host ""

Set-Location ..
