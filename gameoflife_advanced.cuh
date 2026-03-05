#ifndef GAMEOFLIFE_ADVANCED_CUH
#define GAMEOFLIFE_ADVANCED_CUH

#include "utils_advanced.h"
#include <cuda_runtime.h>

// Forward declarations of CUDA kernels
__global__ void gameOfLifeKernelNaive(const unsigned char* input, unsigned char* output, 
                                     int width, int height);
                                     
__global__ void gameOfLifeKernelOptimized(const unsigned char* input, unsigned char* output,
                                         int width, int height);

#if USE_TEXTURE_MEMORY
__global__ void gameOfLifeKernelTexture(unsigned char* output, int width, int height);
#endif

#if USE_BIT_PACKING
__global__ void gameOfLifeKernelBitPacked(const unsigned int* input, unsigned int* output,
                                         int width, int height);
__global__ void unpackGridKernel(const unsigned int* packed, unsigned char* unpacked,
                                int width, int height);
#endif

// CUDA Context class declaration
class CUDAContext {
public:
    // Device memory
    unsigned char* d_grid[2];
    unsigned char* d_display;
    int currentGrid;
    
    // Pinned host memory for async transfers
#if USE_PINNED_MEMORY
    unsigned char* h_pinned[2];
#endif
    
    // Dual streams for overlapped execution
    cudaStream_t computeStream;
    cudaStream_t transferStream;
    cudaEvent_t computeReady[2];
    
    // CUDA Graphs
#if USE_CUDA_GRAPHS
    cudaGraph_t graph;
    cudaGraphExec_t graphExec;
    bool graphCreated;
#endif

    // Grid configuration
    dim3 blockDim;
    dim3 gridDim;
    size_t gridBytes;
    
    // Current CA rule
    CARule rule;
    
    // Constructor
    CUDAContext();
    
    // Destructor
    ~CUDAContext();
    
    // Initialize grid with pattern
    void initializeGrid(const unsigned char* pattern);
    
    // Get pinned buffer for reading results
    unsigned char* getPinnedBuffer(int index);
    
    // Set CA rule
    void setRule(const CARule& newRule);
    
    // Execute one generation
    void step();
    
#if USE_CUDA_GRAPHS
    // Create CUDA graph
    void createGraph();
    
    // Execute using CUDA graph
    void stepGraph();
#endif
    
    // Transfer result to host asynchronously
    void transferToHostAsync(int bufferIndex);
    
    // Synchronize transfer stream
    void syncTransfer();
    
    // Get current grid pointer (device)
    unsigned char* getCurrentGrid();
};

#endif // GAMEOFLIFE_ADVANCED_CUH
