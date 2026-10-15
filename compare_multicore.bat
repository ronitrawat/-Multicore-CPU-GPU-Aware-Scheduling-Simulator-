@echo off
echo ============================================
echo Multicore Performance Comparison
echo ============================================
echo.
echo Testing 8 processes with different core configurations...
echo.

echo --- Single Core (1.0x) ---
build\scheduler.exe --algo priority --cores 1 --speeds 1.0 --random 8 --seed 42 | findstr /C:"Total Time" /C:"CPU Utilization" /C:"Average Waiting"
echo.

echo --- Dual Core (1.0x, 1.0x) ---
build\scheduler.exe --algo priority --cores 2 --speeds 1.0,1.0 --load-balancer global --random 8 --seed 42 | findstr /C:"Total Time" /C:"CPU Utilization" /C:"Average Waiting"
echo.

echo --- Quad Core (1.0x each) ---
build\scheduler.exe --algo priority --cores 4 --speeds 1.0,1.0,1.0,1.0 --load-balancer global --random 8 --seed 42 | findstr /C:"Total Time" /C:"CPU Utilization" /C:"Average Waiting"
echo.

echo --- Big.LITTLE (0.5x, 1.0x, 2.0x) ---
build\scheduler.exe --algo priority --cores 3 --speeds 0.5,1.0,2.0 --load-balancer speedaware --random 8 --seed 42 | findstr /C:"Total Time" /C:"CPU Utilization" /C:"Average Waiting"
echo.

echo --- Extreme Heterogeneous (0.2x, 1.0x, 3.0x) ---
build\scheduler.exe --algo priority --cores 3 --speeds 0.2,1.0,3.0 --load-balancer speedaware --random 8 --seed 42 | findstr /C:"Total Time" /C:"CPU Utilization" /C:"Average Waiting"
echo.

echo ============================================
echo Priority Scheduler Comparison
echo ============================================
echo.

echo --- Priority (No Aging) ---
build\scheduler.exe --algo priority --aging 0 --cores 2 --random 6 --seed 123 | findstr /C:"Average Waiting" /C:"Average Response"
echo.

echo --- Priority (Aging=5) ---
build\scheduler.exe --algo priority --aging 5 --cores 2 --random 6 --seed 123 | findstr /C:"Average Waiting" /C:"Average Response"
echo.

echo --- Priority Non-Preemptive ---
build\scheduler.exe --algo priority-np --aging 5 --cores 2 --random 6 --seed 123 | findstr /C:"Average Waiting" /C:"Average Response"
echo.

echo ============================================
echo Load Balancer Comparison
echo ============================================
echo.

echo --- Global Queue ---
build\scheduler.exe --algo rr --quantum 2 --cores 3 --speeds 0.5,1.0,2.0 --load-balancer global --random 8 --seed 456 | findstr /C:"Total Time" /C:"Average Waiting"
echo.

echo --- Per-Core Affinity ---
build\scheduler.exe --algo rr --quantum 2 --cores 3 --speeds 0.5,1.0,2.0 --load-balancer percore --random 8 --seed 456 | findstr /C:"Total Time" /C:"Average Waiting"
echo.

echo --- Speed-Aware ---
build\scheduler.exe --algo rr --quantum 2 --cores 3 --speeds 0.5,1.0,2.0 --load-balancer speedaware --random 8 --seed 456 | findstr /C:"Total Time" /C:"Average Waiting"
echo.

echo ============================================
echo Comparison Complete!
echo ============================================