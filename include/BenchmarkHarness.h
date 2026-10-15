#pragma once

#include <vector>
#include <string>
#include <cstdint>

namespace scheduler {

struct BenchmarkThroughputResult {
    uint32_t process_count;
    double elapsed_seconds;
    double processes_per_second;
    uint32_t simulated_makespan;
};

struct QuantumSweepResult {
    uint32_t quantum;
    uint32_t makespan;
    double avg_turnaround;
    double avg_waiting;
    double avg_response;
    uint32_t context_switches;
};

struct CoreScalingResult {
    int num_cores;
    uint32_t makespan;
    double speedup;
    double cpu_utilization;
};

class BenchmarkHarness {
public:
    static std::vector<BenchmarkThroughputResult> run_scalability_sweep(
        const std::vector<uint32_t>& counts = {100, 1000, 10000, 50000});
        
    static std::vector<QuantumSweepResult> run_quantum_sweep(
        uint32_t num_processes = 50,
        const std::vector<uint32_t>& quanta = {1, 2, 4, 8, 16, 32});
        
    static std::vector<CoreScalingResult> run_core_scaling_sweep(
        uint32_t num_processes = 100,
        const std::vector<int>& core_counts = {1, 2, 4, 8, 16});
        
    static void print_benchmarks();
};

} // namespace scheduler
