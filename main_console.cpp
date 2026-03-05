#include "utils.h"
#include <iostream>
#include <vector>
#include <iomanip>
#include <fstream>
#include <string>

// CUDA function declarations
extern "C" {
    void initializeCUDA(uint8_t** d_input, uint8_t** d_output, const uint8_t* h_grid, int size);
    void updateGridGPU(uint8_t* d_input, uint8_t* d_output, int width, int height);
    void updateGridGPUNaive(uint8_t* d_input, uint8_t* d_output, int width, int height);
    void copyToHost(uint8_t* h_grid, const uint8_t* d_grid, int size);
    void cleanupCUDA(uint8_t* d_input, uint8_t* d_output);
    void swapPointers(uint8_t** a, uint8_t** b);
}

/**
 * Save grid to text file for visualization
 */
void saveGridToFile(const uint8_t* grid, int width, int height, const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return;
    }
    
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            file << (grid[y * width + x] ? "█" : " ");
        }
        file << "\n";
    }
    file.close();
    std::cout << "Grid saved to " << filename << std::endl;
}

/**
 * Print a small portion of the grid to console
 */
void printGridSection(const uint8_t* grid, int width, int height, int startX, int startY, int size) {
    std::cout << "\nGrid section (" << startX << "," << startY << ") to (" 
              << (startX + size) << "," << (startY + size) << "):\n";
    for (int y = startY; y < std::min(startY + size, height); ++y) {
        for (int x = startX; x < std::min(startX + size, width); ++x) {
            std::cout << (grid[y * width + x] ? "█" : "·");
        }
        std::cout << "\n";
    }
}

/**
 * Run CPU benchmark
 */
void runCPUBenchmark(int numGenerations = 100) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "Running CPU Benchmark..." << std::endl;
    std::cout << "========================================" << std::endl;
    
    std::vector<uint8_t> grid1(GRID_SIZE);
    std::vector<uint8_t> grid2(GRID_SIZE);
    
    initializeGrid(grid1.data(), GRID_WIDTH, GRID_HEIGHT, INITIAL_ALIVE_PROBABILITY);
    
    Timer timer;
    
    for (int gen = 0; gen < numGenerations; ++gen) {
        if (gen % 2 == 0) {
            updateGridCPU(grid1.data(), grid2.data(), GRID_WIDTH, GRID_HEIGHT);
        } else {
            updateGridCPU(grid2.data(), grid1.data(), GRID_WIDTH, GRID_HEIGHT);
        }
    }
    
    double elapsed = timer.elapsed();
    double avgTime = elapsed / numGenerations;
    double fps = 1000.0 / avgTime;
    
    std::cout << "CPU Performance:" << std::endl;
    std::cout << "  Generations: " << numGenerations << std::endl;
    std::cout << "  Total Time: " << std::fixed << std::setprecision(2) << elapsed << " ms" << std::endl;
    std::cout << "  Avg Time/Gen: " << std::fixed << std::setprecision(3) << avgTime << " ms" << std::endl;
    std::cout << "  FPS: " << std::fixed << std::setprecision(1) << fps << std::endl;
}

/**
 * Run GPU benchmark (optimized version)
 */
void runGPUBenchmark(int numGenerations = 100) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "Running GPU Benchmark (Optimized)..." << std::endl;
    std::cout << "========================================" << std::endl;
    
    std::vector<uint8_t> h_grid(GRID_SIZE);
    initializeGrid(h_grid.data(), GRID_WIDTH, GRID_HEIGHT, INITIAL_ALIVE_PROBABILITY);
    
    uint8_t* d_input;
    uint8_t* d_output;
    
    initializeCUDA(&d_input, &d_output, h_grid.data(), GRID_SIZE);
    
    // Warm-up run
    updateGridGPU(d_input, d_output, GRID_WIDTH, GRID_HEIGHT);
    CUDA_CHECK(cudaDeviceSynchronize());
    
    Timer timer;
    
    for (int gen = 0; gen < numGenerations; ++gen) {
        updateGridGPU(d_input, d_output, GRID_WIDTH, GRID_HEIGHT);
        swapPointers(&d_input, &d_output);
    }
    
    CUDA_CHECK(cudaDeviceSynchronize());
    double elapsed = timer.elapsed();
    
    double avgTime = elapsed / numGenerations;
    double fps = 1000.0 / avgTime;
    
    std::cout << "GPU Performance (Optimized):" << std::endl;
    std::cout << "  Generations: " << numGenerations << std::endl;
    std::cout << "  Total Time: " << std::fixed << std::setprecision(2) << elapsed << " ms" << std::endl;
    std::cout << "  Avg Time/Gen: " << std::fixed << std::setprecision(3) << avgTime << " ms" << std::endl;
    std::cout << "  FPS: " << std::fixed << std::setprecision(1) << fps << std::endl;
    
    // Copy final result back
    copyToHost(h_grid.data(), d_input, GRID_SIZE);
    
    // Print a small section to show pattern
    printGridSection(h_grid.data(), GRID_WIDTH, GRID_HEIGHT, 400, 400, 40);
    
    // Save final state
    saveGridToFile(h_grid.data(), GRID_WIDTH, GRID_HEIGHT, "generation_" + std::to_string(numGenerations) + ".txt");
    
    cleanupCUDA(d_input, d_output);
}

/**
 * Run GPU benchmark (naive version for comparison)
 */
void runGPUBenchmarkNaive(int numGenerations = 100) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "Running GPU Benchmark (Naive)..." << std::endl;
    std::cout << "========================================" << std::endl;
    
    std::vector<uint8_t> h_grid(GRID_SIZE);
    initializeGrid(h_grid.data(), GRID_WIDTH, GRID_HEIGHT, INITIAL_ALIVE_PROBABILITY);
    
    uint8_t* d_input;
    uint8_t* d_output;
    
    initializeCUDA(&d_input, &d_output, h_grid.data(), GRID_SIZE);
    
    // Warm-up run
    updateGridGPUNaive(d_input, d_output, GRID_WIDTH, GRID_HEIGHT);
    CUDA_CHECK(cudaDeviceSynchronize());
    
    Timer timer;
    
    for (int gen = 0; gen < numGenerations; ++gen) {
        updateGridGPUNaive(d_input, d_output, GRID_WIDTH, GRID_HEIGHT);
        swapPointers(&d_input, &d_output);
    }
    
    CUDA_CHECK(cudaDeviceSynchronize());
    double elapsed = timer.elapsed();
    
    double avgTime = elapsed / numGenerations;
    double fps = 1000.0 / avgTime;
    
    std::cout << "GPU Performance (Naive - No Shared Memory):" << std::endl;
    std::cout << "  Generations: " << numGenerations << std::endl;
    std::cout << "  Total Time: " << std::fixed << std::setprecision(2) << elapsed << " ms" << std::endl;
    std::cout << "  Avg Time/Gen: " << std::fixed << std::setprecision(3) << avgTime << " ms" << std::endl;
    std::cout << "  FPS: " << std::fixed << std::setprecision(1) << fps << std::endl;
    
    cleanupCUDA(d_input, d_output);
}

/**
 * Run console simulation
 */
void runConsoleSimulation(int numGenerations = 1000) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "Running Console Simulation..." << std::endl;
    std::cout << "Simulating " << numGenerations << " generations..." << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Initialize host grid
    std::vector<uint8_t> h_grid(GRID_SIZE);
    initializeGrid(h_grid.data(), GRID_WIDTH, GRID_HEIGHT, INITIAL_ALIVE_PROBABILITY);
    
    // Save initial state
    saveGridToFile(h_grid.data(), GRID_WIDTH, GRID_HEIGHT, "generation_0.txt");
    printGridSection(h_grid.data(), GRID_WIDTH, GRID_HEIGHT, 400, 400, 40);
    
    // Initialize device memory
    uint8_t* d_input;
    uint8_t* d_output;
    initializeCUDA(&d_input, &d_output, h_grid.data(), GRID_SIZE);
    
    Timer totalTimer;
    int saveInterval = numGenerations / 10; // Save 10 snapshots
    
    std::cout << "\nProgress:" << std::endl;
    
    for (int gen = 1; gen <= numGenerations; ++gen) {
        Timer genTimer;
        updateGridGPU(d_input, d_output, GRID_WIDTH, GRID_HEIGHT);
        CUDA_CHECK(cudaDeviceSynchronize());
        
        swapPointers(&d_input, &d_output);
        
        // Print progress every 10%
        if (gen % (numGenerations / 10) == 0) {
            double avgFPS = gen * 1000.0 / totalTimer.elapsed();
            std::cout << "  Gen " << std::setw(5) << gen << " / " << numGenerations 
                     << " | Avg FPS: " << std::fixed << std::setprecision(1) << avgFPS
                     << std::endl;
            
            // Save snapshot
            copyToHost(h_grid.data(), d_input, GRID_SIZE);
            saveGridToFile(h_grid.data(), GRID_WIDTH, GRID_HEIGHT, "generation_" + std::to_string(gen) + ".txt");
        }
    }
    
    double totalTime = totalTimer.elapsed();
    double avgFPS = numGenerations * 1000.0 / totalTime;
    
    // Copy final state
    copyToHost(h_grid.data(), d_input, GRID_SIZE);
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "Simulation Complete!" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Total Generations: " << numGenerations << std::endl;
    std::cout << "Total Time: " << std::fixed << std::setprecision(2) << totalTime << " ms" << std::endl;
    std::cout << "Average FPS: " << std::fixed << std::setprecision(1) << avgFPS << std::endl;
    std::cout << "\nFinal state:" << std::endl;
    printGridSection(h_grid.data(), GRID_WIDTH, GRID_HEIGHT, 400, 400, 40);
    
    // Cleanup
    cleanupCUDA(d_input, d_output);
}

/**
 * Main entry point (console version without OpenCV)
 */
int main(int argc, char** argv) {
    std::cout << R"(
╔════════════════════════════════════════════════════════════╗
║         Conway's Game of Life - CUDA Implementation        ║
║                Console Version (No OpenCV)                 ║
║                      RTX 4060 Optimized                    ║
╚════════════════════════════════════════════════════════════╝
)" << std::endl;
    
    // Print GPU information
    printGPUInfo();
    
    std::cout << "\nGrid Configuration:" << std::endl;
    std::cout << "  Grid Size: " << GRID_WIDTH << " x " << GRID_HEIGHT 
              << " (" << GRID_SIZE << " cells)" << std::endl;
    std::cout << "  Block Size: " << BLOCK_SIZE << " x " << BLOCK_SIZE 
              << " (" << BLOCK_SIZE * BLOCK_SIZE << " threads/block)" << std::endl;
    std::cout << "  Grid Blocks: " << (GRID_WIDTH / BLOCK_SIZE) << " x " << (GRID_HEIGHT / BLOCK_SIZE) 
              << " (" << (GRID_WIDTH / BLOCK_SIZE) * (GRID_HEIGHT / BLOCK_SIZE) << " blocks)" << std::endl;
    std::cout << "  Initial Alive Probability: " << (INITIAL_ALIVE_PROBABILITY * 100) << "%" << std::endl;
    
    // Check command line arguments
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "--benchmark" || arg == "-b") {
            runCPUBenchmark(100);
            runGPUBenchmarkNaive(100);
            runGPUBenchmark(100);
            
            std::cout << "\n========================================" << std::endl;
            std::cout << "Performance Summary" << std::endl;
            std::cout << "========================================" << std::endl;
            std::cout << "The GPU (optimized) should be 50-100x faster than CPU" << std::endl;
            std::cout << "The GPU (optimized) should be 2-3x faster than GPU (naive)" << std::endl;
        } else if (arg == "--simulate" || arg == "-s") {
            int gens = (argc > 2) ? std::stoi(argv[2]) : 1000;
            runConsoleSimulation(gens);
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "\nUsage: " << argv[0] << " [options]\n";
            std::cout << "Options:\n";
            std::cout << "  --benchmark, -b           Run benchmarks (CPU vs GPU)\n";
            std::cout << "  --simulate, -s [gens]     Run simulation for N generations (default: 1000)\n";
            std::cout << "  --help, -h                Show this help message\n";
            std::cout << "\nOutput files:\n";
            std::cout << "  generation_N.txt          Text file with grid state at generation N\n";
        }
    } else {
        // Default: run benchmark
        std::cout << "\nNo arguments provided. Running benchmarks...\n";
        std::cout << "Use --help to see available options.\n";
        
        runCPUBenchmark(100);
        runGPUBenchmarkNaive(100);
        runGPUBenchmark(100);
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "Performance Summary" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "GPU (optimized) is significantly faster!" << std::endl;
        std::cout << "\nTry: " << argv[0] << " --simulate 1000  (to run 1000 generations)" << std::endl;
    }
    
    std::cout << "\nProgram completed successfully!" << std::endl;
    return 0;
}
