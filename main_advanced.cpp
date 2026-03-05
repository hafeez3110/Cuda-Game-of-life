#include "utils_advanced.h"
#include "gameoflife_advanced.cuh"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <algorithm>

// Global state for mouse interaction
struct MouseState {
    bool isPanning;
    bool isDrawing;
    cv::Point lastPos;
    cv::Point offset;
    double zoom;
    
    MouseState() : isPanning(false), isDrawing(false), offset(0, 0), zoom(1.0) {}
};

MouseState g_mouseState;
CUDAContext* g_ctx = nullptr;
bool g_needUpdate = false;

// Mouse callback for interactive editing
void mouseCallback(int event, int x, int y, int flags, void* userdata) {
    if (event == cv::EVENT_LBUTTONDOWN) {
        g_mouseState.lastPos = cv::Point(x, y);
        if (flags & cv::EVENT_FLAG_CTRLKEY) {
            g_mouseState.isPanning = true;
        } else {
            g_mouseState.isDrawing = true;
        }
    }
    else if (event == cv::EVENT_LBUTTONUP) {
        g_mouseState.isPanning = false;
        g_mouseState.isDrawing = false;
    }
    else if (event == cv::EVENT_MOUSEMOVE) {
        if (g_mouseState.isPanning) {
            cv::Point delta = cv::Point(x, y) - g_mouseState.lastPos;
            g_mouseState.offset += delta;
            g_mouseState.lastPos = cv::Point(x, y);
            g_needUpdate = true;
        }
        else if (g_mouseState.isDrawing && g_ctx) {
            // Toggle cell at mouse position (account for 4x scaling)
            int cellX = (int)((x / 4.0 - g_mouseState.offset.x) / g_mouseState.zoom);
            int cellY = (int)((y / 4.0 - g_mouseState.offset.y) / g_mouseState.zoom);
            
            if (cellX >= 0 && cellX < GRID_WIDTH && cellY >= 0 && cellY < GRID_HEIGHT) {
                // TODO: Implement cell toggling
                g_needUpdate = true;
            }
        }
    }
    else if (event == cv::EVENT_MOUSEWHEEL) {
        double delta = cv::getMouseWheelDelta(flags) > 0 ? 1.1 : 0.9;
        g_mouseState.zoom *= delta;
        g_mouseState.zoom = std::max(0.1, std::min(10.0, g_mouseState.zoom));
        g_needUpdate = true;
    }
}

// Convert grid to colored image with visualization modes
enum class ColorMode {
    BlackWhite,
    CellAge,
    BirthDeath,
    Heatmap
};

void gridToImageAdvanced(const unsigned char* grid, cv::Mat& image, ColorMode mode,
                        const std::vector<int>& cellAges = std::vector<int>()) {
    if (mode == ColorMode::BlackWhite) {
        // Fast grayscale
        unsigned char* imgData = image.data;
        for (int i = 0; i < GRID_SIZE; i++) {
            imgData[i] = grid[i] ? 255 : 0;
        }
    }
    else if (mode == ColorMode::CellAge && !cellAges.empty()) {
        // Color by cell age (heatmap)
        cv::Mat gray(GRID_HEIGHT, GRID_WIDTH, CV_8UC1);
        for (int i = 0; i < GRID_SIZE; i++) {
            if (grid[i]) {
                int age = std::min(cellAges[i], 255);
                gray.data[i] = age;
            } else {
                gray.data[i] = 0;
            }
        }
        cv::applyColorMap(gray, image, cv::COLORMAP_JET);
    }
    else {
        // Default to black/white
        for (int i = 0; i < GRID_SIZE; i++) {
            image.data[i] = grid[i] ? 255 : 0;
        }
    }
}

// Performance sparkline data
class Sparkline {
    std::vector<double> data;
    size_t maxSize;
    
public:
    Sparkline(size_t size = 100) : maxSize(size) {}
    
    void add(double value) {
        data.push_back(value);
        if (data.size() > maxSize) {
            data.erase(data.begin());
        }
    }
    
    void draw(cv::Mat& img, cv::Rect region, double minVal, double maxVal) {
        if (data.empty()) return;
        
        cv::rectangle(img, region, cv::Scalar(0, 0, 0), -1);
        
        double range = maxVal - minVal;
        if (range <= 0) range = 1.0;
        
        for (size_t i = 1; i < data.size(); i++) {
            int x1 = region.x + (i - 1) * region.width / maxSize;
            int y1 = region.y + region.height - 
                    (int)((data[i-1] - minVal) / range * region.height);
            int x2 = region.x + i * region.width / maxSize;
            int y2 = region.y + region.height - 
                    (int)((data[i] - minVal) / range * region.height);
            
            cv::line(img, cv::Point(x1, y1), cv::Point(x2, y2), 
                    cv::Scalar(0, 255, 0), 1);
        }
    }
};

// CSV logger
class CSVLogger {
    std::ofstream file;
    bool headerWritten;
    
public:
    CSVLogger(const std::string& filename) : headerWritten(false) {
        file.open(filename);
        if (file.is_open()) {
            file << "Generation,KernelTime_ms,TransferTime_ms,DisplayTime_ms,TotalTime_ms,FPS,AliveCells\n";
            headerWritten = true;
        }
    }
    
    ~CSVLogger() {
        if (file.is_open()) file.close();
    }
    
    void log(const PerformanceMetrics& metrics, double fps) {
        if (file.is_open()) {
            file << metrics.generation << ","
                 << metrics.kernelTime << ","
                 << metrics.transferTime << ","
                 << metrics.displayTime << ","
                 << metrics.totalTime << ","
                 << fps << ","
                 << metrics.aliveCells << "\n";
        }
    }
};

// RLE Pattern Loader
struct RLEPattern {
    std::string name;
    std::string rule;
    int width;
    int height;
    std::vector<unsigned char> data;
    
    static RLEPattern fromFile(const std::string& filename) {
        RLEPattern pattern;
        std::ifstream file(filename);
        
        if (!file.is_open()) {
            std::cerr << "Could not open pattern file: " << filename << std::endl;
            return pattern;
        }
        
        std::string line;
        while (std::getline(file, line)) {
            // Skip comments
            if (line.empty() || line[0] == '#') {
                if (line.find("#N") == 0) {
                    pattern.name = line.substr(3);
                }
                continue;
            }
            
            // Parse header line: x = m, y = n, rule = B3/S23
            if (line[0] == 'x') {
                size_t xPos = line.find("x =");
                size_t yPos = line.find("y =");
                size_t rPos = line.find("rule =");
                
                if (xPos != std::string::npos && yPos != std::string::npos) {
                    pattern.width = std::stoi(line.substr(xPos + 3));
                    pattern.height = std::stoi(line.substr(yPos + 3));
                }
                
                if (rPos != std::string::npos) {
                    pattern.rule = line.substr(rPos + 7);
                    // Remove trailing whitespace
                    pattern.rule.erase(pattern.rule.find_last_not_of(" \n\r\t") + 1);
                }
                
                pattern.data.resize(pattern.width * pattern.height, 0);
                continue;
            }
            
            // Parse RLE data
            int x = 0, y = 0;
            int runCount = 0;
            
            for (char c : line) {
                if (isdigit(c)) {
                    runCount = runCount * 10 + (c - '0');
                } else if (c == 'b') {
                    // Dead cells
                    if (runCount == 0) runCount = 1;
                    x += runCount;
                    runCount = 0;
                } else if (c == 'o') {
                    // Live cells
                    if (runCount == 0) runCount = 1;
                    for (int i = 0; i < runCount; i++) {
                        if (x < pattern.width && y < pattern.height) {
                            pattern.data[y * pattern.width + x] = 1;
                        }
                        x++;
                    }
                    runCount = 0;
                } else if (c == '$') {
                    // End of row
                    if (runCount == 0) runCount = 1;
                    y += runCount;
                    x = 0;
                    runCount = 0;
                } else if (c == '!') {
                    // End of pattern
                    break;
                }
            }
        }
        
        return pattern;
    }
    
    void placeInGrid(unsigned char* grid, int gridWidth, int gridHeight, 
                    int offsetX, int offsetY) const {
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int gx = (offsetX + x + gridWidth) % gridWidth;
                int gy = (offsetY + y + gridHeight) % gridHeight;
                grid[gy * gridWidth + gx] = data[y * width + x];
            }
        }
    }
};

// Famous patterns embedded
namespace Patterns {
    RLEPattern Glider() {
        RLEPattern p;
        p.name = "Glider";
        p.rule = "B3/S23";
        p.width = 3;
        p.height = 3;
        p.data = {
            0, 1, 0,
            0, 0, 1,
            1, 1, 1
        };
        return p;
    }
    
    RLEPattern Blinker() {
        RLEPattern p;
        p.name = "Blinker";
        p.rule = "B3/S23";
        p.width = 3;
        p.height = 3;
        p.data = {
            0, 0, 0,
            1, 1, 1,
            0, 0, 0
        };
        return p;
    }
    
    RLEPattern GliderGun() {
        RLEPattern p;
        p.name = "Gosper Glider Gun";
        p.rule = "B3/S23";
        p.width = 36;
        p.height = 9;
        p.data.resize(36 * 9, 0);
        
        // Left square
        p.data[1 * 36 + 24] = 1;
        p.data[2 * 36 + 22] = 1; p.data[2 * 36 + 24] = 1;
        p.data[3 * 36 + 12] = 1; p.data[3 * 36 + 13] = 1;
        p.data[3 * 36 + 20] = 1; p.data[3 * 36 + 21] = 1;
        p.data[3 * 36 + 34] = 1; p.data[3 * 36 + 35] = 1;
        // ... (full pattern would be quite long)
        
        return p;
    }
}

// Advanced visualization with all features
void runAdvancedVisualization(int maxGenerations, const std::string& logFile = "",
                              const std::string& videoFile = "") {
    // Initialize CUDA context
    g_ctx = new CUDAContext();
    
    // Initialize random grid
    std::vector<unsigned char> hostGrid(GRID_SIZE);
    srand(time(nullptr));
    for (int i = 0; i < GRID_SIZE; i++) {
        hostGrid[i] = (rand() / (float)RAND_MAX) < INITIAL_ALIVE_PROBABILITY ? 1 : 0;
    }
    
    g_ctx->initializeGrid(hostGrid.data());
    
    // Print GPU info
    printGPUInfo();
    
    // Create window
    cv::namedWindow("Conway's Game of Life - Advanced", cv::WINDOW_NORMAL);
    cv::resizeWindow("Conway's Game of Life - Advanced", GRID_WIDTH * 4, GRID_HEIGHT * 4);
    cv::setMouseCallback("Conway's Game of Life - Advanced", mouseCallback);
    
    // Create trackbars
    int speed = 1;
    int colorModeInt = 0;
    int ruleSelector = 0;
    
    cv::createTrackbar("Speed", "Conway's Game of Life - Advanced", &speed, 10);
    cv::createTrackbar("Color Mode", "Conway's Game of Life - Advanced", &colorModeInt, 3);
    cv::createTrackbar("Rule", "Conway's Game of Life - Advanced", &ruleSelector, 3);
    
    // Available rules
    std::vector<CARule> rules = {
        CARule::ConwaysLife(),
        CARule::HighLife(),
        CARule::Seeds(),
        CARule::DayAndNight()
    };
    
    // Performance tracking
    Sparkline fpsSparkline(200);
    Sparkline gpuTimeSparkline(200);
    CSVLogger* logger = nullptr;
    if (!logFile.empty()) {
        logger = new CSVLogger(logFile);
    }
    
    // Video writer
    cv::VideoWriter videoWriter;
    if (!videoFile.empty()) {
        videoWriter.open(videoFile, cv::VideoWriter::fourcc('m', 'p', '4', 'v'),
                        60.0, cv::Size(GRID_WIDTH, GRID_HEIGHT), false);
    }
    
    // Cell age tracking for visualization
    std::vector<int> cellAges(GRID_SIZE, 0);
    
    // Create CUDA graph for performance
    #if USE_CUDA_GRAPHS
    g_ctx->createGraph();
    #endif
    
    // Main loop
    Timer totalTimer;
    Timer fpsTimer;
    Timer gpuTimer;
    int generation = 0;
    int frameCount = 0;
    double fps = 0;
    bool paused = false;
    bool running = true;
    int lastRuleSelector = 0;
    ColorMode colorMode = ColorMode::BlackWhite;
    
    cv::Mat displayMat(GRID_HEIGHT, GRID_WIDTH, CV_8UC1);
    cv::Mat colorMat(GRID_HEIGHT, GRID_WIDTH, CV_8UC3);
    
    printf("\n========================================\n");
    printf("Advanced Visualization Started\n");
    printf("========================================\n");
    printf("Controls:\n");
    printf("  SPACE - Pause/Resume\n");
    printf("  'r' - Reset grid\n");
    printf("  'q' or ESC - Quit\n");
    printf("  'g' - Load Glider pattern\n");
    printf("  'b' - Load Blinker pattern\n");
    printf("  'G' - Load Glider Gun pattern\n");
    printf("  'p' - Save screenshot\n");
    printf("  Mouse wheel - Zoom\n");
    printf("  Ctrl+Drag - Pan\n");
    printf("  Click - Toggle cell\n");
    printf("========================================\n\n");
    
    while (running && generation < maxGenerations) {
        // Check for rule change
        if (ruleSelector != lastRuleSelector) {
            g_ctx->setRule(rules[ruleSelector]);
            lastRuleSelector = ruleSelector;
            printf("Rule changed to: %s\n", rules[ruleSelector].name.c_str());
        }
        
        // Update color mode
        colorMode = static_cast<ColorMode>(colorModeInt);
        
        if (!paused) {
            // Run simulation step
            NVTX_RANGE("SimulationFrame");
            
            gpuTimer.reset();
            
            #if USE_CUDA_GRAPHS
            g_ctx->stepGraph();
            #else
            g_ctx->step();
            #endif
            
            double gpuTime = gpuTimer.elapsed();
            
            // Async transfer
            int bufferIdx = generation % 2;
            g_ctx->transferToHostAsync(bufferIdx);
            g_ctx->syncTransfer();
            
            double transferTime = gpuTimer.elapsed() - gpuTime;
            
            // Update cell ages
            unsigned char* gridData = g_ctx->getPinnedBuffer(bufferIdx);
            for (int i = 0; i < GRID_SIZE; i++) {
                if (gridData[i]) {
                    cellAges[i]++;
                } else {
                    cellAges[i] = 0;
                }
            }
            
            generation++;
        } else {
            // When paused, use current grid
            unsigned char* gridData = g_ctx->getPinnedBuffer(0);
        }
        
        // Render
        Timer displayTimer;
        
        unsigned char* gridData = g_ctx->getPinnedBuffer(generation % 2);
        gridToImageAdvanced(gridData, displayMat, colorMode, cellAges);
        
        // Scale up the image 4x with NEAREST interpolation for clearly visible cells
        cv::Mat scaledMat;
        cv::resize(displayMat, scaledMat, cv::Size(GRID_WIDTH * 4, GRID_HEIGHT * 4), 0, 0, cv::INTER_NEAREST);
        
        // Convert to color for overlay
        cv::cvtColor(scaledMat, colorMat, cv::COLOR_GRAY2BGR);
        
        // Add overlay with statistics (scaled for 4x display)
        std::string genText = "Gen: " + std::to_string(generation);
        std::string fpsText = "FPS: " + std::to_string((int)fps);
        std::string ruleText = "Rule: " + rules[ruleSelector].name;
        std::string statusText = paused ? "PAUSED" : "Running";
        
        cv::putText(colorMat, genText, cv::Point(40, 120), 
                   cv::FONT_HERSHEY_SIMPLEX, 2.8, cv::Scalar(0, 255, 0), 6);
        cv::putText(colorMat, fpsText, cv::Point(40, 240),
                   cv::FONT_HERSHEY_SIMPLEX, 2.8, cv::Scalar(0, 255, 0), 6);
        cv::putText(colorMat, ruleText, cv::Point(40, 360),
                   cv::FONT_HERSHEY_SIMPLEX, 2.8, cv::Scalar(0, 255, 0), 6);
        cv::putText(colorMat, statusText, cv::Point(40, 480),
                   cv::FONT_HERSHEY_SIMPLEX, 2.8, 
                   paused ? cv::Scalar(0, 0, 255) : cv::Scalar(0, 255, 0), 6);
        
        // Draw sparklines (scaled positions for 4x)
        cv::Rect fpsSparkRect(GRID_WIDTH * 4 - 840, 40, 800, 200);
        cv::Rect gpuSparkRect(GRID_WIDTH * 4 - 840, 280, 800, 200);
        fpsSparkline.draw(colorMat, fpsSparkRect, 0, 300);
        gpuTimeSparkline.draw(colorMat, gpuSparkRect, 0, 1.0);
        
        cv::imshow("Conway's Game of Life - Advanced", colorMat);
        
        // Write to video
        if (videoWriter.isOpened()) {
            videoWriter.write(displayMat);
        }
        
        double displayTime = displayTimer.elapsed();
        
        // Calculate FPS
        frameCount++;
        if (frameCount % 10 == 0) {
            double elapsed = fpsTimer.elapsed();
            fps = 10000.0 / elapsed;
            fpsTimer.reset();
            
            fpsSparkline.add(fps);
            gpuTimeSparkline.add(gpuTimer.elapsed());
            
            if (generation % 100 == 0) {
                printf("Gen: %6d | FPS: %6.1f | GPU: %5.3f ms | Status: %s\n",
                       generation, fps, gpuTimer.elapsed(),
                       paused ? "PAUSED" : "Running");
            }
        }
        
        // Log metrics
        if (logger && !paused) {
            PerformanceMetrics metrics;
            metrics.generation = generation;
            metrics.kernelTime = gpuTimer.elapsed();
            metrics.displayTime = displayTime;
            metrics.totalTime = totalTimer.elapsed();
            logger->log(metrics, fps);
        }
        
        // Handle keyboard input
        int waitTime = std::max(1, 11 - speed);
        int key = cv::waitKey(waitTime);
        
        if (key == 'q' || key == 27) { // ESC
            running = false;
        }
        else if (key == ' ') {
            paused = !paused;
        }
        else if (key == 'r') {
            // Reset grid
            for (int i = 0; i < GRID_SIZE; i++) {
                hostGrid[i] = (rand() / (float)RAND_MAX) < INITIAL_ALIVE_PROBABILITY ? 1 : 0;
            }
            g_ctx->initializeGrid(hostGrid.data());
            cellAges.assign(GRID_SIZE, 0);
            generation = 0;
        }
        else if (key == 'g') {
            // Load glider
            Patterns::Glider().placeInGrid(hostGrid.data(), GRID_WIDTH, GRID_HEIGHT, 
                                          GRID_WIDTH/2, GRID_HEIGHT/2);
            g_ctx->initializeGrid(hostGrid.data());
        }
        else if (key == 'b') {
            // Load blinker
            Patterns::Blinker().placeInGrid(hostGrid.data(), GRID_WIDTH, GRID_HEIGHT,
                                           GRID_WIDTH/2, GRID_HEIGHT/2);
            g_ctx->initializeGrid(hostGrid.data());
        }
        else if (key == 'G') {
            // Load glider gun
            Patterns::GliderGun().placeInGrid(hostGrid.data(), GRID_WIDTH, GRID_HEIGHT,
                                             GRID_WIDTH/4, GRID_HEIGHT/2);
            g_ctx->initializeGrid(hostGrid.data());
        }
        else if (key == 'p') {
            // Save screenshot
            std::string filename = "screenshot_gen" + std::to_string(generation) + ".png";
            cv::imwrite(filename, colorMat);
            printf("Saved screenshot: %s\n", filename.c_str());
        }
    }
    
    // Cleanup
    if (logger) delete logger;
    if (videoWriter.isOpened()) videoWriter.release();
    delete g_ctx;
    
    printf("\n========================================\n");
    printf("Simulation Complete!\n");
    printf("========================================\n");
    printf("Total Generations: %d\n", generation);
    printf("Total Time: %.2f seconds\n", totalTimer.elapsed() / 1000.0);
    printf("Average FPS: %.1f\n", generation / (totalTimer.elapsed() / 1000.0));
    printf("========================================\n\n");
}

int main(int argc, char** argv) {
    int maxGenerations = 10000;
    std::string logFile = "";
    std::string videoFile = "";
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-g" && i + 1 < argc) {
            maxGenerations = std::stoi(argv[++i]);
        }
        else if (arg == "-log" && i + 1 < argc) {
            logFile = argv[++i];
        }
        else if (arg == "-video" && i + 1 < argc) {
            videoFile = argv[++i];
        }
        else if (arg == "-h" || arg == "--help") {
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  -g <num>        Maximum generations (default: 10000)\n");
            printf("  -log <file>     CSV log file for metrics\n");
            printf("  -video <file>   Output video file (MP4)\n");
            printf("  -h, --help      Show this help\n");
            return 0;
        }
    }
    
    runAdvancedVisualization(maxGenerations, logFile, videoFile);
    
    return 0;
}
