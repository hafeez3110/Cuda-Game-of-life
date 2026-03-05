#ifndef UTILS_ADVANCED_H
#define UTILS_ADVANCED_H

#include <cuda_runtime.h>

// Conditional NVTX include
#if defined(ENABLE_NVTX) && ENABLE_NVTX
#include <nvToolsExt.h>
#define NVTX_AVAILABLE 1
#else
#define NVTX_AVAILABLE 0
#endif

#include <chrono>
#include <string>
#include <cstdio>

// Grid configuration (override via CMake)
#ifdef GRID_WIDTH_OVERRIDE
#define GRID_WIDTH GRID_WIDTH_OVERRIDE
#else
#ifndef GRID_WIDTH
#define GRID_WIDTH 256
#endif
#endif

#ifdef GRID_HEIGHT_OVERRIDE
#define GRID_HEIGHT GRID_HEIGHT_OVERRIDE
#else
#ifndef GRID_HEIGHT
#define GRID_HEIGHT 256
#endif
#endif

#define GRID_SIZE (GRID_WIDTH * GRID_HEIGHT)

// Block configuration (override via CMake)
#ifdef BLOCK_SIZE_OVERRIDE
#define BLOCK_SIZE BLOCK_SIZE_OVERRIDE
#else
#ifndef BLOCK_SIZE
#define BLOCK_SIZE 16
#endif
#endif

#define SHARED_BLOCK_SIZE (BLOCK_SIZE + 2)

// Initial pattern configuration
constexpr float INITIAL_ALIVE_PROBABILITY = 0.3f;

// Feature flags (set via CMake or runtime)
#ifndef USE_CUDA_GRAPHS
#define USE_CUDA_GRAPHS 0
#endif

#ifndef USE_TEXTURE_MEMORY
#define USE_TEXTURE_MEMORY 0
#endif

#ifndef USE_BIT_PACKING
#define USE_BIT_PACKING 0
#endif

#ifndef USE_PINNED_MEMORY
#define USE_PINNED_MEMORY 1
#endif

// Cellular Automata Rule Structure
struct CARule {
    unsigned int birthMask;   // Bit mask for birth (e.g., bit 3 set for B3)
    unsigned int survivalMask; // Bit mask for survival (e.g., bits 2,3 set for S23)
    std::string name;
    
    CARule() : birthMask(0), survivalMask(0), name("Custom") {}
    
    // Standard Conway's Life: B3/S23
    static CARule ConwaysLife() {
        CARule rule;
        rule.birthMask = (1 << 3);           // Birth on 3 neighbors
        rule.survivalMask = (1 << 2) | (1 << 3); // Survive on 2 or 3 neighbors
        rule.name = "Conway's Life (B3/S23)";
        return rule;
    }
    
    // HighLife: B36/S23
    static CARule HighLife() {
        CARule rule;
        rule.birthMask = (1 << 3) | (1 << 6);
        rule.survivalMask = (1 << 2) | (1 << 3);
        rule.name = "HighLife (B36/S23)";
        return rule;
    }
    
    // Seeds: B2/S
    static CARule Seeds() {
        CARule rule;
        rule.birthMask = (1 << 2);
        rule.survivalMask = 0;
        rule.name = "Seeds (B2/S)";
        return rule;
    }
    
    // Day & Night: B3678/S34678
    static CARule DayAndNight() {
        CARule rule;
        rule.birthMask = (1 << 3) | (1 << 6) | (1 << 7) | (1 << 8);
        rule.survivalMask = (1 << 3) | (1 << 4) | (1 << 6) | (1 << 7) | (1 << 8);
        rule.name = "Day & Night (B3678/S34678)";
        return rule;
    }
    
    // Parse from string (e.g., "B3/S23")
    static CARule FromString(const std::string& ruleStr) {
        CARule rule;
        rule.name = ruleStr;
        
        size_t slashPos = ruleStr.find('/');
        if (slashPos == std::string::npos) return rule;
        
        // Parse birth
        size_t bPos = ruleStr.find('B');
        if (bPos != std::string::npos && bPos < slashPos) {
            for (size_t i = bPos + 1; i < slashPos; ++i) {
                if (isdigit(ruleStr[i])) {
                    int n = ruleStr[i] - '0';
                    rule.birthMask |= (1 << n);
                }
            }
        }
        
        // Parse survival
        size_t sPos = ruleStr.find('S');
        if (sPos != std::string::npos && sPos > slashPos) {
            for (size_t i = sPos + 1; i < ruleStr.length(); ++i) {
                if (isdigit(ruleStr[i])) {
                    int n = ruleStr[i] - '0';
                    rule.survivalMask |= (1 << n);
                }
            }
        }
        
        return rule;
    }
};

// CUDA error checking macro
#define CUDA_CHECK(call) \
    do { \
        cudaError_t err = call; \
        if (err != cudaSuccess) { \
            fprintf(stderr, "CUDA Error in %s:%d: %s\n", __FILE__, __LINE__, \
                    cudaGetErrorString(err)); \
            exit(EXIT_FAILURE); \
        } \
    } while(0)

// NVTX profiling helpers
#if NVTX_AVAILABLE
class NVTXRange {
public:
    NVTXRange(const char* name) {
        nvtxRangePushA(name);
    }
    ~NVTXRange() {
        nvtxRangePop();
    }
};

#define NVTX_RANGE(name) NVTXRange nvtx_range(name)
#else
// No-op implementations when NVTX is not available
class NVTXRange {
public:
    NVTXRange(const char*) {}
    ~NVTXRange() {}
};

#define NVTX_RANGE(name) do {} while(0)
#endif

// Performance metrics structure
struct PerformanceMetrics {
    double kernelTime;
    double transferTime;
    double displayTime;
    double totalTime;
    int generation;
    size_t aliveCells;
    
    PerformanceMetrics() : kernelTime(0), transferTime(0), displayTime(0), 
                          totalTime(0), generation(0), aliveCells(0) {}
};

// Timer class
class Timer {
private:
    std::chrono::high_resolution_clock::time_point start_time;
    
public:
    Timer() {
        reset();
    }
    
    void reset() {
        start_time = std::chrono::high_resolution_clock::now();
    }
    
    double elapsed() const {
        auto end_time = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end_time - start_time).count();
    }
};

// GPU auto-tuner result
struct AutoTuneResult {
    int blockSize;
    bool useSharedMemory;
    bool useTexture;
    double bestTime;
    std::string config;
};

// Print GPU information
inline void printGPUInfo() {
    int deviceCount;
    CUDA_CHECK(cudaGetDeviceCount(&deviceCount));
    
    if (deviceCount == 0) {
        printf("No CUDA-capable devices found!\n");
        exit(1);
    }
    
    printf("\n========================================\n");
    printf("GPU Information:\n");
    
    for (int i = 0; i < deviceCount; i++) {
        cudaDeviceProp prop;
        CUDA_CHECK(cudaGetDeviceProperties(&prop, i));
        
        printf("  Device %d: %s\n", i, prop.name);
        printf("  Compute Capability: %d.%d\n", prop.major, prop.minor);
        printf("  Global Memory: %zu MB\n", prop.totalGlobalMem / (1024 * 1024));
        printf("  Shared Memory per Block: %zu KB\n", prop.sharedMemPerBlock / 1024);
        printf("  Max Threads per Block: %d\n", prop.maxThreadsPerBlock);
        printf("  Multiprocessors: %d\n", prop.multiProcessorCount);
        printf("  Warp Size: %d\n", prop.warpSize);
        printf("  Max Grid Size: %d x %d x %d\n", 
               prop.maxGridSize[0], prop.maxGridSize[1], prop.maxGridSize[2]);
        printf("  Memory Bus Width: %d-bit\n", prop.memoryBusWidth);
        printf("  L2 Cache Size: %d KB\n", prop.l2CacheSize / 1024);
    }
    
    printf("========================================\n\n");
}

// CPU reference implementation with rule support
inline void updateGridCPU(const unsigned char* input, unsigned char* output, 
                         int width, int height, const CARule& rule) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int neighbors = 0;
            
            // Count neighbors (toroidal topology)
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0) continue;
                    
                    int nx = (x + dx + width) % width;
                    int ny = (y + dy + height) % height;
                    neighbors += input[ny * width + nx];
                }
            }
            
            int idx = y * width + x;
            unsigned char current = input[idx];
            
            // Apply rule
            if (current) {
                // Cell is alive - check survival
                output[idx] = (rule.survivalMask & (1 << neighbors)) ? 1 : 0;
            } else {
                // Cell is dead - check birth
                output[idx] = (rule.birthMask & (1 << neighbors)) ? 1 : 0;
            }
        }
    }
}

// Count alive cells
inline size_t countAliveCells(const unsigned char* grid, int size) {
    size_t count = 0;
    for (int i = 0; i < size; i++) {
        count += grid[i];
    }
    return count;
}

#endif // UTILS_ADVANCED_H
