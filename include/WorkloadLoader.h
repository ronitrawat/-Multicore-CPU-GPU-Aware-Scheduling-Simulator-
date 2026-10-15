#pragma once

#include <vector>
#include <string>
#include <memory>
#include "Process.h"

namespace scheduler {

enum class WorkloadDistribution {
    UNIFORM,
    EXPONENTIAL,
    BIMODAL
};

class WorkloadLoader {
public:
    // Load processes from CSV file
    // Expected format: pid,name,arrival_time,burst_time,priority
    static std::vector<std::unique_ptr<Process>> load_from_csv(const std::string& filename);
    
    // Generate random workload
    struct GeneratorConfig {
        uint32_t num_processes = 20;
        uint32_t seed = 42;
        uint32_t max_arrival_time = 50;
        uint32_t min_burst_time = 1;
        uint32_t max_burst_time = 15;
        bool use_priorities = false;
        WorkloadDistribution distribution = WorkloadDistribution::UNIFORM;
        
        // Distribution parameters
        double arrival_lambda = 0.2;       // Mean inter-arrival = 5 ticks
        double burst_lambda = 0.15;        // Mean burst ~ 6.7 ticks
        double bimodal_ratio = 0.75;       // 75% short (IO-heavy), 25% long (CPU-heavy)
        uint32_t bimodal_short_max = 3;    // Short jobs burst: 1-3
        uint32_t bimodal_long_min = 15;    // Long jobs burst: 15-30
        uint32_t bimodal_long_max = 30;
    };
    
    static std::vector<std::unique_ptr<Process>> generate_random(const GeneratorConfig& config);
    
    // Deep clone a workload (critical for running multiple algorithms on the exact same processes)
    static std::vector<std::unique_ptr<Process>> clone_workload(
        const std::vector<std::unique_ptr<Process>>& source);
    
    // Save workload to CSV (for reproducibility)
    static void save_to_csv(const std::vector<std::unique_ptr<Process>>& processes,
                           const std::string& filename);
                           
    static WorkloadDistribution parse_distribution(const std::string& name);
    static std::string distribution_to_string(WorkloadDistribution dist);
};

} // namespace scheduler
