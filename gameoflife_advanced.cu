#include "utils_advanced.h"
#include "gameoflife_advanced.cuh"
#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <texture_types.h>

// Texture reference for input grid (optional optimization)
#if USE_TEXTURE_MEMORY
texture<unsigned char, cudaTextureType2D, cudaReadModeElementType> texGrid;
#endif

// Device constant for CA rule
__constant__ unsigned int d_birthMask;
__constant__ unsigned int d_survivalMask;

// ==================================================================
// CUDA Kernels
// ==================================================================

// Optimized kernel with shared memory and configurable rules
__global__ void gameOfLifeKernelOptimized(const unsigned char* input, unsigned char* output,
                                         int width, int height) {
    __shared__ unsigned char shared[SHARED_BLOCK_SIZE][SHARED_BLOCK_SIZE];
    
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    
    int tx = threadIdx.x + 1;
    int ty = threadIdx.y + 1;
    
    // Load center cells
    if (x < width && y < height) {
        shared[ty][tx] = input[y * width + x];
    }
    
    // Load halo cells (toroidal wrap-around)
    if (threadIdx.x == 0) {
        int leftX = (x - 1 + width) % width;
        if (y < height) shared[ty][0] = input[y * width + leftX];
    }
    if (threadIdx.x == blockDim.x - 1 || x == width - 1) {
        int rightX = (x + 1) % width;
        if (y < height) shared[ty][tx + 1] = input[y * width + rightX];
    }
    if (threadIdx.y == 0) {
        int topY = (y - 1 + height) % height;
        if (x < width) shared[0][tx] = input[topY * width + x];
    }
    if (threadIdx.y == blockDim.y - 1 || y == height - 1) {
        int bottomY = (y + 1) % height;
        if (x < width) shared[ty + 1][tx] = input[bottomY * width + x];
    }
    
    // Load corners
    if (threadIdx.x == 0 && threadIdx.y == 0) {
        int leftX = (x - 1 + width) % width;
        int topY = (y - 1 + height) % height;
        shared[0][0] = input[topY * width + leftX];
    }
    if ((threadIdx.x == blockDim.x - 1 || x == width - 1) && threadIdx.y == 0) {
        int rightX = (x + 1) % width;
        int topY = (y - 1 + height) % height;
        shared[0][tx + 1] = input[topY * width + rightX];
    }
    if (threadIdx.x == 0 && (threadIdx.y == blockDim.y - 1 || y == height - 1)) {
        int leftX = (x - 1 + width) % width;
        int bottomY = (y + 1) % height;
        shared[ty + 1][0] = input[bottomY * width + leftX];
    }
    if ((threadIdx.x == blockDim.x - 1 || x == width - 1) && 
        (threadIdx.y == blockDim.y - 1 || y == height - 1)) {
        int rightX = (x + 1) % width;
        int bottomY = (y + 1) % height;
        shared[ty + 1][tx + 1] = input[bottomY * width + rightX];
    }
    
    __syncthreads();
    
    if (x >= width || y >= height) return;
    
    // Count neighbors from shared memory
    int neighbors = shared[ty - 1][tx - 1] + shared[ty - 1][tx] + shared[ty - 1][tx + 1] +
                    shared[ty][tx - 1] + shared[ty][tx + 1] +
                    shared[ty + 1][tx - 1] + shared[ty + 1][tx] + shared[ty + 1][tx + 1];
    
    unsigned char current = shared[ty][tx];
    int idx = y * width + x;
    
    // Apply CA rule using bit masks
    if (current) {
        output[idx] = (d_survivalMask & (1 << neighbors)) ? 1 : 0;
    } else {
        output[idx] = (d_birthMask & (1 << neighbors)) ? 1 : 0;
    }
}

// Texture memory variant
#if USE_TEXTURE_MEMORY
__global__ void gameOfLifeKernelTexture(unsigned char* output, int width, int height) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    
    if (x >= width || y >= height) return;
    
    int neighbors = 0;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            
            // Texture automatically wraps (set in host code)
            float fx = (float)((x + dx + width) % width);
            float fy = (float)((y + dy + height) % height);
            neighbors += tex2D(texGrid, fx, fy);
        }
    }
    
    unsigned char current = tex2D(texGrid, (float)x, (float)y);
    int idx = y * width + x;
    
    if (current) {
        output[idx] = (d_survivalMask & (1 << neighbors)) ? 1 : 0;
    } else {
        output[idx] = (d_birthMask & (1 << neighbors)) ? 1 : 0;
    }
}
#endif

// Bit-packed kernel (32x memory compression)
#if USE_BIT_PACKING
__global__ void gameOfLifeKernelBitPacked(const unsigned int* input, unsigned int* output,
                                          int width, int height) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    
    if (x >= width || y >= height) return;
    
    int wordsPerRow = (width + 31) / 32;
    int wordX = x / 32;
    int bitX = x % 32;
    
    unsigned int result = 0;
    
    // Process 32 cells in parallel using bitwise ops
    for (int bit = 0; bit < 32 && (x + bit) < width; bit++) {
        int cx = x + bit;
        int neighbors = 0;
        
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0) continue;
                
                int nx = (cx + dx + width) % width;
                int ny = (y + dy + height) % height;
                int nWordX = nx / 32;
                int nBitX = nx % 32;
                
                unsigned int word = input[ny * wordsPerRow + nWordX];
                neighbors += (word >> nBitX) & 1;
            }
        }
        
        unsigned int current = (input[y * wordsPerRow + wordX] >> bit) & 1;
        unsigned int newState;
        
        if (current) {
            newState = (d_survivalMask & (1 << neighbors)) ? 1 : 0;
        } else {
            newState = (d_birthMask & (1 << neighbors)) ? 1 : 0;
        }
        
        result |= (newState << bit);
    }
    
    output[y * wordsPerRow + wordX] = result;
}

// Unpack bit-packed grid for display
__global__ void unpackGridKernel(const unsigned int* packed, unsigned char* unpacked,
                                 int width, int height) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    
    if (x >= width || y >= height) return;
    
    int wordsPerRow = (width + 31) / 32;
    int wordX = x / 32;
    int bitX = x % 32;
    
    unsigned int word = packed[y * wordsPerRow + wordX];
    unpacked[y * width + x] = ((word >> bitX) & 1) ? 255 : 0;
}
#endif

// Naive kernel (for comparison)
__global__ void gameOfLifeKernelNaive(const unsigned char* input, unsigned char* output,
                                     int width, int height) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    
    if (x >= width || y >= height) return;
    
    int neighbors = 0;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            
            int nx = (x + dx + width) % width;
            int ny = (y + dy + height) % height;
            neighbors += input[ny * width + nx];
        }
    }
    
    unsigned char current = input[y * width + x];
    int idx = y * width + x;
    
    if (current) {
        output[idx] = (d_survivalMask & (1 << neighbors)) ? 1 : 0;
    } else {
        output[idx] = (d_birthMask & (1 << neighbors)) ? 1 : 0;
    }
}

// ==================================================================
// Host Functions
// ==================================================================

// CUDAContext method implementations
CUDAContext::CUDAContext() : currentGrid(0), graphCreated(false), graph(nullptr), graphExec(nullptr) {
    gridBytes = GRID_SIZE * sizeof(unsigned char);
    
    // Allocate device memory
    CUDA_CHECK(cudaMalloc(&d_grid[0], gridBytes));
    CUDA_CHECK(cudaMalloc(&d_grid[1], gridBytes));
    CUDA_CHECK(cudaMalloc(&d_display, gridBytes));
    
    // Allocate pinned host memory for async transfers
    #if USE_PINNED_MEMORY
    CUDA_CHECK(cudaHostAlloc(&h_pinned[0], gridBytes, cudaHostAllocPortable));
    CUDA_CHECK(cudaHostAlloc(&h_pinned[1], gridBytes, cudaHostAllocPortable));
    #else
    h_pinned[0] = new unsigned char[GRID_SIZE];
    h_pinned[1] = new unsigned char[GRID_SIZE];
    #endif
    
    // Create streams
    CUDA_CHECK(cudaStreamCreate(&computeStream));
    CUDA_CHECK(cudaStreamCreate(&transferStream));
    
    // Create events
    CUDA_CHECK(cudaEventCreate(&computeReady[0]));
    CUDA_CHECK(cudaEventCreate(&computeReady[1]));
    
    // Set up grid/block dimensions
    blockDim = dim3(BLOCK_SIZE, BLOCK_SIZE);
    gridDim = dim3((GRID_WIDTH + BLOCK_SIZE - 1) / BLOCK_SIZE,
                  (GRID_HEIGHT + BLOCK_SIZE - 1) / BLOCK_SIZE);
    
    // Set default rule (Conway's Life)
    rule = CARule::ConwaysLife();
    CUDA_CHECK(cudaMemcpyToSymbol(d_birthMask, &rule.birthMask, sizeof(unsigned int)));
    CUDA_CHECK(cudaMemcpyToSymbol(d_survivalMask, &rule.survivalMask, sizeof(unsigned int)));
}

CUDAContext::~CUDAContext() {
    // Free device memory
    cudaFree(d_grid[0]);
    cudaFree(d_grid[1]);
    cudaFree(d_display);
    
    // Free host memory
    #if USE_PINNED_MEMORY
    cudaFreeHost(h_pinned[0]);
    cudaFreeHost(h_pinned[1]);
    #else
    delete[] h_pinned[0];
    delete[] h_pinned[1];
    #endif
    
    // Destroy streams
    cudaStreamDestroy(computeStream);
    cudaStreamDestroy(transferStream);
    
    // Destroy events
    cudaEventDestroy(computeReady[0]);
    cudaEventDestroy(computeReady[1]);
    
    // Destroy graph
    if (graphCreated) {
        cudaGraphExecDestroy(graphExec);
        cudaGraphDestroy(graph);
    }
}

void CUDAContext::setRule(const CARule& newRule) {
    rule = newRule;
    CUDA_CHECK(cudaMemcpyToSymbol(d_birthMask, &rule.birthMask, sizeof(unsigned int)));
    CUDA_CHECK(cudaMemcpyToSymbol(d_survivalMask, &rule.survivalMask, sizeof(unsigned int)));
    
    // Invalidate graph if it exists
    if (graphCreated) {
        cudaGraphExecDestroy(graphExec);
        cudaGraphDestroy(graph);
        graphCreated = false;
    }
}

void CUDAContext::initializeGrid(const unsigned char* hostData) {
    CUDA_CHECK(cudaMemcpy(d_grid[0], hostData, gridBytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_grid[1], hostData, gridBytes, cudaMemcpyHostToDevice));
}

void CUDAContext::createGraph() {
    #if USE_CUDA_GRAPHS
    if (graphCreated) return;
    
    NVTX_RANGE("CreateGraph");
    
    cudaStreamBeginCapture(computeStream, cudaStreamCaptureModeGlobal);
    
    // Kernel launch
    gameOfLifeKernelOptimized<<<gridDim, blockDim, 0, computeStream>>>(
        d_grid[currentGrid], d_grid[1 - currentGrid], GRID_WIDTH, GRID_HEIGHT);
    
    cudaStreamEndCapture(computeStream, &graph);
    CUDA_CHECK(cudaGraphInstantiate(&graphExec, graph, nullptr, nullptr, 0));
    
    graphCreated = true;
    #endif
}

void CUDAContext::stepGraph() {
    #if USE_CUDA_GRAPHS
    NVTX_RANGE("GraphLaunch");
    CUDA_CHECK(cudaGraphLaunch(graphExec, computeStream));
    currentGrid = 1 - currentGrid;
    #endif
}

void CUDAContext::step() {
    NVTX_RANGE("Kernel");
    
    gameOfLifeKernelOptimized<<<gridDim, blockDim, 0, computeStream>>>(
        d_grid[currentGrid], d_grid[1 - currentGrid], GRID_WIDTH, GRID_HEIGHT);
    
    currentGrid = 1 - currentGrid;
}

void CUDAContext::transferToHostAsync(int bufferIdx) {
    NVTX_RANGE("TransferD2H");
    
    CUDA_CHECK(cudaEventRecord(computeReady[bufferIdx], computeStream));
    CUDA_CHECK(cudaStreamWaitEvent(transferStream, computeReady[bufferIdx], 0));
    CUDA_CHECK(cudaMemcpyAsync(h_pinned[bufferIdx], d_grid[currentGrid], gridBytes,
                               cudaMemcpyDeviceToHost, transferStream));
}

void CUDAContext::syncTransfer() {
    CUDA_CHECK(cudaStreamSynchronize(transferStream));
}

unsigned char* CUDAContext::getCurrentGrid() {
    return d_grid[currentGrid];
}

unsigned char* CUDAContext::getPinnedBuffer(int idx) {
    return h_pinned[idx];
}

// Export C-style interface for compatibility
extern "C" {
    CUDAContext* createCUDAContext() {
        return new CUDAContext();
    }
    
    void destroyCUDAContext(CUDAContext* ctx) {
        delete ctx;
    }
    
    void setCARule(CUDAContext* ctx, const char* ruleString) {
        CARule rule = CARule::FromString(ruleString);
        ctx->setRule(rule);
    }
}
