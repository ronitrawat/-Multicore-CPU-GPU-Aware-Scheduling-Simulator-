# Build script for CPU Scheduler Simulator
# Automatically detects compiler and builds the project

Write-Host "CPU Scheduler Simulator - Build Script" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

# Clean old build
if (Test-Path "build") {
    Write-Host "Cleaning old build..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
}

# Try to find CMake
$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmake) {
    Write-Host "ERROR: CMake not found. Please install CMake." -ForegroundColor Red
    exit 1
}

# Try to find compiler
$compiler = $null
$generator = $null

# Check for MSVC
$cl = Get-Command cl -ErrorAction SilentlyContinue
if ($cl) {
    Write-Host "Found MSVC compiler" -ForegroundColor Green
    $generator = "NMake Makefiles"
}

# Check for g++ (MSYS2/MinGW)
if (-not $compiler) {
    $gpp = Get-Command g++ -ErrorAction SilentlyContinue
    if ($gpp) {
        Write-Host "Found g++ compiler at: $($gpp.Source)" -ForegroundColor Green
        $compiler = $gpp.Source
        
        # Find make
        $make = Get-Command make -ErrorAction SilentlyContinue
        if ($make) {
            $generator = "Unix Makefiles"
            $makeProgram = $make.Source
        }
    }
}

if (-not $compiler -and -not $cl) {
    Write-Host "ERROR: No C++ compiler found (cl.exe or g++.exe)" -ForegroundColor Red
    Write-Host "Please install MSVC (Visual Studio) or MSYS2/MinGW" -ForegroundColor Yellow
    exit 1
}

# Configure
Write-Host "`nConfiguring project..." -ForegroundColor Cyan
if ($generator -eq "Unix Makefiles") {
    $gccPath = (Get-Command gcc -ErrorAction SilentlyContinue).Source
    cmake -B build -S . -G "Unix Makefiles" `
        -DCMAKE_CXX_COMPILER="$compiler" `
        -DCMAKE_C_COMPILER="$gccPath" `
        -DCMAKE_MAKE_PROGRAM="$makeProgram" `
        -DCMAKE_BUILD_TYPE=Release
} else {
    cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
}

if ($LASTEXITCODE -ne 0) {
    Write-Host "`nERROR: CMake configuration failed" -ForegroundColor Red
    exit 1
}

# Build
Write-Host "`nBuilding project..." -ForegroundColor Cyan
cmake --build build --config Release

if ($LASTEXITCODE -ne 0) {
    Write-Host "`nERROR: Build failed" -ForegroundColor Red
    exit 1
}

Write-Host "`n========================================" -ForegroundColor Green
Write-Host "Build completed successfully!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host "`nExecutables:"
Write-Host "  - build\scheduler.exe (main simulator)" -ForegroundColor Yellow
Write-Host "  - build\tests\run_tests.exe (unit tests)" -ForegroundColor Yellow
Write-Host "`nRun tests with: .\build\tests\run_tests.exe" -ForegroundColor Cyan
Write-Host "Run simulator with: .\build\scheduler.exe --help" -ForegroundColor Cyan
