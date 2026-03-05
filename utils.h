#ifndef UTILS_H
#define UTILS_H

#include <cuda_runtime.h>
#include <cstdint>
#include <iostream>
#include <chrono>

// Grid configuration
constexpr int GRID_WIDTH = 1024;
constexpr int GRID_HEIGHT = 1024;
constexpr int GRID_SIZE = GRID_WIDTH * GRID_HEIGHT;

// CUDA kernel configuration
constexpr int BLOCK_SIZE = 16;  // 16x16 threads per block
constexpr int SHARED_BLOCK_SIZE = BLOCK_SIZE + 2;  // Include halo cells for neighbors

// Visualization settings
constexpr int WINDOW_WIDTH = 1024;
constexpr int WINDOW_HEIGHT = 1024;
constexpr int MAX_GENERATIONS = 10000;
constexpr float INITIAL_ALIVE_PROBABILITY = 0.25f;  // 25% cells initially alive (cleaner visuals)

// Cell states
constexpr uint8_t DEAD = 0;
constexpr uint8_t ALIVE = 1;

// CUDA error checking macro
#define CUDA_CHECK(call) \
    do { \
        cudaError_t error = call; \
        if (error != cudaSuccess) { \
            std::cerr << "CUDA error at " << __FILE__ << ":" << __LINE__ << " - " \
                      << cudaGetErrorString(error) << std::endl; \
            exit(EXIT_FAILURE); \
        } \
    } while(0)

// Simple timer class for performance measurement
class Timer {
private:
    std::chrono::high_resolution_clock::time_point start_time;
    
public:
    Timer() : start_time(std::chrono::high_resolution_clock::now()) {}
    
    void reset() {
        start_time = std::chrono::high_resolution_clock::now();
    }
    
    // Returns elapsed time in milliseconds
    double elapsed() const {
        auto end_time = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end_time - start_time).count();
    }
};

// Initialize grid with random values
inline void initializeGrid(uint8_t* grid, int width, int height, float aliveProbability) {
    srand(static_cast<unsigned int>(time(nullptr)));
    for (int i = 0; i < width * height; ++i) {
        grid[i] = (static_cast<float>(rand()) / RAND_MAX < aliveProbability) ? ALIVE : DEAD;
    }
}

// CPU version of Game of Life for comparison
inline void updateGridCPU(const uint8_t* input, uint8_t* output, int width, int height) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int neighbors = 0;
            
            // Count alive neighbors (8-connected neighborhood)
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    
                    // Wrap around boundaries (toroidal grid)
                    int nx = (x + dx + width) % width;
                    int ny = (y + dy + height) % height;
                    
                    neighbors += input[ny * width + nx];
                }
            }
            
            int idx = y * width + x;
            uint8_t cell = input[idx];
            
            // Apply Conway's Game of Life rules:
            // 1. Any live cell with 2 or 3 live neighbors survives
            // 2. Any dead cell with exactly 3 live neighbors becomes alive
            // 3. All other cells die or stay dead
            if (cell == ALIVE) {
                output[idx] = (neighbors == 2 || neighbors == 3) ? ALIVE : DEAD;
            } else {
                output[idx] = (neighbors == 3) ? ALIVE : DEAD;
            }
        }
    }
}

// Print GPU information
inline void printGPUInfo() {
    int deviceCount;
    CUDA_CHECK(cudaGetDeviceCount(&deviceCount));
    
    if (deviceCount == 0) {
        std::cerr << "No CUDA capable devices found!" << std::endl;
        exit(EXIT_FAILURE);
    }
    
    cudaDeviceProp prop;
    CUDA_CHECK(cudaGetDeviceProperties(&prop, 0));
    
    std::cout << "========================================" << std::endl;
    std::cout << "GPU Information:" << std::endl;
    std::cout << "  Device: " << prop.name << std::endl;
    std::cout << "  Compute Capability: " << prop.major << "." << prop.minor << std::endl;
    std::cout << "  Global Memory: " << prop.totalGlobalMem / (1024 * 1024) << " MB" << std::endl;
    std::cout << "  Shared Memory per Block: " << prop.sharedMemPerBlock / 1024 << " KB" << std::endl;
    std::cout << "  Max Threads per Block: " << prop.maxThreadsPerBlock << std::endl;
    std::cout << "  Multiprocessors: " << prop.multiProcessorCount << std::endl;
    std::cout << "========================================" << std::endl;
}

#endif // UTILS_H
