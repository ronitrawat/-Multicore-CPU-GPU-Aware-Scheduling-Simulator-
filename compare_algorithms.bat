@echo off
echo ============================================
echo CPU Scheduler Algorithm Comparison
echo ============================================
echo.
echo Testing workload: workloads\textbook_sjf.csv
echo.

echo --- FCFS (First-Come First-Served) ---
build\scheduler.exe --algo fcfs --workload workloads\textbook_sjf.csv | findstr /C:"Average Waiting" /C:"Average Response"
echo.

echo --- SJF (Shortest Job First) ---
build\scheduler.exe --algo sjf --workload workloads\textbook_sjf.csv | findstr /C:"Average Waiting" /C:"Average Response"
echo.

echo --- SRTF (Shortest Remaining Time First) ---
build\scheduler.exe --algo srtf --workload workloads\textbook_sjf.csv | findstr /C:"Average Waiting" /C:"Average Response"
echo.

echo --- Round Robin (q=2) ---
build\scheduler.exe --algo rr --quantum 2 --workload workloads\textbook_sjf.csv | findstr /C:"Average Waiting" /C:"Average Response"
echo.

echo --- Round Robin (q=4) ---
build\scheduler.exe --algo rr --quantum 4 --workload workloads\textbook_sjf.csv | findstr /C:"Average Waiting" /C:"Average Response"
echo.

echo ============================================
echo Comparison Complete!
echo ============================================
