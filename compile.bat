@echo off
echo Compiling CPU Scheduler Simulator...
echo.

if not exist build mkdir build

C:\msys64\ucrt64\bin\g++.exe -std=c++17 -I include -Wall -Wextra -O2 ^
  src/Process.cpp ^
  src/Core.cpp ^
  src/FCFSScheduler.cpp ^
  src/SJFScheduler.cpp ^
  src/SRTFScheduler.cpp ^
  src/RoundRobinScheduler.cpp ^
  src/PriorityScheduler.cpp ^
  src/MLFQScheduler.cpp ^
  src/SimulationEngine.cpp ^
  src/MetricsCollector.cpp ^
  src/WorkloadLoader.cpp ^
  src/GanttChart.cpp ^
  src/LoadBalancer.cpp ^
  src/PowerModel.cpp ^
  src/ParallelEngine.cpp ^
  src/BenchmarkHarness.cpp ^
  src/gpu/WarpScheduler.cpp ^
  src/gpu/CudaStreamGraph.cpp ^
  src/main.cpp ^
  -o build\scheduler.exe

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Build successful!
    echo Executable: build\scheduler.exe
    echo.
    echo Run with: build\scheduler.exe --help
) else (
    echo.
    echo Build failed!
)
