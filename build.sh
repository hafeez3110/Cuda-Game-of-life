#!/bin/bash
# Conway's Game of Life - Build Script for Linux
# Run this script with: ./build.sh

echo "========================================"
echo "Conway's Game of Life - CUDA Build Script"
echo "========================================"
echo ""

# Check if nvcc is available
if ! command -v nvcc &> /dev/null; then
    echo "ERROR: nvcc (CUDA compiler) not found!"
    echo "Please install CUDA Toolkit from: https://developer.nvidia.com/cuda-downloads"
    exit 1
fi

echo "CUDA compiler found: $(nvcc --version | grep release)"

# Check if OpenCV is installed
if ! pkg-config --exists opencv4; then
    echo "WARNING: OpenCV 4 not found via pkg-config"
    echo "Attempting to continue anyway (CMake might find it differently)..."
else
    echo "OpenCV found: $(pkg-config --modversion opencv4)"
fi

echo ""

# Create build directory
if [ -d "build" ]; then
    echo "Cleaning existing build directory..."
    rm -rf build
fi

mkdir build
cd build

echo ""
echo "Running CMake configuration..."

# Configure with CMake
cmake ..

if [ $? -ne 0 ]; then
    echo ""
    echo "ERROR: CMake configuration failed!"
    echo ""
    echo "If OpenCV is not found, try:"
    echo "  Ubuntu/Debian: sudo apt-get install libopencv-dev"
    echo "  Or specify path: cmake .. -DOpenCV_DIR=/path/to/opencv"
    cd ..
    exit 1
fi

echo ""
echo "Building project..."

# Build with all available cores
make -j$(nproc)

if [ $? -ne 0 ]; then
    echo ""
    echo "ERROR: Build failed!"
    cd ..
    exit 1
fi

echo ""
echo "========================================"
echo "Build completed successfully!"
echo "========================================"
echo ""
echo "Executable location: build/gameoflife"
echo ""
echo "To run:"
echo "  cd build"
echo "  ./gameoflife              # Visualization mode"
echo "  ./gameoflife --benchmark  # Benchmark mode"
echo "  ./gameoflife --both       # Both modes"
echo ""

cd ..
