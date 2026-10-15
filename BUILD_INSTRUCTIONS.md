# Build Instructions

## Quick Build (Manual Steps)

### Step 1: Configure CMake

Open PowerShell and navigate to the project directory:

```powershell
cd "c:\Users\ronit\Desktop\cpu scheduler"
```

If you have MSVC (Visual Studio):
```powershell
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
```

If you have g++ (MSYS2/MinGW):
```powershell
# First, find your paths
where.exe g++
where.exe make

# Then configure with explicit paths (replace with your actual paths)
cmake -B build -S . -G "Unix Makefiles" `
  -DCMAKE_CXX_COMPILER="C:/msys64/ucrt64/bin/g++.exe" `
  -DCMAKE_C_COMPILER="C:/msys64/ucrt64/bin/gcc.exe" `
  -DCMAKE_MAKE_PROGRAM="C:/msys64/usr/bin/make.exe" `
  -DCMAKE_BUILD_TYPE=Release
```

### Step 2: Build

```powershell
cmake --build build --config Release
```

### Step 3: Run

```powershell
# Run with default example
.\build\scheduler.exe

# Run tests
.\build\tests\run_tests.exe
```

## Alternative: Use MSYS2 Shell Directly

If CMake configuration is failing in PowerShell, you can use the MSYS2 terminal directly:

1. Open MSYS2 MinGW 64-bit terminal
2. Navigate to project:
   ```bash
   cd "/c/Users/ronit/Desktop/cpu scheduler"
   ```
3. Build:
   ```bash
   cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
   cmake --build build
   ```
4. Run:
   ```bash
   ./build/scheduler.exe
   ./build/tests/run_tests.exe
   ```

## Troubleshooting

### "CMake not found"
Install CMake from https://cmake.org/download/

### "No compiler found"
Install one of:
- **MSVC**: Install Visual Studio 2019+ with C++ development tools
- **MinGW/MSYS2**: Install from https://www.msys2.org/
  ```bash
  pacman -S mingw-w64-ucrt-x86_64-gcc
  pacman -S mingw-w64-ucrt-x86_64-cmake
  pacman -S make
  ```

### "Generator mismatch"
Clean the build directory:
```powershell
Remove-Item -Recurse -Force build
```
Then reconfigure.

## Manual Compilation (No CMake)

If CMake is not working, you can compile manually:

```powershell
# Create build directory
mkdir build

# Compile
g++ -std=c++17 -I include `
  src/Process.cpp `
  src/Core.cpp `
  src/FCFSScheduler.cpp `
  src/SimulationEngine.cpp `
  src/MetricsCollector.cpp `
  src/WorkloadLoader.cpp `
  src/main.cpp `
  -o build/scheduler.exe
```

Then run:
```powershell
.\build\scheduler.exe --workload workloads\example.csv
```
