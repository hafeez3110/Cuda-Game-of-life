# Conway's Game of Life - Advanced Build Script
# Builds the version with ALL advanced features

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Conway's Game of Life - Advanced Build" -ForegroundColor Cyan
Write-Host "(All Features Enabled)" -ForegroundColor Cyan
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

if (Test-Path "C:\vcpkg\installed\x64-windows\include\opencv2") {
    $opencvPath = "C:\vcpkg\installed\x64-windows"
    $opencvFound = $true
    Write-Host "OpenCV found via vcpkg at: $opencvPath" -ForegroundColor Green
}
elseif (Test-Path "C:\opencv\build") {
    $opencvPath = "C:\opencv\build"
    $opencvFound = $true
    Write-Host "OpenCV found at: $opencvPath" -ForegroundColor Green
}

if (-not $opencvFound) {
    Write-Host ""
    Write-Host "ERROR: OpenCV not found!" -ForegroundColor Red
    Write-Host "Please install OpenCV first." -ForegroundColor Yellow
    exit 1
}

# Build configuration variables (can be modified)
$Preset = "AUTO"
$UseCudaGraphs = $true
$UseTexture = $false
$UseBitPacking = $false
$EnableNVTX = $true
$BuildTests = $false
$GridWidth = 1024
$GridHeight = 1024
$BlockSize = 16

Write-Host ""
Write-Host "Build Configuration:" -ForegroundColor Cyan
Write-Host "  Preset: $Preset" -ForegroundColor White
Write-Host "  CUDA Graphs: $UseCudaGraphs" -ForegroundColor White
Write-Host "  Texture Memory: $UseTexture" -ForegroundColor White
Write-Host "  Bit Packing: $UseBitPacking" -ForegroundColor White
Write-Host "  NVTX Profiling: $EnableNVTX" -ForegroundColor White
Write-Host "  Build Tests: $BuildTests" -ForegroundColor White
Write-Host "  Grid Size: ${GridWidth}x${GridHeight}" -ForegroundColor White
Write-Host "  Block Size: ${BlockSize}x${BlockSize}" -ForegroundColor White
Write-Host ""

# Create build directory
if (Test-Path "build_advanced") {
    Write-Host "Cleaning existing build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force build_advanced
}

New-Item -ItemType Directory -Path "build_advanced" | Out-Null
Set-Location build_advanced

Write-Host ""
Write-Host "Running CMake configuration..." -ForegroundColor Cyan

# Copy source files
Copy-Item ..\gameoflife_advanced.cu .
Copy-Item ..\gameoflife_advanced.cuh .
Copy-Item ..\main_advanced.cpp .
Copy-Item ..\utils_advanced.h .
Copy-Item ..\CMakeLists_advanced.txt .\CMakeLists.txt

# Build CMake arguments
$nvccPathEscaped = $nvccPath -replace '\\', '/'
$cmakeArgs = @(
    ".",
    "-G", "Ninja",
    "-DCMAKE_CUDA_COMPILER=$nvccPathEscaped",
    "-DCMAKE_BUILD_TYPE=Release",
    "-DPRESET=$Preset",
    "-DGRID_WIDTH=$GridWidth",
    "-DGRID_HEIGHT=$GridHeight",
    "-DBLOCK_SIZE=$BlockSize"
)

if ($UseCudaGraphs) { $cmakeArgs += "-DUSE_CUDA_GRAPHS=ON" } else { $cmakeArgs += "-DUSE_CUDA_GRAPHS=OFF" }
if ($UseTexture) { $cmakeArgs += "-DUSE_TEXTURE_MEMORY=ON" } else { $cmakeArgs += "-DUSE_TEXTURE_MEMORY=OFF" }
if ($UseBitPacking) { $cmakeArgs += "-DUSE_BIT_PACKING=ON" } else { $cmakeArgs += "-DUSE_BIT_PACKING=OFF" }
if ($EnableNVTX) { $cmakeArgs += "-DENABLE_NVTX=ON" } else { $cmakeArgs += "-DENABLE_NVTX=OFF" }
if ($BuildTests) { $cmakeArgs += "-DBUILD_TESTS=ON" } else { $cmakeArgs += "-DBUILD_TESTS=OFF" }

# Add OpenCV path
if (Test-Path "C:\vcpkg\scripts\buildsystems\vcpkg.cmake") {
    Write-Host "Using vcpkg toolchain..." -ForegroundColor Yellow
    $cmakeArgs += "-DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
} else {
    Write-Host "Using manual OpenCV path..." -ForegroundColor Yellow
    $opencvPathEscaped = $opencvPath -replace '\\', '/'
    $cmakeArgs += "-DOpenCV_DIR=$opencvPathEscaped"
}

# Run CMake
& cmake @cmakeArgs

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

# Copy OpenCV DLL
if (Test-Path "C:\opencv\build\x64\vc16\bin\opencv_world4110.dll") {
    Copy-Item "C:\opencv\build\x64\vc16\bin\opencv_world4110.dll" .
    Write-Host "OpenCV DLL copied to build directory" -ForegroundColor Green
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "Build completed successfully!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""
Write-Host "Executable: build_advanced\gameoflife_advanced.exe" -ForegroundColor Cyan
Write-Host ""
Write-Host "To run:" -ForegroundColor Cyan
Write-Host "  cd build_advanced" -ForegroundColor White
Write-Host "  .\gameoflife_advanced.exe                    # Default settings" -ForegroundColor White
Write-Host "  .\gameoflife_advanced.exe -g 5000            # Run for 5000 generations" -ForegroundColor White
Write-Host "  .\gameoflife_advanced.exe -log metrics.csv   # Log performance to CSV" -ForegroundColor White
Write-Host "  .\gameoflife_advanced.exe -video output.mp4  # Record video" -ForegroundColor White
Write-Host ""
Write-Host "Advanced Controls:" -ForegroundColor Cyan
Write-Host "  SPACE - Pause/Resume" -ForegroundColor White
Write-Host "  'r' - Reset grid" -ForegroundColor White
Write-Host "  'q' or ESC - Quit" -ForegroundColor White
Write-Host "  'g' - Load Glider pattern" -ForegroundColor White
Write-Host "  'b' - Load Blinker pattern" -ForegroundColor White
Write-Host "  'G' - Load Glider Gun pattern" -ForegroundColor White
Write-Host "  'p' - Save screenshot" -ForegroundColor White
Write-Host "  Mouse wheel - Zoom" -ForegroundColor White
Write-Host "  Ctrl+Drag - Pan" -ForegroundColor White
Write-Host ""
Write-Host "Trackbars:" -ForegroundColor Cyan
Write-Host "  Speed - Adjust simulation speed" -ForegroundColor White
Write-Host "  Color Mode - Change visualization (0=B/W, 1=Age, 2=Birth/Death, 3=Heatmap)" -ForegroundColor White
Write-Host "  Rule - Select CA rule (0=Conway, 1=HighLife, 2=Seeds, 3=Day&Night)" -ForegroundColor White
Write-Host ""

Set-Location ..

# Display feature summary
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Advanced Features Included:" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  ✓ Zero-copy pinned memory transfers" -ForegroundColor Green
Write-Host "  ✓ Dual-stream async compute/transfer" -ForegroundColor Green
Write-Host "  ✓ CUDA Graphs for reduced overhead" -ForegroundColor Green
Write-Host "  ✓ Configurable CA rules (B/S notation)" -ForegroundColor Green
Write-Host "  ✓ Pattern library (Glider, Gun, etc.)" -ForegroundColor Green
Write-Host "  ✓ Multiple color modes" -ForegroundColor Green
Write-Host "  ✓ Zoom & pan with mouse" -ForegroundColor Green
Write-Host "  ✓ Interactive cell editing" -ForegroundColor Green
Write-Host "  ✓ Performance sparklines" -ForegroundColor Green
Write-Host "  ✓ CSV logging" -ForegroundColor Green
Write-Host "  ✓ MP4 video recording" -ForegroundColor Green
Write-Host "  ✓ NVTX profiling markers" -ForegroundColor Green
Write-Host "  ✓ Runtime rule switching" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
