cmake_minimum_required(VERSION 3.14)
project(CPUScheduler VERSION 1.0 LANGUAGES CXX)

# C++17 required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# Sanitizers for debug builds (non-MSVC)
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT MSVC)
    add_compile_options(-fsanitize=address,undefined)
    add_link_options(-fsanitize=address,undefined)
endif()

# Project include directory
include_directories(${PROJECT_SOURCE_DIR}/include)

# Source files
set(SOURCES
    src/Process.cpp
    src/Core.cpp
    src/FCFSScheduler.cpp
    src/SJFScheduler.cpp
    src/SRTFScheduler.cpp
    src/RoundRobinScheduler.cpp
    src/PriorityScheduler.cpp
    src/MLFQScheduler.cpp
    src/SimulationEngine.cpp
    src/MetricsCollector.cpp
    src/WorkloadLoader.cpp
    src/GanttChart.cpp
    src/LoadBalancer.cpp
    src/PowerModel.cpp
    src/ParallelEngine.cpp
    src/BenchmarkHarness.cpp
    src/gpu/WarpScheduler.cpp
    src/gpu/CudaStreamGraph.cpp
)

# Main executable
add_executable(scheduler src/main.cpp ${SOURCES})

# Enable testing
enable_testing()
add_subdirectory(tests)
