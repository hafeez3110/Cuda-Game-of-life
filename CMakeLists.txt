cmake_minimum_required(VERSION 3.18)
project(GameOfLife LANGUAGES CXX CUDA)

# Set C++ and CUDA standards
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

# Set CUDA architecture for RTX 4060 (Ada Lovelace - compute capability 8.9)
# Also include 8.6 for broader compatibility
set(CMAKE_CUDA_ARCHITECTURES 86 89)

# Find OpenCV
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

# CUDA flags for optimization
set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -O3 -use_fast_math")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")

# Add executable
add_executable(gameoflife
    main.cpp
    gameoflife.cu
)

# Link libraries
target_link_libraries(gameoflife ${OpenCV_LIBS})

# Set CUDA separable compilation
set_property(TARGET gameoflife PROPERTY CUDA_SEPARABLE_COMPILATION ON)

# Platform-specific settings
if(WIN32)
    # Windows-specific settings
    set_property(TARGET gameoflife PROPERTY CUDA_RESOLVE_DEVICE_SYMBOLS ON)
endif()

# Print configuration info
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenCV libraries: ${OpenCV_LIBS}")
message(STATUS "CUDA architectures: ${CMAKE_CUDA_ARCHITECTURES}")
