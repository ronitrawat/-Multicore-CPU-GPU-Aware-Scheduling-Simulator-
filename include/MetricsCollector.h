#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <memory>

namespace scheduler {

class Process;
class Core;

struct ProcessMetrics {
    uint32_t pid;
    std::string name;
    uint32_t arrival_time;
    uint32_t completion_time;
    uint32_t turnaround_time;
    uint32_t waiting_time;
    uint32_t response_time;
    uint32_t burst_time;
    uint32_t migrations;
    int last_core;
    bool starved;
};

struct CoreMetrics {
    int core_id;
    double speed;
    uint32_t busy_time;
    uint32_t idle_time;
    uint32_t context_switches;
    uint32_t overhead_time;
    double utilization;  // busy / (busy + idle + overhead)
};

struct GlobalMetrics {
    uint32_t total_processes;
    uint32_t total_time;  // Makespan
    
    // Averages
    double avg_turnaround_time;
    double avg_waiting_time;
    double avg_response_time;
    
    // Min/Max
    uint32_t min_turnaround_time;
    uint32_t max_turnaround_time;
    uint32_t min_waiting_time;
    uint32_t max_waiting_time;
    uint32_t min_response_time;
    uint32_t max_response_time;
    
    // Percentiles (95th and 99th)
    double p95_turnaround_time;
    double p99_turnaround_time;
    double p95_waiting_time;
    double p99_waiting_time;
    double p95_response_time;
    double p99_response_time;
    
    // Fairness (Jain's fairness index over normalized turnaround time: TAT / Burst)
    double jains_fairness_index;
    
    // Starvation
    uint32_t starvation_threshold;
    uint32_t starved_process_count;
    
    // Multicore and overhead summary
    uint32_t total_context_switches;
    uint32_t total_migrations;
    uint32_t total_overhead_time;
    uint32_t total_busy_time;
    uint32_t total_idle_time;
    
    // Overall CPU utilization
    double cpu_utilization;
    
    // Throughput (processes per time unit)
    double throughput;
};

// Summary structure for comparative reporting
struct AlgorithmResult {
    std::string algorithm_name;
    GlobalMetrics metrics;
    std::vector<CoreMetrics> core_metrics;
};

class MetricsCollector {
public:
    explicit MetricsCollector(uint32_t starvation_threshold = 0);
    
    void set_starvation_threshold(uint32_t threshold) { starvation_threshold_ = threshold; }
    
    void collect_process(const Process& proc);
    void collect_cores(const std::vector<Core>& cores);
    
    void compute_global_metrics(uint32_t total_time);
    
    // Accessors
    const std::vector<ProcessMetrics>& process_metrics() const { return process_metrics_; }
    const std::vector<CoreMetrics>& core_metrics() const { return core_metrics_; }
    const GlobalMetrics& global_metrics() const { return global_metrics_; }
    
    // Report generation
    void print_summary() const;
    void print_detailed() const;
    static void print_comparison_table(const std::vector<AlgorithmResult>& results);
    
    // Export functionality
    void export_to_csv(const std::string& filename) const;
    void export_to_json(const std::string& filename, const std::string& algo_name = "") const;
    
    static void export_comparison_csv(const std::vector<AlgorithmResult>& results, const std::string& filename);
    static void export_comparison_json(const std::vector<AlgorithmResult>& results, const std::string& filename);

private:
    uint32_t starvation_threshold_;
    std::vector<ProcessMetrics> process_metrics_;
    std::vector<CoreMetrics> core_metrics_;
    GlobalMetrics global_metrics_;
    
    static double compute_percentile(std::vector<double>& sorted_vals, double p);
};

} // namespace scheduler
