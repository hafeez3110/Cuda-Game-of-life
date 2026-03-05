#include "utils.h"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <iomanip>

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
 * Convert grid to OpenCV image for visualization
 */
void gridToImage(const uint8_t* grid, cv::Mat& image, int width, int height) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint8_t value = grid[y * width + x] * 255;  // 0 or 255
            image.at<cv::Vec3b>(y, x) = cv::Vec3b(value, value, value);
        }
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
 * Main visualization loop
 */
void runVisualization() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "Starting Visualization..." << std::endl;
    std::cout << "Press 'q' to quit, 'r' to reset, SPACE to pause" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Initialize host grid
    std::vector<uint8_t> h_grid(GRID_SIZE);
    initializeGrid(h_grid.data(), GRID_WIDTH, GRID_HEIGHT, INITIAL_ALIVE_PROBABILITY);
    
    // Initialize device memory
    uint8_t* d_input;
    uint8_t* d_output;
    initializeCUDA(&d_input, &d_output, h_grid.data(), GRID_SIZE);
    
    // Create OpenCV window
    cv::namedWindow("Conway's Game of Life - CUDA", cv::WINDOW_NORMAL);
    cv::resizeWindow("Conway's Game of Life - CUDA", WINDOW_WIDTH, WINDOW_HEIGHT);
    
    cv::Mat image(GRID_HEIGHT, GRID_WIDTH, CV_8UC3);
    
    int generation = 0;
    bool paused = false;
    Timer fpsTimer;
    double totalTime = 0.0;
    int frameCount = 0;
    
    std::cout << "\nStarting simulation..." << std::endl;
    
    while (generation < MAX_GENERATIONS) {
        // Update grid on GPU
        if (!paused) {
            Timer genTimer;
            updateGridGPU(d_input, d_output, GRID_WIDTH, GRID_HEIGHT);
            CUDA_CHECK(cudaDeviceSynchronize());
            
            double genTime = genTimer.elapsed();
            totalTime += genTime;
            frameCount++;
            
            // Swap buffers for ping-pong
            swapPointers(&d_input, &d_output);
            generation++;
        }
        
        // Copy to host every frame for visualization
        copyToHost(h_grid.data(), d_input, GRID_SIZE);
        
        // Convert to image
        gridToImage(h_grid.data(), image, GRID_WIDTH, GRID_HEIGHT);
        
        // Display statistics on image
        double avgFPS = (totalTime > 0) ? (frameCount * 1000.0 / totalTime) : 0.0;
        double avgGenTime = (frameCount > 0) ? (totalTime / frameCount) : 0.0;
        
        std::string genText = "Generation: " + std::to_string(generation);
        std::string fpsText = "FPS: " + std::to_string(static_cast<int>(avgFPS));
        std::string timeText = "Avg: " + std::to_string(avgGenTime).substr(0, 5) + " ms";
        std::string statusText = paused ? "PAUSED" : "Running";
        
        cv::putText(image, genText, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 0), 2);
        cv::putText(image, fpsText, cv::Point(10, 60), cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 0), 2);
        cv::putText(image, timeText, cv::Point(10, 90), cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 0), 2);
        cv::putText(image, statusText, cv::Point(10, 120), cv::FONT_HERSHEY_SIMPLEX, 0.7, 
                   paused ? cv::Scalar(0, 0, 255) : cv::Scalar(0, 255, 0), 2);
        
        cv::imshow("Conway's Game of Life - CUDA", image);
        
        // Handle keyboard input
        int key = cv::waitKey(1);
        if (key == 'q' || key == 27) {  // 'q' or ESC
            break;
        } else if (key == 'r') {  // Reset
            generation = 0;
            totalTime = 0.0;
            frameCount = 0;
            initializeGrid(h_grid.data(), GRID_WIDTH, GRID_HEIGHT, INITIAL_ALIVE_PROBABILITY);
            CUDA_CHECK(cudaMemcpy(d_input, h_grid.data(), GRID_SIZE * sizeof(uint8_t), cudaMemcpyHostToDevice));
            std::cout << "Grid reset!" << std::endl;
        } else if (key == ' ') {  // Space - pause/unpause
            paused = !paused;
            std::cout << (paused ? "Paused" : "Resumed") << std::endl;
        }
        
        // Print stats every 100 generations
        if (generation % 100 == 0 && generation > 0 && !paused) {
            std::cout << "Gen: " << std::setw(5) << generation 
                     << " | FPS: " << std::setw(6) << std::fixed << std::setprecision(1) << avgFPS
                     << " | Avg Time: " << std::fixed << std::setprecision(3) << avgGenTime << " ms"
                     << std::endl;
        }
    }
    
    std::cout << "\nSimulation ended at generation " << generation << std::endl;
    std::cout << "Average FPS: " << std::fixed << std::setprecision(1) << (frameCount * 1000.0 / totalTime) << std::endl;
    
    // Cleanup
    cleanupCUDA(d_input, d_output);
    cv::destroyAllWindows();
}

/**
 * Main entry point
 */
int main(int argc, char** argv) {
    std::cout << R"(
╔════════════════════════════════════════════════════════════╗
║         Conway's Game of Life - CUDA Implementation        ║
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
    
    // Check command line arguments for mode selection
    bool benchmarkMode = false;
    bool visualMode = true;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--benchmark" || arg == "-b") {
            benchmarkMode = true;
            visualMode = false;
        } else if (arg == "--both") {
            benchmarkMode = true;
            visualMode = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "\nUsage: " << argv[0] << " [options]\n";
            std::cout << "Options:\n";
            std::cout << "  --benchmark, -b    Run benchmarks only (no visualization)\n";
            std::cout << "  --both             Run benchmarks then visualization\n";
            std::cout << "  --help, -h         Show this help message\n";
            std::cout << "  (no arguments)     Run visualization only\n";
            return 0;
        }
    }
    
    // Run benchmarks if requested
    if (benchmarkMode) {
        runCPUBenchmark(100);
        runGPUBenchmarkNaive(100);
        runGPUBenchmark(100);
        
        // Calculate speedups
        std::cout << "\n========================================" << std::endl;
        std::cout << "Performance Summary" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "The GPU (optimized) should be 50-100x faster than CPU" << std::endl;
        std::cout << "The GPU (optimized) should be 2-3x faster than GPU (naive)" << std::endl;
        std::cout << "========================================\n" << std::endl;
    }
    
    // Run visualization if requested
    if (visualMode) {
        std::cout << "\nStarting visualization mode...\n" << std::endl;
        runVisualization();
    }
    
    std::cout << "\nProgram completed successfully!" << std::endl;
    return 0;
}
