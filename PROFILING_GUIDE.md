# CUDA Profiling Guide for Conway's Game of Life

This guide explains how to profile the advanced version using NVIDIA Nsight tools to identify bottlenecks and optimize performance.

## 🎯 Prerequisites

- NVIDIA Nsight Systems (ships with CUDA Toolkit)
- NVIDIA Nsight Compute (optional, for kernel analysis)
- Advanced version built with NVTX enabled (default)

## 📊 Tools Overview

### Nsight Systems
- **Purpose**: Timeline analysis, CPU/GPU activity, stream management
- **Best for**: Understanding overall application flow, finding idle time
- **Output**: Visual timeline with CUDA API calls, kernels, memcpy

### Nsight Compute
- **Purpose**: Detailed kernel analysis, occupancy, memory throughput
- **Best for**: Optimizing individual kernels
- **Output**: Performance metrics, roofline analysis, source correlation

---

## 🚀 Quick Start

### 1. Build with Profiling Support

```powershell
# NVTX enabled by default
.\build_advanced.ps1 -EnableNVTX:$true

cd build_advanced
```

### 2. Capture Profile with Nsight Systems

```powershell
# Basic capture (1000 generations)
nsys profile -o gameoflife.nsys-rep .\gameoflife_advanced.exe -g 1000

# With more details
nsys profile --trace=cuda,nvtx,opengl -o detailed.nsys-rep .\gameoflife_advanced.exe -g 1000
```

### 3. Open in Nsight Systems GUI

```powershell
nsys-ui gameoflife.nsys-rep
```

---

## 📈 Reading the Timeline

### Expected Timeline (Optimized)

```
CPU Thread ────────────────────────────────────────────────────>
  ▓ API ▓ API ▓ API ▓ API ▓ API ...

CUDA HW (Compute) ─────────────────────────────────────────────>
  ▓▓▓▓▓ Kernel ▓▓▓▓▓ Kernel ▓▓▓▓▓ Kernel ▓▓▓▓▓ Kernel ...
  
CUDA HW (Copy) ────────────────────────────────────────────────>
     ▓ D2H ▓ D2H ▓ D2H ▓ D2H ▓ D2H ...

NVTX Ranges:
  [─SimulationFrame─][─SimulationFrame─][─SimulationFrame─]
    [Kernel]   [Transfer] [Kernel]   [Transfer]
```

### Good Signs ✅
- **Overlapping** compute and copy streams
- **Minimal gaps** between kernels
- **Short API calls** (graph mode)
- **Consistent kernel duration** (no variance)

### Bad Signs ❌
- **Gaps** between operations (idle GPU)
- **Long API calls** (CPU overhead)
- **Memcpy blocking kernel** (no async)
- **Increasing kernel time** (thermal throttling)

---

## 🔍 Key Metrics to Check

### 1. GPU Utilization

**Where**: Top of timeline, "GPU Utilization" row

**Target**: 60-80% (accounting for display time)

**If low (<30%)**:
- CPU-bound (check CPU timeline for busy work)
- Too many small kernels (use CUDA Graphs)
- Synchronization issues (check for `cudaDeviceSynchronize`)

**Fix**:
```cpp
// Remove explicit syncs
// cudaDeviceSynchronize(); ← Remove these!

// Use streams and events instead
cudaEventRecord(event, stream);
cudaStreamWaitEvent(otherStream, event, 0);
```

---

### 2. Kernel Duration

**Where**: CUDA HW row, hover over kernel blocks

**Target**: 0.2-0.3 ms for 1024x1024 grid

**If slower (>1 ms)**:
- Wrong block size
- Not using shared memory
- Divergent branches

**Check**:
```
Right-click kernel → "Show in Events View"
Look at: Grid size, Block size, Shared Memory, Registers
```

**Optimize**:
- Ensure BLOCK_SIZE matches GPU (16 for laptops, 32 for desktop)
- Check shared memory usage (should be ~4 KB per block)

---

### 3. Memory Transfer Time

**Where**: CUDA HW (Copy) row

**Target**: <0.05 ms for 1024x1024 grid

**If slower (>0.1 ms)**:
- Not using pinned memory
- Blocking transfers
- PCIe bandwidth limited

**Fix**:
```powershell
# Rebuild with pinned memory (should be default)
.\build_advanced.ps1 -UsePinnedMemory:$true
```

---

### 4. Stream Overlap

**Where**: Compare CUDA HW (Compute) and CUDA HW (Copy) rows

**Target**: Transfers happen during kernel execution

**Example - Good overlap**:
```
Compute: [──Kernel──]  [──Kernel──]
Copy:           [D2H]         [D2H]
         └─ Overlap! ─┘
```

**Example - Bad (no overlap)**:
```
Compute: [Kernel] [Kernel] [Kernel]
Copy:           [D2H][D2H][D2H]
         └─ Gap! No overlap ─┘
```

**Fix**: Ensure dual-stream implementation is enabled (it is in advanced version).

---

### 5. NVTX Ranges

**Where**: NVTX row in timeline

**What to look for**:
- `"Kernel"` should match GPU kernel duration
- `"TransferD2H"` should match memcpy duration
- `"SimulationFrame"` shows total frame time

**Use case**: Correlate CPU activity with GPU work.

---

## 🧪 Profiling Experiments

### Experiment 1: CUDA Graphs vs Traditional Launch

**Purpose**: Measure launch overhead reduction

**Steps**:
```powershell
# Build without graphs
.\build_advanced.ps1 -UseCudaGraphs:$false
cd build_advanced
nsys profile -o without_graphs.nsys-rep .\gameoflife_advanced.exe -g 1000

# Build with graphs
cd ..
.\build_advanced.ps1 -UseCudaGraphs:$true
cd build_advanced
nsys profile -o with_graphs.nsys-rep .\gameoflife_advanced.exe -g 1000

# Compare in GUI
nsys-ui without_graphs.nsys-rep
nsys-ui with_graphs.nsys-rep
```

**What to look for**:
- API call duration (should be shorter with graphs)
- CPU activity (less with graphs)
- FPS improvement (10-15%)

---

### Experiment 2: Pinned vs Pageable Memory

**Purpose**: Measure transfer speedup

**Steps**:
```powershell
# Pinned (default)
.\build_advanced.ps1 -UsePinnedMemory:$true
cd build_advanced
nsys profile -o pinned.nsys-rep .\gameoflife_advanced.exe -g 1000

# Pageable
cd ..
.\build_advanced.ps1 -UsePinnedMemory:$false
cd build_advanced
nsys profile -o pageable.nsys-rep .\gameoflife_advanced.exe -g 1000
```

**What to look for**:
- Memcpy duration in CUDA HW (Copy) row
- Expect 2-3x faster with pinned memory

---

### Experiment 3: Block Size Optimization

**Purpose**: Find optimal block size for your GPU

**Steps**:
```powershell
foreach ($bs in @(8, 16, 32)) {
    $BlockSize = $bs
    .\build_advanced.ps1
    cd build_advanced
    nsys profile -o block_${bs}.nsys-rep .\gameoflife_advanced.exe -g 1000
    cd ..
}

# Compare all three
nsys-ui build_advanced/block_*.nsys-rep
```

**What to look for**:
- Kernel duration (lowest is best)
- Occupancy (higher is usually better)

---

## 🎓 Nsight Compute Deep Dive

### Capturing Kernel Profile

```powershell
# Profile specific kernel
ncu --set full -o kernel_profile .\gameoflife_advanced.exe -g 100

# Open in GUI
ncu-ui kernel_profile.ncu-rep
```

### Key Sections to Check

#### 1. **GPU Speed Of Light (SOL)**
- Shows % of peak memory/compute utilization
- **Target**: >50% for memory or compute

**If Memory Bound (<50% compute, >70% memory)**:
- Optimize memory access patterns
- Use shared memory more
- Consider bit-packing

**If Compute Bound (>70% compute, <50% memory)**:
- Simplify arithmetic
- Use lookup tables
- Optimize branch divergence

---

#### 2. **Occupancy**
- Threads per SM / Max threads per SM
- **Target**: >50%

**If low occupancy**:
- Reduce shared memory usage
- Reduce register usage
- Increase block size

**Check**:
```
"Launch Statistics" → "Theoretical Occupancy"
```

---

#### 3. **Memory Workload Analysis**
- L1/L2 cache hit rates
- Global memory throughput

**Target hit rates**:
- L1: >80% (with shared memory)
- L2: >60%

**If low hit rates**:
- Check access patterns (should be coalesced)
- Increase shared memory usage
- Verify block/thread mapping

---

#### 4. **Warp State Statistics**
- Shows warp stalls (memory, sync, etc.)

**Common issues**:
- **High "Memory Throttle"**: Memory bound, optimize access
- **High "Synchronization"**: Too many `__syncthreads()`
- **High "Math Pipe Throttle"**: Compute bound, optimize math

---

## 📝 Profiling Checklist

Before optimizing, verify:

- [ ] Kernel duration is consistent (not varying ±20%)
- [ ] Memcpy overlaps with compute (dual-stream)
- [ ] No explicit `cudaDeviceSynchronize()` in hot path
- [ ] GPU utilization >50%
- [ ] Kernel occupancy >40%
- [ ] L1 cache hit rate >70%
- [ ] No large gaps in timeline
- [ ] NVTX ranges make sense

If all checked, you're optimized! 🎉

---

## 🛠️ Common Issues & Fixes

### Issue: GPU Idle Time

**Symptom**: Gaps between kernels in timeline

**Causes**:
1. CPU overhead (parsing, logic)
2. Synchronous API calls
3. Display blocking

**Fix**:
```cpp
// Move work to GPU or use async
cv::waitKey(1);  // ← Non-blocking, good!
// vs
cv::waitKey(0);  // ← Blocking, bad!
```

---

### Issue: Memory Thrashing

**Symptom**: Increasing kernel time over generations

**Cause**: Memory allocations in loop

**Fix**:
```cpp
// BAD: Allocate every frame
for (int i = 0; i < N; i++) {
    cudaMalloc(&temp, size);  // ← Slow!
    // ...
    cudaFree(temp);
}

// GOOD: Allocate once
cudaMalloc(&temp, size);
for (int i = 0; i < N; i++) {
    // Use temp
}
cudaFree(temp);
```

---

### Issue: Kernel Launch Overhead

**Symptom**: High CPU activity, short kernels

**Fix**: Use CUDA Graphs!
```powershell
.\build_advanced.ps1 -UseCudaGraphs:$true
```

---

## 📚 Advanced Topics

### Roofline Analysis

**What it shows**: Theoretical peak vs actual performance

**How to read**:
```
If dot is below roofline → Memory bound (optimize bandwidth)
If dot is near roofline → Compute bound (optimize math)
```

**Where**: Nsight Compute → "GPU Speed Of Light" → "Roofline Chart"

---

### Source Correlation

**What it does**: Maps performance metrics to source code lines

**Steps**:
1. Compile with `-lineinfo` flag (already done)
2. Capture profile with Nsight Compute
3. Open "Source" tab in GUI
4. See hotspots highlighted

**Use case**: Find exact line causing slowdown!

---

### Multi-Stream Profiling

**Verify stream overlap**:
```
1. Open timeline
2. Expand "CUDA HW" row
3. Check stream IDs (should see Stream 1, Stream 2)
4. Verify operations overlap
```

---

## 🎯 Optimization Workflow

```
1. Profile with Nsight Systems
   ↓
2. Identify bottleneck (GPU idle? Memory bound?)
   ↓
3. If GPU idle → Reduce CPU work, use graphs
   If memory bound → Optimize access patterns
   If compute bound → Optimize kernel math
   ↓
4. Make change, rebuild
   ↓
5. Profile again
   ↓
6. Compare before/after
   ↓
7. Repeat until satisfied
```

---

## 📊 Benchmark Comparison Template

```powershell
# Create comparison report
$configs = @("baseline", "pinned", "graphs", "all_opts")

foreach ($cfg in $configs) {
    # Build with config
    # ...
    
    # Profile
    nsys profile -o ${cfg}.nsys-rep .\gameoflife_advanced.exe -g 1000
    
    # Extract metrics
    nsys stats ${cfg}.nsys-rep > ${cfg}_stats.txt
}

# Compare in spreadsheet
```

**Metrics to extract**:
- Average kernel time
- Average memcpy time
- Total GPU time
- FPS

---

## 🏆 Target Metrics (1024x1024 grid)

| Metric | RTX 4060 | RTX 4090 |
|--------|----------|----------|
| Kernel time | 0.25 ms | 0.10 ms |
| Memcpy time | 0.02 ms | 0.01 ms |
| GPU utilization | 60% | 70% |
| L1 hit rate | 85% | 90% |
| Occupancy | 60% | 75% |
| FPS (real-time) | 230 | 400 |
| FPS (console) | 17,000 | 40,000 |

---

## 📖 Further Reading

- [Nsight Systems User Guide](https://docs.nvidia.com/nsight-systems/)
- [Nsight Compute User Guide](https://docs.nvidia.com/nsight-compute/)
- [CUDA Best Practices Guide](https://docs.nvidia.com/cuda/cuda-c-best-practices-guide/)
- [NVTX Documentation](https://docs.nvidia.com/nsight-visual-studio-edition/nvtx/index.html)

---

**Happy profiling! May your kernels be fast and your memory coalesced! 🚀**
