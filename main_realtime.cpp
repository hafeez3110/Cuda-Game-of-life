#include "utils.h"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <iomanip>
#include <string>

// CUDA function declarations
extern "C" {
    void initializeCUDA(uint8_t** d_input, uint8_t** d_output, const uint8_t* h_grid, int size);
    void updateGridGPU(uint8_t* d_input, uint8_t* d_output, int width, int height);
    void copyToHost(uint8_t* h_grid, const uint8_t* d_grid, int size);
    void cleanupCUDA(uint8_t* d_input, uint8_t* d_output);
    void swapPointers(uint8_t** a, uint8_t** b);
}

// ====================== Presentation Visuals (Upscale + Grid + Birth/Death) ======================
// Runtime presentation controls
static int  g_scale = 5;             // pixels per cell (3–8 recommended on large screens)
static bool g_showGrid = true;       // toggle with 'g'
static bool g_showBirthDeath = true; // toggle with 'c'
// Performance / utilization controls
static int  g_stepsPerFrame = 1;     // how many generations to advance before a render
static const int kMaxStepsPerFrame = 500; // safety cap to avoid extremely long UI stalls
static bool g_headless = false;      // if true, skip visualization to maximize GPU utilization
static bool g_autoSteps = false;     // auto-adjust steps/frame to target utilization range (toggle 'a')

// Metrics collection for summary page
static std::vector<double> g_sampleComputeMs;    // batch compute time (ms)
static std::vector<double> g_sampleFrameMs;      // total frame time (ms)
static std::vector<double> g_sampleUtilPercent;  // compute/frame * 100
static std::vector<int>    g_sampleGeneration;   // generation after batch

enum class Theme { BW = 0, Night, Emerald };
static Theme g_theme = Theme::BW;    // toggle with 'm'

// Build a reusable grid overlay for the current scale (drawn once, blended every frame)
static cv::Mat makeGridOverlay(int width, int height, int scale,
                               cv::Scalar color = {40,40,40})
{
    const int W = width * scale;
    const int H = height * scale;
    cv::Mat overlay(H, W, CV_8UC3, cv::Scalar(0,0,0));
    // Draw grid lines every 'scale' pixels
    for (int x = 0; x <= W; x += scale) cv::line(overlay, {x,0}, {x,H}, color, 1, cv::LINE_8);
    for (int y = 0; y <= H; y += scale) cv::line(overlay, {0,y}, {W,y}, color, 1, cv::LINE_8);
    return overlay;
}

static void applyTheme(cv::Mat& bgr)
{
    switch (g_theme) {
        case Theme::BW: break; // no tint
        case Theme::Night: {
            cv::Mat mask; cv::inRange(bgr, cv::Scalar(200,200,200), cv::Scalar(255,255,255), mask);
            bgr.setTo(cv::Scalar(200,160,60), mask); // BGR (blue-ish)
            break;
        }
        case Theme::Emerald: {
            cv::Mat mask; cv::inRange(bgr, cv::Scalar(200,200,200), cv::Scalar(255,255,255), mask);
            bgr.setTo(cv::Scalar(60,200,60), mask);  // green-ish
            break;
        }
    }
}

// Render: upscale + optional birth/death coloring + optional grid overlay
// currLogical/prevLogical: CV_8UC1 with values 0/1
static void renderFrame(const cv::Mat& currLogical,
                        const cv::Mat& prevLogical,
                        int scale,
                        bool showGrid,
                        bool showBirthDeath,
                        cv::Mat& gridOverlayCache,
                        cv::Mat& outBGR)
{
    CV_Assert(currLogical.type() == CV_8UC1);

    // Convert 0/1 to 0/255
    cv::Mat curr255; currLogical.convertTo(curr255, CV_8U, 255);

    // Upscale with nearest-neighbor (crisp, pixel-art look)
    const int W = currLogical.cols * scale;
    const int H = currLogical.rows * scale;
    cv::Mat upGray; cv::resize(curr255, upGray, cv::Size(W, H), 0, 0, cv::INTER_NEAREST);

    // Optional: subtle cell gap for scale >= 3
    if (scale >= 3) {
        cv::erode(upGray, upGray, cv::Mat(), cv::Point(-1,-1), 1);
    }

    // Convert to BGR (white alive, black dead)
    cv::cvtColor(upGray, outBGR, cv::COLOR_GRAY2BGR);

    // Births (green) and Deaths (red) overlays
    if (showBirthDeath && !prevLogical.empty()) {
        CV_Assert(prevLogical.type() == CV_8UC1);
        cv::Mat births, deaths;
        // births: current=1 and previous=0; deaths: current=0 and previous=1
        cv::Mat currBin = currLogical > 0;
        cv::Mat prevBin = prevLogical > 0;
        cv::bitwise_and(currBin, ~prevBin, births);
        cv::bitwise_and(~currBin, prevBin, deaths);

        if (!births.empty()) {
            cv::Mat birthsUp; cv::resize(births, birthsUp, cv::Size(W, H), 0, 0, cv::INTER_NEAREST);
            outBGR.setTo(cv::Scalar(0,180,0), birthsUp); // green
        }
        if (!deaths.empty()) {
            cv::Mat deathsUp; cv::resize(deaths, deathsUp, cv::Size(W, H), 0, 0, cv::INTER_NEAREST);
            // Tint pixels that are now dead
            cv::Mat deadMask; cv::inRange(upGray, 0, 0, deadMask);
            cv::bitwise_and(deadMask, deathsUp, deadMask);
            outBGR.setTo(cv::Scalar(0,0,180), deadMask); // red tint on dead
        }
    }

    // Theme tinting
    applyTheme(outBGR);

    // Grid overlay (rebuild if scale/window changed)
    if (showGrid) {
        static int cachedScale = -1;
        if (gridOverlayCache.empty() || cachedScale != scale ||
            gridOverlayCache.cols != W || gridOverlayCache.rows != H) {
            gridOverlayCache = makeGridOverlay(currLogical.cols, currLogical.rows, scale);
            cachedScale = scale;
        }
        cv::addWeighted(outBGR, 1.0, gridOverlayCache, 0.35, 0.0, outBGR);
    }
}

/**
 * Convert grid to OpenCV image for visualization (optimized version)
 * Creates a grayscale image where white = alive, black = dead
 */
void gridToImage(const uint8_t* grid, cv::Mat& image, int width, int height) {
    // Use direct memory access for better performance
    uint8_t* imageData = image.data;
    const int totalPixels = width * height;
    
    for (int i = 0; i < totalPixels; ++i) {
        imageData[i] = grid[i] * 255;  // 0 -> 0 (black), 1 -> 255 (white)
    }
}

/**
 * Real-time OpenCV visualization of Game of Life
 * 
 * This function runs the simulation and displays it in real-time using OpenCV.
 * - White pixels = alive cells
 * - Black pixels = dead cells
 * - Updates smoothly with minimal delay
 * - Uses asynchronous copying to avoid blocking GPU
 */
void runRealtimeVisualization(int maxGenerations = 10000) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "Starting Real-time Visualization..." << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  'q' or ESC - Quit" << std::endl;
    std::cout << "  'r' - Reset grid" << std::endl;
    std::cout << "  SPACE - Pause/Resume" << std::endl;
    std::cout << "  '[' / ']' - Decrease / Increase steps per frame" << std::endl;
    std::cout << "  'h' - Toggle headless (compute only)" << std::endl;
    std::cout << "  'g' - Toggle grid overlay" << std::endl;
    std::cout << "  'c' - Toggle birth/death coloring" << std::endl;
    std::cout << "  'm' - Cycle theme" << std::endl;
    std::cout << "  'a' - Toggle auto-steps (adaptive utilization)" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    // Initialize host grid
    std::vector<uint8_t> h_grid(GRID_SIZE);
    initializeGrid(h_grid.data(), GRID_WIDTH, GRID_HEIGHT, INITIAL_ALIVE_PROBABILITY);
    
    // Initialize device memory
    uint8_t* d_input;
    uint8_t* d_output;
    initializeCUDA(&d_input, &d_output, h_grid.data(), GRID_SIZE);
    
    // Create OpenCV window
    const std::string kWindow = "Game of Life - CUDA";
    cv::namedWindow(kWindow, cv::WINDOW_NORMAL);
    cv::resizeWindow(kWindow, WINDOW_WIDTH, WINDOW_HEIGHT);
    // Buffers for presentation rendering
    cv::Mat prevLogical; // CV_8UC1 0/1
    cv::Mat frameBGR;    // Upscaled color frame
    cv::Mat gridOverlay; // cached grid lines
    
    // Performance tracking
    int generation = 0;
    bool paused = false;
    Timer totalTimer;
    double totalComputeTime = 0.0;
    int computeCount = 0;
    
    // FPS calculation
    const int FPS_UPDATE_INTERVAL = 10;  // Update FPS every N frames
    Timer fpsTimer;
    int framesSinceLastFpsUpdate = 0;
    double currentFPS = 0.0;
    
    std::cout << "Simulation started. Window opened." << std::endl;
    std::cout << "Generation updates will be displayed in real-time...\n" << std::endl;
    
    Timer frameTimer; // measures total frame duration
    while (generation < maxGenerations) {
        frameTimer.reset();
        // ======== CUDA COMPUTATION ========
        if (!paused) {
            Timer computeTimer;
            for (int step = 0; step < g_stepsPerFrame; ++step) {
                updateGridGPU(d_input, d_output, GRID_WIDTH, GRID_HEIGHT);
                swapPointers(&d_input, &d_output);
            }
            // Synchronize once per batch
            CUDA_CHECK(cudaDeviceSynchronize());
            double computeTime = computeTimer.elapsed();
            totalComputeTime += computeTime;
            computeCount += g_stepsPerFrame; // generation count for average per gen
            generation += g_stepsPerFrame;

            // Auto-adjust steps/frame to push utilization into target window (35% - 70%)
            // We'll compute utilization from previous frame's metrics when available.
            if (g_autoSteps && !g_headless) {
                if (!g_sampleUtilPercent.empty()) {
                    double lastUtil = g_sampleUtilPercent.back();
                    if (lastUtil < 35.0 && g_stepsPerFrame < kMaxStepsPerFrame) {
                        g_stepsPerFrame = std::min(kMaxStepsPerFrame, g_stepsPerFrame + 1);
                    } else if (lastUtil > 70.0 && g_stepsPerFrame > 1) {
                        g_stepsPerFrame = std::max(1, g_stepsPerFrame - 1);
                    }
                }
            }
        }

        // Headless mode: skip all visualization work (except minimal key polling)
        double frameElapsedSoFar = frameTimer.elapsed(); // includes compute time so far
        if (!g_headless) {
            // ======== COPY TO HOST (for visualization) ========
            copyToHost(h_grid.data(), d_input, GRID_SIZE);
            // Wrap current logical grid (0/1) without copy
            cv::Mat currLogical(GRID_HEIGHT, GRID_WIDTH, CV_8UC1, h_grid.data());

        // Calculate FPS
        framesSinceLastFpsUpdate++;
        if (framesSinceLastFpsUpdate >= FPS_UPDATE_INTERVAL) {
            double elapsed = fpsTimer.elapsed();
            currentFPS = (framesSinceLastFpsUpdate * 1000.0) / elapsed;
            framesSinceLastFpsUpdate = 0;
            fpsTimer.reset();
        }
        
            // Build presentation frame
            renderFrame(currLogical, prevLogical, g_scale, g_showGrid, g_showBirthDeath, gridOverlay, frameBGR);

            // Add text overlay with statistics
            double avgComputeTime = (computeCount > 0) ? (totalComputeTime / computeCount) : 0.0;

            std::string genText = "Generation: " + std::to_string(generation);
            std::string fpsText = "FPS: " + std::to_string(static_cast<int>(currentFPS));
            std::string avgTimeText = "Avg GPU Time: " + std::to_string(avgComputeTime).substr(0, 5) + " ms";
            std::string statusText = paused ? "PAUSED" : "Running";
            std::string stepsText = "Steps/Frame: " + std::to_string(g_stepsPerFrame);
            std::string headlessText = g_headless ? "Headless: ON" : "Headless: OFF";

            // HUD with soft shadow (expand rectangle for extra lines)
            cv::rectangle(frameBGR, cv::Point(8, 8), cv::Point(400, 220), cv::Scalar(0, 0, 0), -1);
            cv::rectangle(frameBGR, cv::Point(8, 8), cv::Point(400, 220), cv::Scalar(0, 255, 0), 2);

            auto putShadowText = [&](const std::string& t, cv::Point org, const cv::Scalar& col) {
                cv::putText(frameBGR, t, org + cv::Point(2,2), cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0,0,0), 3, cv::LINE_AA);
                cv::putText(frameBGR, t, org,                 cv::FONT_HERSHEY_SIMPLEX, 0.7, col,                 2, cv::LINE_AA);
            };
            putShadowText(genText,     {18, 38},  {40,255,40});
            putShadowText(fpsText,     {18, 68},  {40,255,40});
            putShadowText(avgTimeText, {18, 98},  {40,255,40});
            putShadowText(statusText,  {18,128},  paused ? cv::Scalar(0,0,255) : cv::Scalar(40,255,40));
            putShadowText(stepsText,   {18,158},  {40,255,40});
            std::string autoStepsText = g_autoSteps ? "Auto-Steps: ON" : "Auto-Steps: OFF";
            putShadowText(headlessText,{18,188},  g_headless ? cv::Scalar(0,0,255) : cv::Scalar(40,255,40));
            putShadowText(autoStepsText,{18,218},  g_autoSteps ? cv::Scalar(0,0,255) : cv::Scalar(40,255,40));

            // Display the frame
            cv::imshow(kWindow, frameBGR);
            // Store previous logical for birth/death coloring
            prevLogical = currLogical.clone();
        }
        
        // ======== HANDLE USER INPUT ========
        // cv::waitKey(1) allows smooth animation while checking for key presses
    int key = cv::waitKey(1); // still poll keys in headless mode
        
        if (key == 'q' || key == 27) {  // 'q' or ESC
            std::cout << "\nUser requested quit. Stopping simulation..." << std::endl;
            break;
        } else if (key == 'r') {  // Reset
            std::cout << "\nResetting grid..." << std::endl;
            generation = 0;
            totalComputeTime = 0.0;
            computeCount = 0;
            initializeGrid(h_grid.data(), GRID_WIDTH, GRID_HEIGHT, INITIAL_ALIVE_PROBABILITY);
            CUDA_CHECK(cudaMemcpy(d_input, h_grid.data(), GRID_SIZE * sizeof(uint8_t), 
                                 cudaMemcpyHostToDevice));
            prevLogical.release();
        } else if (key == ' ') {  // Space - pause/unpause
            paused = !paused;
            std::cout << (paused ? "Paused" : "Resumed") << " at generation " 
                     << generation << std::endl;
        } else if (key == 'g' || key == 'G') {
            g_showGrid = !g_showGrid;
        } else if (key == 'c' || key == 'C') {
            g_showBirthDeath = !g_showBirthDeath;
        } else if (key == '+' || key == '=') {
            g_scale = std::min(16, g_scale + 1); gridOverlay.release();
        } else if (key == '-' || key == '_') {
            g_scale = std::max(1, g_scale - 1);  gridOverlay.release();
        } else if (key == 'm' || key == 'M') {
            g_theme = static_cast<Theme>((static_cast<int>(g_theme) + 1) % 3);
        } else if (key == '[') {
            g_stepsPerFrame = std::max(1, g_stepsPerFrame - 1);
        } else if (key == ']') {
            g_stepsPerFrame = std::min(kMaxStepsPerFrame, g_stepsPerFrame + 1);
        } else if (key == 'h' || key == 'H') {
            g_headless = !g_headless;
            if (g_headless) {
                std::cout << "Headless mode ENABLED: maximizing GPU compute (no rendering)." << std::endl;
            } else {
                std::cout << "Headless mode DISABLED: resuming visualization." << std::endl;
            }
        } else if (key == 'a' || key == 'A') {
            g_autoSteps = !g_autoSteps;
            std::cout << (g_autoSteps ? "Adaptive steps ENABLED." : "Adaptive steps DISABLED.") << std::endl;
        }
        
        // Print progress every 100 generations
        // Record metrics sample (after handling input). Use frame duration now.
        double frameTotalMs = frameTimer.elapsed();
        if (!paused) {
            // compute time for this batch approximated as difference in totalComputeTime minus last sample
            double batchComputeMs = 0.0;
            if (g_sampleComputeMs.empty()) {
                batchComputeMs = totalComputeTime;
            } else {
                double sumPrev = 0.0; // previous cumulative compute time not stored separately; reconstruct by summing samples
                for (double v : g_sampleComputeMs) sumPrev += v;
                batchComputeMs = totalComputeTime - sumPrev;
            }
            double utilPercent = (frameTotalMs > 0.0) ? (batchComputeMs / frameTotalMs * 100.0) : 0.0;
            g_sampleComputeMs.push_back(batchComputeMs);
            g_sampleFrameMs.push_back(frameTotalMs);
            g_sampleUtilPercent.push_back(utilPercent);
            g_sampleGeneration.push_back(generation);
        }

        if (generation % 500 == 0 && generation > 0 && !paused) {
            double avgComputeTime = (computeCount > 0) ? (totalComputeTime / computeCount) : 0.0;
            double recentUtil = g_sampleUtilPercent.empty() ? 0.0 : g_sampleUtilPercent.back();
            std::cout << "Generation: " << std::setw(7) << generation
                      << " | Steps/Frame: " << std::setw(3) << g_stepsPerFrame
                      << " | Avg GPU Time/gen: " << std::fixed << std::setprecision(3)
                      << avgComputeTime << " ms"
                      << " | Last Util: " << std::fixed << std::setprecision(1) << recentUtil << "%";
            if (!g_headless) {
                std::cout << " | FPS: " << std::fixed << std::setprecision(1) << currentFPS;
            }
            std::cout << (g_headless ? " | HEADLESS" : "") << std::endl;
        }
    }
    
    // ======== CLEANUP AND FINAL STATISTICS ========
    double totalElapsed = totalTimer.elapsed();
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "Simulation Complete!" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Final Statistics:" << std::endl;
    std::cout << "  Total Generations: " << generation << std::endl;
    std::cout << "  Total Time: " << std::fixed << std::setprecision(2) 
              << totalElapsed << " ms" << std::endl;
    std::cout << "  Average GPU Compute Time: " << std::fixed << std::setprecision(3) 
              << (totalComputeTime / computeCount) << " ms/generation" << std::endl;
    std::cout << "  Overall FPS: " << std::fixed << std::setprecision(1) 
              << (generation * 1000.0 / totalElapsed) << std::endl;
    std::cout << "  GPU Efficiency: " << std::fixed << std::setprecision(1)
              << (totalComputeTime / totalElapsed * 100.0) << "%" << std::endl;
    std::cout << "========================================\n" << std::endl;

    // ======== SUMMARY PAGE (Timeline Graph) ========
    auto percentile = [](std::vector<double> v, double p) {
        if (v.empty()) return 0.0; std::sort(v.begin(), v.end());
        double idx = (p/100.0)*(v.size()-1); size_t i = static_cast<size_t>(idx);
        size_t j = std::min(v.size()-1, i+1); double frac = idx - i; return v[i] + (v[j]-v[i])*frac;
    };
    double avgMsGen = (computeCount>0)?(totalComputeTime/computeCount):0.0;
    double minMs = g_sampleComputeMs.empty()?0.0:*std::min_element(g_sampleComputeMs.begin(), g_sampleComputeMs.end());
    double maxMs = g_sampleComputeMs.empty()?0.0:*std::max_element(g_sampleComputeMs.begin(), g_sampleComputeMs.end());
    double p50 = percentile(g_sampleComputeMs,50);
    double p95 = percentile(g_sampleComputeMs,95);
    double avgUtil = 0.0; for (double u: g_sampleUtilPercent) avgUtil += u; if(!g_sampleUtilPercent.empty()) avgUtil/=g_sampleUtilPercent.size();

    // Build plot image
    int plotW = 1000; int plotH = 300; int margin = 50;
    cv::Mat plot(plotH+2*margin, plotW+2*margin, CV_8UC3, cv::Scalar(20,20,20));
    cv::putText(plot, "GPU Utilization Timeline", {margin,30}, cv::FONT_HERSHEY_SIMPLEX, 0.8, {0,255,0},2,cv::LINE_AA);
    // Axes
    cv::line(plot,{margin,margin},{margin,plotH+margin},{200,200,200},1);
    cv::line(plot,{margin,plotH+margin},{plotW+margin,plotH+margin},{200,200,200},1);
    cv::putText(plot,"0%", {5, plotH+margin}, cv::FONT_HERSHEY_PLAIN, 1.0, {200,200,200},1);
    cv::putText(plot,"100%", {5, margin+5}, cv::FONT_HERSHEY_PLAIN, 1.0, {200,200,200},1);
    // Plot utilization line
    if (!g_sampleUtilPercent.empty()) {
        double maxGen = g_sampleGeneration.back();
        for (size_t i=1;i<g_sampleUtilPercent.size();++i){
            double g0 = g_sampleGeneration[i-1]; double g1 = g_sampleGeneration[i];
            double u0 = g_sampleUtilPercent[i-1]; double u1 = g_sampleUtilPercent[i];
            int x0 = margin + static_cast<int>((g0/maxGen)*plotW);
            int x1 = margin + static_cast<int>((g1/maxGen)*plotW);
            int y0 = margin + static_cast<int>((100.0 - std::clamp(u0,0.0,100.0))/100.0 * plotH);
            int y1 = margin + static_cast<int>((100.0 - std::clamp(u1,0.0,100.0))/100.0 * plotH);
            cv::line(plot,{x0,y0},{x1,y1},{0,180,255},2,cv::LINE_AA);
        }
    }
    // Draw target band (35%-70%)
    int yLow = margin + static_cast<int>((100.0-70.0)/100.0 * plotH);
    int yHigh= margin + static_cast<int>((100.0-35.0)/100.0 * plotH);
    cv::rectangle(plot,{margin,yLow},{plotW+margin,yHigh},{40,80,40},-1);
    cv::addWeighted(plot,1.0,plot,0.0,0.0,plot); // placeholder to keep rectangle
    cv::putText(plot,"Target Util Band (35%-70%)", {plotW/2, yHigh-5}, cv::FONT_HERSHEY_PLAIN, 1.0, {150,255,150},1);

    // Stats panel
    cv::Mat summary(700, 1000, CV_8UC3, cv::Scalar(0,0,0));
    cv::putText(summary, "Simulation Summary", {20,40}, cv::FONT_HERSHEY_SIMPLEX, 1.2, {0,255,0},2,cv::LINE_AA);
    int y=80; auto line=[&](std::string t){ cv::putText(summary,t,{20,y},cv::FONT_HERSHEY_PLAIN,1.4,{255,255,255},1,cv::LINE_AA); y+=28;};
    line("Total Generations: " + std::to_string(generation));
    line("Total Time (ms): " + std::to_string(totalElapsed));
    line("Avg ms / Generation: " + std::to_string(avgMsGen));
    line("Min / Max ms (batch): " + std::to_string(minMs) + " / " + std::to_string(maxMs));
    line("p50 / p95 ms (batch): " + std::to_string(p50) + " / " + std::to_string(p95));
    line("Avg Utilization (%): " + std::to_string(avgUtil));
    line("Threads per Block: " + std::to_string(BLOCK_SIZE*BLOCK_SIZE));
    line("Blocks: " + std::to_string((GRID_WIDTH / BLOCK_SIZE)*(GRID_HEIGHT / BLOCK_SIZE)));
    double cellsPerSec = (generation * 1000.0) / totalElapsed * (double)(GRID_WIDTH*GRID_HEIGHT);
    line("Cell Updates / sec: " + std::to_string(static_cast<long long>(cellsPerSec)));
    line("Steps/Frame Final: " + std::to_string(g_stepsPerFrame));
    line(std::string("Adaptive Steps: ") + (g_autoSteps?"ENABLED":"DISABLED"));
    line(std::string("Headless During Run: ") + (g_headless?"YES":"NO"));
    // Efficiency defined as compute / total time
    double efficiencyPercent = (totalComputeTime / totalElapsed * 100.0);
    line("Overall GPU Efficiency (%): " + std::to_string(efficiencyPercent));
    line("Tip: Increase grid size or enable auto-steps for higher utilization.");
    line("Press any key to exit.");

    // Combine summary + plot vertically
    cv::Mat combined(summary.rows + plot.rows, std::max(summary.cols, plot.cols), CV_8UC3, cv::Scalar(0,0,0));
    summary.copyTo(combined(cv::Rect(0,0,summary.cols, summary.rows)));
    plot.copyTo(combined(cv::Rect(0,summary.rows, plot.cols, plot.rows)));
    cv::imshow("Game of Life - CUDA", combined);
    cv::waitKey(0);

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
║              Real-time OpenCV Visualization                ║
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
    std::cout << "  Grid Blocks: " << (GRID_WIDTH / BLOCK_SIZE) << " x " 
              << (GRID_HEIGHT / BLOCK_SIZE) << " (" 
              << (GRID_WIDTH / BLOCK_SIZE) * (GRID_HEIGHT / BLOCK_SIZE) << " blocks)" << std::endl;
    std::cout << "  Initial Alive Probability: " << (INITIAL_ALIVE_PROBABILITY * 100) << "%" << std::endl;
    std::cout << "  Display Window: " << WINDOW_WIDTH << " x " << WINDOW_HEIGHT << " pixels" << std::endl;
    
    // Parse command line arguments
    int maxGenerations = MAX_GENERATIONS;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--generations" || arg == "-g") {
            if (i + 1 < argc) {
                maxGenerations = std::stoi(argv[++i]);
            }
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "\nUsage: " << argv[0] << " [options]\n";
            std::cout << "Options:\n";
            std::cout << "  --generations, -g N    Run for N generations (default: " 
                     << MAX_GENERATIONS << ")\n";
            std::cout << "  --help, -h             Show this help message\n";
            std::cout << "\nControls (during visualization):\n";
            std::cout << "  'q' or ESC    Quit\n";
            std::cout << "  'r'           Reset grid\n";
            std::cout << "  SPACE         Pause/Resume\n";
            return 0;
        }
    }
    
    // Run real-time visualization
    runRealtimeVisualization(maxGenerations);
    
    std::cout << "Program completed successfully!" << std::endl;
    return 0;
}
