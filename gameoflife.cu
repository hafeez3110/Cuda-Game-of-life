#include "utils.h"
#include <cuda_runtime.h>
#include <device_launch_parameters.h>

/**
 * CUDA Kernel: Game of Life Update (Optimized with Shared Memory)
 * 
 * Grid/Block Configuration:
 * - Each block handles a BLOCK_SIZE x BLOCK_SIZE tile of cells (16x16 = 256 threads)
 * - Grid dimensions are calculated to cover entire GRID_WIDTH x GRID_HEIGHT
 * - Total blocks: (GRID_WIDTH/BLOCK_SIZE) x (GRID_HEIGHT/BLOCK_SIZE)
 * 
 * Shared Memory Strategy:
 * - Allocate shared memory of size (BLOCK_SIZE+2) x (BLOCK_SIZE+2)
 * - Extra cells (halo/ghost cells) store neighbors from adjacent blocks
 * - This reduces global memory reads from 9 per cell to ~1-2 per cell
 * 
 * Memory Coalescing:
 * - Threads in a warp read consecutive memory addresses
 * - Input/output arrays accessed in row-major order
 * - Each thread computes one cell, ensuring aligned memory access
 * 
 * Thread Synchronization:
 * - __syncthreads() after loading shared memory to ensure all data is ready
 * - No synchronization needed after computation (each thread writes to unique location)
 */
__global__ void gameOfLifeKernel(const uint8_t* __restrict__ input, 
                                  uint8_t* __restrict__ output, 
                                  int width, 
                                  int height) {
    // Allocate shared memory for the block plus halo cells
    __shared__ uint8_t sharedGrid[SHARED_BLOCK_SIZE][SHARED_BLOCK_SIZE];
    
    // Global thread coordinates
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    
    // Local thread coordinates within the block
    int tx = threadIdx.x;
    int ty = threadIdx.y;
    
    // Shared memory coordinates (offset by 1 for halo)
    int sx = tx + 1;
    int sy = ty + 1;
    
    // ======== LOAD DATA INTO SHARED MEMORY ========
    
    // Load center cell (coalesced memory access)
    if (x < width && y < height) {
        sharedGrid[sy][sx] = input[y * width + x];
    } else {
        sharedGrid[sy][sx] = DEAD;
    }
    
    // Load halo cells (boundary cells needed for neighbor calculation)
    // This is done by threads at the edges of the block
    
    // Top halo
    if (ty == 0) {
        int ny = (y - 1 + height) % height;  // Wrap around (toroidal grid)
        if (x < width) {
            sharedGrid[0][sx] = input[ny * width + x];
        } else {
            sharedGrid[0][sx] = DEAD;
        }
    }
    
    // Bottom halo
    if (ty == blockDim.y - 1) {
        int ny = (y + 1) % height;
        if (x < width) {
            sharedGrid[sy + 1][sx] = input[ny * width + x];
        } else {
            sharedGrid[sy + 1][sx] = DEAD;
        }
    }
    
    // Left halo
    if (tx == 0) {
        int nx = (x - 1 + width) % width;
        if (y < height) {
            sharedGrid[sy][0] = input[y * width + nx];
        } else {
            sharedGrid[sy][0] = DEAD;
        }
    }
    
    // Right halo
    if (tx == blockDim.x - 1) {
        int nx = (x + 1) % width;
        if (y < height) {
            sharedGrid[sy][sx + 1] = input[y * width + nx];
        } else {
            sharedGrid[sy][sx + 1] = DEAD;
        }
    }
    
    // Corner halos (only loaded by corner threads)
    if (tx == 0 && ty == 0) {  // Top-left corner
        int nx = (x - 1 + width) % width;
        int ny = (y - 1 + height) % height;
        sharedGrid[0][0] = (nx < width && ny < height) ? input[ny * width + nx] : DEAD;
    }
    
    if (tx == blockDim.x - 1 && ty == 0) {  // Top-right corner
        int nx = (x + 1) % width;
        int ny = (y - 1 + height) % height;
        sharedGrid[0][sx + 1] = (nx < width && ny < height) ? input[ny * width + nx] : DEAD;
    }
    
    if (tx == 0 && ty == blockDim.y - 1) {  // Bottom-left corner
        int nx = (x - 1 + width) % width;
        int ny = (y + 1) % height;
        sharedGrid[sy + 1][0] = (nx < width && ny < height) ? input[ny * width + nx] : DEAD;
    }
    
    if (tx == blockDim.x - 1 && ty == blockDim.y - 1) {  // Bottom-right corner
        int nx = (x + 1) % width;
        int ny = (y + 1) % height;
        sharedGrid[sy + 1][sx + 1] = (nx < width && ny < height) ? input[ny * width + nx] : DEAD;
    }
    
    // ======== SYNCHRONIZE THREADS ========
    // Wait for all threads to finish loading shared memory
    // This ensures all halo cells are available before neighbor counting
    __syncthreads();
    
    // ======== COMPUTE NEXT GENERATION ========
    
    // Only compute for valid cells within grid bounds
    if (x < width && y < height) {
        // Count alive neighbors using shared memory (fast!)
        int neighbors = 0;
        
        // Unrolled loop for better performance
        neighbors += sharedGrid[sy - 1][sx - 1];  // Top-left
        neighbors += sharedGrid[sy - 1][sx    ];  // Top
        neighbors += sharedGrid[sy - 1][sx + 1];  // Top-right
        neighbors += sharedGrid[sy    ][sx - 1];  // Left
        neighbors += sharedGrid[sy    ][sx + 1];  // Right
        neighbors += sharedGrid[sy + 1][sx - 1];  // Bottom-left
        neighbors += sharedGrid[sy + 1][sx    ];  // Bottom
        neighbors += sharedGrid[sy + 1][sx + 1];  // Bottom-right
        
        uint8_t cell = sharedGrid[sy][sx];
        
        // Apply Conway's Game of Life rules
        // Rule 1: Live cell with 2 or 3 neighbors survives
        // Rule 2: Dead cell with exactly 3 neighbors becomes alive
        // Rule 3: All other cells die or stay dead
        uint8_t nextState;
        if (cell == ALIVE) {
            nextState = (neighbors == 2 || neighbors == 3) ? ALIVE : DEAD;
        } else {
            nextState = (neighbors == 3) ? ALIVE : DEAD;
        }
        
        // Write result to global memory (coalesced write)
        // No race condition: each thread writes to unique location
        output[y * width + x] = nextState;
    }
}

/**
 * CUDA Kernel: Naive version without shared memory (for comparison)
 * This version demonstrates the performance difference when not using shared memory.
 * Each thread reads 9 values from global memory (very slow).
 */
__global__ void gameOfLifeKernelNaive(const uint8_t* __restrict__ input, 
                                       uint8_t* __restrict__ output, 
                                       int width, 
                                       int height) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    
    if (x >= width || y >= height) return;
    
    int neighbors = 0;
    
    // Count neighbors with 9 global memory reads per thread (inefficient)
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            
            int nx = (x + dx + width) % width;
            int ny = (y + dy + height) % height;
            
            neighbors += input[ny * width + nx];
        }
    }
    
    uint8_t cell = input[y * width + x];
    uint8_t nextState;
    
    if (cell == ALIVE) {
        nextState = (neighbors == 2 || neighbors == 3) ? ALIVE : DEAD;
    } else {
        nextState = (neighbors == 3) ? ALIVE : DEAD;
    }
    
    output[y * width + x] = nextState;
}

// ======== HOST FUNCTIONS ========

/**
 * Initialize CUDA memory and copy initial grid
 */
extern "C" void initializeCUDA(uint8_t** d_input, uint8_t** d_output, const uint8_t* h_grid, int size) {
    size_t bytes = size * sizeof(uint8_t);
    
    // Allocate device memory
    CUDA_CHECK(cudaMalloc(d_input, bytes));
    CUDA_CHECK(cudaMalloc(d_output, bytes));
    
    // Copy initial grid to device
    CUDA_CHECK(cudaMemcpy(*d_input, h_grid, bytes, cudaMemcpyHostToDevice));
}

/**
 * Run one generation update on GPU (optimized version)
 */
extern "C" void updateGridGPU(uint8_t* d_input, uint8_t* d_output, int width, int height) {
    // Configure grid and block dimensions
    dim3 blockDim(BLOCK_SIZE, BLOCK_SIZE);
    dim3 gridDim((width + BLOCK_SIZE - 1) / BLOCK_SIZE, 
                 (height + BLOCK_SIZE - 1) / BLOCK_SIZE);
    
    // Launch kernel
    gameOfLifeKernel<<<gridDim, blockDim>>>(d_input, d_output, width, height);
    
    // Check for kernel launch errors
    CUDA_CHECK(cudaGetLastError());
}

/**
 * Run one generation update on GPU (naive version for comparison)
 */
extern "C" void updateGridGPUNaive(uint8_t* d_input, uint8_t* d_output, int width, int height) {
    dim3 blockDim(BLOCK_SIZE, BLOCK_SIZE);
    dim3 gridDim((width + BLOCK_SIZE - 1) / BLOCK_SIZE, 
                 (height + BLOCK_SIZE - 1) / BLOCK_SIZE);
    
    gameOfLifeKernelNaive<<<gridDim, blockDim>>>(d_input, d_output, width, height);
    CUDA_CHECK(cudaGetLastError());
}

/**
 * Copy result back to host
 */
extern "C" void copyToHost(uint8_t* h_grid, const uint8_t* d_grid, int size) {
    CUDA_CHECK(cudaMemcpy(h_grid, d_grid, size * sizeof(uint8_t), cudaMemcpyDeviceToHost));
}

/**
 * Free CUDA memory
 */
extern "C" void cleanupCUDA(uint8_t* d_input, uint8_t* d_output) {
    CUDA_CHECK(cudaFree(d_input));
    CUDA_CHECK(cudaFree(d_output));
}

/**
 * Swap device pointers (for ping-pong buffering)
 */
extern "C" void swapPointers(uint8_t** a, uint8_t** b) {
    uint8_t* temp = *a;
    *a = *b;
    *b = temp;
}
