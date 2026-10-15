#pragma once

#include <vector>
#include <string>
#include <memory>
#include <functional>
#include "MetricsCollector.h"
#include "WorkloadLoader.h"

namespace scheduler {

struct ParallelSimulationJob {
    std::string algorithm_name;
    uint32_t quantum = 4;
    uint32_t aging_interval = 10;
    std::vector<uint32_t> mlfq_quanta = {2, 4, 8};
    uint32_t mlfq_boost = 50;
    
    int num_cores = 1;
    std::vector<double> core_speeds;
    std::string load_balancer_type = "global";
    uint32_t migration_penalty = 1;
    uint32_t ctx_switch_cost = 0;
};

struct ParallelRunResults {
    std::vector<AlgorithmResult> results;
    double wall_clock_seconds = 0.0;
    double serial_estimated_seconds = 0.0;
    double speedup = 1.0;
    size_t threads_used = 1;
};

class ParallelSimulationEngine {
public:
    explicit ParallelSimulationEngine(size_t max_threads = 0);
    
    ParallelRunResults run_batch(const std::vector<std::unique_ptr<Process>>& base_workload,
                                const std::vector<ParallelSimulationJob>& jobs);
                                
    static void print_parallel_report(const ParallelRunResults& results);

private:
    size_t max_threads_;
};

} // namespace scheduler
