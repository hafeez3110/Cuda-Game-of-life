# Quick Start Guide

Get Conway's Game of Life running on your RTX 4060 in 5 minutes!

## ⚡ Fast Track (Windows)

### Prerequisites Check
```powershell
# 1. Check if CUDA is installed
nvcc --version

# 2. Check if CMake is installed
cmake --version

# 3. Check if you have a C++ compiler (Visual Studio)
cl.exe
```

If any command fails, see [Installation](#installation) below.

### Build & Run
```powershell
# Navigate to project directory
cd Game_of_Life

# Run build script
.\build.ps1

# Run the program
cd build\Release
.\gameoflife.exe
```

That's it! You should see the simulation running.

## 🐧 Fast Track (Linux)

```bash
# Navigate to project directory
cd Game_of_Life

# Make build script executable
chmod +x build.sh

# Build
./build.sh

# Run
cd build
./gameoflife
```

## 📦 Installation

### 1. CUDA Toolkit (Required)

**Windows:**
1. Download from [NVIDIA CUDA Downloads](https://developer.nvidia.com/cuda-downloads)
2. Run installer (select Express Installation)
3. Restart your terminal
4. Verify: `nvcc --version`

**Linux (Ubuntu/Debian):**
```bash
# Add NVIDIA package repository
wget https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2204/x86_64/cuda-keyring_1.1-1_all.deb
sudo dpkg -i cuda-keyring_1.1-1_all.deb
sudo apt-get update

# Install CUDA
sudo apt-get install cuda-toolkit-12-6

# Add to PATH (add to ~/.bashrc for permanent)
export PATH=/usr/local/cuda-12.6/bin:$PATH
export LD_LIBRARY_PATH=/usr/local/cuda-12.6/lib64:$LD_LIBRARY_PATH

# Verify
nvcc --version
```

### 2. CMake (Required)

**Windows:**
- Download from [CMake Downloads](https://cmake.org/download/)
- Run installer, select "Add CMake to system PATH"
- Verify: `cmake --version`

**Linux:**
```bash
sudo apt-get install cmake
cmake --version
```

### 3. Visual Studio (Windows Required)

**Windows:**
1. Download [Visual Studio Community 2022](https://visualstudio.microsoft.com/downloads/) (Free)
2. During installation, select:
   - ✅ Desktop development with C++
   - ✅ C++ CMake tools for Windows
3. Restart after installation

**Linux:**
```bash
# Install GCC
sudo apt-get install build-essential
g++ --version
```

### 4. OpenCV (Required)

**Windows - Option A (vcpkg - Recommended):**
```powershell
# Install vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat

# Install OpenCV
.\vcpkg install opencv:x64-windows

# Remember the vcpkg path for later!
```

**Windows - Option B (Manual):**
1. Download OpenCV from [opencv.org/releases](https://opencv.org/releases/)
2. Extract to `C:\opencv`
3. When building, specify path:
   ```powershell
   cmake .. -DOpenCV_DIR="C:\opencv\build"
   ```

**Linux:**
```bash
sudo apt-get install libopencv-dev
```

## 🎮 Running the Program

### Visualization Mode (Default)
```bash
./gameoflife
```

**Controls:**
- `Q` or `ESC` - Quit
- `R` - Reset with new random pattern
- `SPACE` - Pause/Resume

### Benchmark Mode
```bash
./gameoflife --benchmark
```
Compares CPU vs GPU performance.

### Both Modes
```bash
./gameoflife --both
```
Runs benchmarks first, then visualization.

## 🔧 Troubleshooting

### "nvcc: command not found"
**Fix:** CUDA not installed or not in PATH
```powershell
# Windows: Add to PATH
$env:PATH += ";C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.6\bin"

# Or install CUDA Toolkit
```

### "CMake could not find OpenCV"
**Fix:** Specify OpenCV path
```powershell
# If using vcpkg
cmake .. -DCMAKE_TOOLCHAIN_FILE="C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake"

# If manually installed
cmake .. -DOpenCV_DIR="C:/opencv/build"
```

### "No CUDA-capable device found"
**Fix:** 
- Ensure you have an NVIDIA GPU
- Update GPU drivers: [NVIDIA Driver Downloads](https://www.nvidia.com/download/index.aspx)
- Check device: `nvidia-smi`

### Build fails with "error MSB8036: The Windows SDK version was not found"
**Fix:** 
- Open Visual Studio Installer
- Modify installation → Add Windows 10 SDK

### "opencv_world4XX.dll not found" when running
**Fix (Windows):**
```powershell
# Copy OpenCV DLLs to build directory
# Or add OpenCV bin to PATH
$env:PATH += ";C:\opencv\build\x64\vc16\bin"
```

## 📊 What to Expect

### First Run Output
```
========================================
GPU Information:
  Device: NVIDIA GeForce RTX 4060 Laptop GPU
  Compute Capability: 8.9
  Global Memory: 8192 MB
  ...
========================================

Grid Configuration:
  Grid Size: 1024 x 1024 (1048576 cells)
  Block Size: 16 x 16 (256 threads/block)
  ...

Starting simulation...
Gen:   100 | FPS:  850.2 | Avg Time: 1.176 ms
Gen:   200 | FPS:  892.5 | Avg Time: 1.120 ms
...
```

### Benchmark Results (Typical)
```
CPU Performance:
  Avg Time/Gen: 154.325 ms
  FPS: 6.5

GPU Performance (Naive):
  Avg Time/Gen: 4.123 ms
  FPS: 242.6

GPU Performance (Optimized):
  Avg Time/Gen: 1.426 ms
  FPS: 701.5
```

**GPU should be ~100-150x faster than CPU!** 🚀

## 🎯 Next Steps

1. ✅ Get it running
2. 📖 Read [OPTIMIZATION_GUIDE.md](OPTIMIZATION_GUIDE.md) to understand CUDA techniques
3. 🔧 Modify grid size in `utils.h` and experiment
4. 🧪 Try different initial patterns
5. 📈 Profile with NVIDIA Nsight Compute: `ncu ./gameoflife --benchmark`

## 💡 Tips

- **Low FPS?** Check GPU usage with `nvidia-smi` - should be ~100%
- **Want bigger grid?** Change `GRID_WIDTH` and `GRID_HEIGHT` in `utils.h`
- **Different patterns?** Adjust `INITIAL_ALIVE_PROBABILITY` (0.0 to 1.0)
- **No GPU?** CPU version still works (just slower)

## 🆘 Still Having Issues?

1. Check that you have:
   - NVIDIA GPU with Compute Capability 8.6+ (RTX 30xx/40xx)
   - Updated GPU drivers
   - CUDA Toolkit 12.x
   - Visual Studio 2019+ (Windows) or GCC 9+ (Linux)
   - OpenCV 4.x

2. Try building with verbose output:
   ```powershell
   cmake --build . --config Release --verbose
   ```

3. Check the full [README.md](README.md) for detailed information

## 📚 Learn More

- **CUDA Programming:** [CUDA C++ Programming Guide](https://docs.nvidia.com/cuda/cuda-c-programming-guide/)
- **Optimization Details:** See [OPTIMIZATION_GUIDE.md](OPTIMIZATION_GUIDE.md)
- **Conway's Game of Life:** [Wikipedia](https://en.wikipedia.org/wiki/Conway%27s_Game_of_Life)

---
**Happy coding! 🎮**
