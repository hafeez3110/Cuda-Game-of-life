# Conway's Game of Life CUDA - Quick Reference

## 🚀 Quick Start (30 seconds)

### Option 1: Real-time Visualization (Recommended)
```powershell
cd c:\vscode\Game_of_Life
.\build_realtime.ps1
cd build_realtime
.\gameoflife_realtime.exe
```
Press `q` to quit when done!

### Option 2: Console (Maximum Speed)
```powershell
cd c:\vscode\Game_of_Life
.\build_console.ps1
cd build_console
.\gameoflife_console.exe -b
```

## 🎮 Keyboard Controls (Real-time)

| Key | Action |
|-----|--------|
| `q` or `ESC` | Quit |
| `r` | Reset with new pattern |
| `SPACE` | Pause/Resume |

## 📊 Performance at a Glance

| Version | FPS | What You See |
|---------|-----|--------------|
| **Real-time** | 225 | OpenCV window, smooth animation |
| **Console** | 17,067 | Terminal text, pure speed |

## 🛠️ Common Commands

### Build
```powershell
.\build_realtime.ps1     # Build with OpenCV
.\build_console.ps1      # Build without OpenCV
```

### Run Real-time
```powershell
cd build_realtime
.\gameoflife_realtime.exe          # Default: 10,000 gens
.\gameoflife_realtime.exe -g 5000  # 5,000 generations
```

### Run Console
```powershell
cd build_console
.\gameoflife_console.exe     # Interactive menu
.\gameoflife_console.exe -b  # All benchmarks
.\gameoflife_console.exe -s  # Simulation mode
```

## 🔧 Quick Customization

### Change Grid Size
Edit `utils.h`:
```cpp
constexpr int GRID_WIDTH = 2048;   // Default: 1024
constexpr int GRID_HEIGHT = 2048;  // Default: 1024
```

### Change Initial Density
Edit `utils.h`:
```cpp
constexpr float INITIAL_ALIVE_PROBABILITY = 0.4f;  // Default: 0.3 (30%)
```

## 🐛 Quick Troubleshooting

### OpenCV Not Found
```powershell
# Download and extract to C:\opencv
# Then rebuild:
.\build_realtime.ps1
```

### DLL Missing
```powershell
cd build_realtime
copy C:\opencv\build\x64\vc16\bin\opencv_world4110.dll .
```

### CUDA Not Found
```powershell
# Check CUDA installed at:
Test-Path "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.0"
```

## 📁 File Locations

```
Game_of_Life/
├── Source:
│   ├── gameoflife.cu          # CUDA kernels
│   ├── main_realtime.cpp      # Real-time version
│   └── main_console.cpp       # Console version
│
├── Build Scripts:
│   ├── build_realtime.ps1     # Build real-time
│   └── build_console.ps1      # Build console
│
├── Executables:
│   ├── build_realtime\gameoflife_realtime.exe
│   └── build_console\gameoflife_console.exe
│
└── Documentation:
    ├── README.md              # Full documentation
    ├── QUICKSTART.md          # Beginner guide
    ├── REALTIME_USAGE.md      # Real-time guide
    └── OPTIMIZATION_GUIDE.md  # CUDA details
```

## 🎯 Performance Targets (RTX 4060)

### Console Version
- **Target**: 15,000+ FPS
- **Achieved**: ✅ 17,067 FPS
- **GPU Time**: 0.059 ms/generation

### Real-time Version
- **Target**: 200+ FPS
- **Achieved**: ✅ 225 FPS
- **GPU Time**: 0.250 ms/generation
- **Display Time**: ~4 ms/frame

## 💡 Tips

1. **First time?** Use real-time version to see patterns evolve
2. **Want speed?** Use console version for pure performance
3. **Interesting patterns?** Press `r` multiple times until you see something cool
4. **Pause to observe?** Press `SPACE` in real-time version
5. **Benchmarking?** Run console with `-b` flag

## 📚 Quick Links

- **Main README**: `README.md`
- **Quick Start Guide**: `QUICKSTART.md`
- **Real-time Usage**: `REALTIME_USAGE.md`
- **CUDA Optimization**: `OPTIMIZATION_GUIDE.md`
- **Completion Reports**: `PROJECT_COMPLETE.md`, `REALTIME_COMPLETE.md`

## 🎮 Example Session

```powershell
# Build and run real-time version
PS> cd c:\vscode\Game_of_Life
PS> .\build_realtime.ps1
# [Build output...]
PS> cd build_realtime
PS> .\gameoflife_realtime.exe -g 2000

# OpenCV window opens showing Game of Life
# Watch patterns evolve at 225 FPS
# Press 'r' to reset if it gets boring
# Press 'q' to quit when done

# Final statistics shown:
#   Total Generations: 2000
#   Total Time: 8874.58 ms
#   Average GPU Compute Time: 0.250 ms/generation
#   Overall FPS: 225.4
```

## ✅ System Requirements

- ✅ Windows 10/11
- ✅ NVIDIA GPU (RTX 30xx/40xx series)
- ✅ CUDA Toolkit 13.0+
- ✅ Visual Studio 2022 Build Tools
- ✅ OpenCV 4.11.0 (real-time only)

## 🎉 Success Indicators

You know it's working when you see:

### Console Version
```
GPU Benchmark (1000 generations):
  Total Time: 59.37 ms
  Average Time per Generation: 0.059 ms
  Frames Per Second: 17067 FPS
  Speedup vs CPU: 97.15x
```

### Real-time Version
- OpenCV window opens immediately
- Smooth animation of black/white cells
- FPS counter shows 220-240
- Keyboard controls work
- Quit shows final statistics

---

**That's it! You're ready to explore Conway's Game of Life on your GPU! 🚀**
