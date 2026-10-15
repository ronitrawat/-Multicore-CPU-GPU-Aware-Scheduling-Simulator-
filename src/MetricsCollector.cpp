#include "MetricsCollector.h"
#include "Process.h"
#include "Core.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <cmath>

namespace scheduler {

MetricsCollector::MetricsCollector(uint32_t starvation_threshold)
    : starvation_threshold_(starvation_threshold) {
    global_metrics_ = {};
}

void MetricsCollector::collect_process(const Process& proc) {
    ProcessMetrics pm;
    pm.pid = proc.pid;
    pm.name = proc.name;
    pm.arrival_time = proc.arrival_time;
    pm.completion_time = proc.completion_time;
    pm.turnaround_time = proc.turnaround_time();
    pm.burst_time = proc.burst_time;
    pm.waiting_time = proc.waiting_time;
    pm.response_time = proc.response_time();
    pm.migrations = proc.migration_count;
    pm.last_core = proc.last_core_id;
    pm.starved = (starvation_threshold_ > 0 && proc.waiting_time >= starvation_threshold_);
    
    process_metrics_.push_back(pm);
}

void MetricsCollector::collect_cores(const std::vector<Core>& cores) {
    core_metrics_.clear();
    for (const auto& core : cores) {
        CoreMetrics cm;
        cm.core_id = core.id();
        cm.speed = core.speed();
        cm.busy_time = core.busy_time();
        cm.idle_time = core.idle_time();
        cm.context_switches = core.context_switch_count();
        cm.overhead_time = core.context_switch_overhead_time();
        
        uint32_t total = cm.busy_time + cm.idle_time + cm.overhead_time;
        cm.utilization = total > 0 ? static_cast<double>(cm.busy_time) / total : 0.0;
        
        core_metrics_.push_back(cm);
    }
}

double MetricsCollector::compute_percentile(std::vector<double>& sorted_vals, double p) {
    if (sorted_vals.empty()) return 0.0;
    if (sorted_vals.size() == 1) return sorted_vals[0];
    
    double rank = p * (sorted_vals.size() - 1);
    size_t low = static_cast<size_t>(std::floor(rank));
    size_t high = static_cast<size_t>(std::ceil(rank));
    double weight = rank - low;
    
    return sorted_vals[low] * (1.0 - weight) + sorted_vals[high] * weight;
}

void MetricsCollector::compute_global_metrics(uint32_t total_time) {
    global_metrics_ = {};
    global_metrics_.total_processes = static_cast<uint32_t>(process_metrics_.size());
    global_metrics_.starvation_threshold = starvation_threshold_;
    
    // Determine makespan
    uint32_t max_completion = 0;
    for (const auto& pm : process_metrics_) {
        max_completion = std::max(max_completion, pm.completion_time);
    }
    global_metrics_.total_time = std::max(total_time, max_completion);
    
    if (process_metrics_.empty()) {
        return;
    }
    
    double sum_turnaround = 0, sum_waiting = 0, sum_response = 0;
    uint32_t min_tat = UINT32_MAX, max_tat = 0;
    uint32_t min_wait = UINT32_MAX, max_wait = 0;
    uint32_t min_resp = UINT32_MAX, max_resp = 0;
    uint32_t total_mig = 0;
    uint32_t starved_count = 0;
    
    std::vector<double> tat_vals;
    std::vector<double> wait_vals;
    std::vector<double> resp_vals;
    std::vector<double> norm_tat_vals;
    
    for (const auto& pm : process_metrics_) {
        sum_turnaround += pm.turnaround_time;
        sum_waiting += pm.waiting_time;
        sum_response += pm.response_time;
        total_mig += pm.migrations;
        if (pm.starved) starved_count++;
        
        min_tat = std::min(min_tat, pm.turnaround_time);
        max_tat = std::max(max_tat, pm.turnaround_time);
        min_wait = std::min(min_wait, pm.waiting_time);
        max_wait = std::max(max_wait, pm.waiting_time);
        min_resp = std::min(min_resp, pm.response_time);
        max_resp = std::max(max_resp, pm.response_time);
        
        tat_vals.push_back(static_cast<double>(pm.turnaround_time));
        wait_vals.push_back(static_cast<double>(pm.waiting_time));
        resp_vals.push_back(static_cast<double>(pm.response_time));
        
        double norm_tat = pm.burst_time > 0 
            ? static_cast<double>(pm.turnaround_time) / pm.burst_time 
            : 1.0;
        norm_tat_vals.push_back(norm_tat);
    }
    
    uint32_t n = global_metrics_.total_processes;
    global_metrics_.avg_turnaround_time = sum_turnaround / n;
    global_metrics_.avg_waiting_time = sum_waiting / n;
    global_metrics_.avg_response_time = sum_response / n;
    global_metrics_.min_turnaround_time = min_tat;
    global_metrics_.max_turnaround_time = max_tat;
    global_metrics_.min_waiting_time = min_wait;
    global_metrics_.max_waiting_time = max_wait;
    global_metrics_.min_response_time = min_resp;
    global_metrics_.max_response_time = max_resp;
    global_metrics_.total_migrations = total_mig;
    global_metrics_.starved_process_count = starved_count;
    
    // Percentiles
    std::sort(tat_vals.begin(), tat_vals.end());
    std::sort(wait_vals.begin(), wait_vals.end());
    std::sort(resp_vals.begin(), resp_vals.end());
    
    global_metrics_.p95_turnaround_time = compute_percentile(tat_vals, 0.95);
    global_metrics_.p99_turnaround_time = compute_percentile(tat_vals, 0.99);
    global_metrics_.p95_waiting_time = compute_percentile(wait_vals, 0.95);
    global_metrics_.p99_waiting_time = compute_percentile(wait_vals, 0.99);
    global_metrics_.p95_response_time = compute_percentile(resp_vals, 0.95);
    global_metrics_.p99_response_time = compute_percentile(resp_vals, 0.99);
    
    // Jain's Fairness Index: (sum(x_i))^2 / (n * sum(x_i^2))
    double sum_x = 0.0;
    double sum_sq_x = 0.0;
    for (double x : norm_tat_vals) {
        sum_x += x;
        sum_sq_x += (x * x);
    }
    if (sum_sq_x > 0.0 && n > 0) {
        global_metrics_.jains_fairness_index = (sum_x * sum_x) / (static_cast<double>(n) * sum_sq_x);
    } else {
        global_metrics_.jains_fairness_index = 1.0;
    }
    
    // Core aggregates
    uint32_t total_busy = 0, total_idle = 0, total_cs = 0, total_ovh = 0;
    for (const auto& cm : core_metrics_) {
        total_busy += cm.busy_time;
        total_idle += cm.idle_time;
        total_cs += cm.context_switches;
        total_ovh += cm.overhead_time;
    }
    global_metrics_.total_busy_time = total_busy;
    global_metrics_.total_idle_time = total_idle;
    global_metrics_.total_context_switches = total_cs;
    global_metrics_.total_overhead_time = total_ovh;
    
    uint32_t total_possible = total_busy + total_idle + total_ovh;
    global_metrics_.cpu_utilization = total_possible > 0
        ? static_cast<double>(total_busy) / total_possible
        : 0.0;
    
    global_metrics_.throughput = global_metrics_.total_time > 0 
        ? static_cast<double>(n) / global_metrics_.total_time 
        : 0.0;
}

void MetricsCollector::print_summary() const {
    std::cout << "\n================ Scheduling Results Summary ================\n\n";
    
    std::cout << "Workload Processes    : " << global_metrics_.total_processes << "\n";
    std::cout << "Makespan (Total Time) : " << global_metrics_.total_time << " ticks\n";
    std::cout << "Throughput            : " << std::fixed << std::setprecision(4) 
              << global_metrics_.throughput << " processes/tick\n";
    std::cout << "CPU Utilization       : " << std::fixed << std::setprecision(2) 
              << (global_metrics_.cpu_utilization * 100.0) << "%\n";
    std::cout << "Useful Work / Overhead: " << global_metrics_.total_busy_time 
              << " / " << global_metrics_.total_overhead_time << " ticks\n";
    std::cout << "Context Switches      : " << global_metrics_.total_context_switches << "\n";
    std::cout << "Migrations            : " << global_metrics_.total_migrations << "\n";
    std::cout << "Jain's Fairness Index : " << std::fixed << std::setprecision(4) 
              << global_metrics_.jains_fairness_index << " (1.0 = optimal)\n\n";
    
    std::cout << "--- Turnaround Time (ticks) ---\n";
    std::cout << "  Avg: " << std::setw(7) << std::fixed << std::setprecision(2) << global_metrics_.avg_turnaround_time
              << " | Min: " << std::setw(4) << global_metrics_.min_turnaround_time
              << " | Max: " << std::setw(4) << global_metrics_.max_turnaround_time
              << " | P95: " << std::setw(6) << global_metrics_.p95_turnaround_time
              << " | P99: " << std::setw(6) << global_metrics_.p99_turnaround_time << "\n";
              
    std::cout << "--- Waiting Time (ticks) ---\n";
    std::cout << "  Avg: " << std::setw(7) << std::fixed << std::setprecision(2) << global_metrics_.avg_waiting_time
              << " | Min: " << std::setw(4) << global_metrics_.min_waiting_time
              << " | Max: " << std::setw(4) << global_metrics_.max_waiting_time
              << " | P95: " << std::setw(6) << global_metrics_.p95_waiting_time
              << " | P99: " << std::setw(6) << global_metrics_.p99_waiting_time << "\n";
              
    std::cout << "--- Response Time (ticks) ---\n";
    std::cout << "  Avg: " << std::setw(7) << std::fixed << std::setprecision(2) << global_metrics_.avg_response_time
              << " | Min: " << std::setw(4) << global_metrics_.min_response_time
              << " | Max: " << std::setw(4) << global_metrics_.max_response_time
              << " | P95: " << std::setw(6) << global_metrics_.p95_response_time
              << " | P99: " << std::setw(6) << global_metrics_.p99_response_time << "\n\n";

    if (starvation_threshold_ > 0) {
        std::cout << "Starvation Threshold  : " << starvation_threshold_ << " ticks\n";
        std::cout << "Starved Processes     : " << global_metrics_.starved_process_count << " / "
                  << global_metrics_.total_processes << "\n";
    }
}

void MetricsCollector::print_detailed() const {
    std::cout << "\n==================== Per-Process Metrics ====================\n\n";
    std::cout << std::setw(5) << "PID" 
              << std::setw(10) << "Name"
              << std::setw(8) << "Arrival"
              << std::setw(8) << "Burst"
              << std::setw(12) << "Completion"
              << std::setw(10) << "Turnaround"
              << std::setw(9) << "Waiting"
              << std::setw(9) << "Response"
              << std::setw(8) << "Migr"
              << std::setw(9) << "Starved" << "\n";
    std::cout << std::string(88, '-') << "\n";
    
    for (const auto& pm : process_metrics_) {
        std::cout << std::setw(5) << pm.pid
                  << std::setw(10) << pm.name
                  << std::setw(8) << pm.arrival_time
                  << std::setw(8) << pm.burst_time
                  << std::setw(12) << pm.completion_time
                  << std::setw(10) << pm.turnaround_time
                  << std::setw(9) << pm.waiting_time
                  << std::setw(9) << pm.response_time
                  << std::setw(8) << pm.migrations
                  << std::setw(9) << (pm.starved ? "YES" : "No") << "\n";
    }
    
    std::cout << "\n===================== Per-Core Metrics =====================\n\n";
    std::cout << std::setw(8) << "Core ID"
              << std::setw(10) << "Speed"
              << std::setw(12) << "Busy Time"
              << std::setw(12) << "Idle Time"
              << std::setw(12) << "Overhead"
              << std::setw(12) << "Util %"
              << std::setw(10) << "Ctx Sw" << "\n";
    std::cout << std::string(76, '-') << "\n";
    
    for (const auto& cm : core_metrics_) {
        std::cout << std::setw(8) << cm.core_id
                  << std::setw(10) << std::fixed << std::setprecision(2) << cm.speed
                  << std::setw(12) << cm.busy_time
                  << std::setw(12) << cm.idle_time
                  << std::setw(12) << cm.overhead_time
                  << std::setw(12) << std::fixed << std::setprecision(1) << (cm.utilization * 100)
                  << std::setw(10) << cm.context_switches << "\n";
    }
}

void MetricsCollector::print_comparison_table(const std::vector<AlgorithmResult>& results) {
    std::cout << "\n=========================================== ALGORITHM COMPARISON MATRIX ===========================================\n\n";
    std::cout << std::left << std::setw(16) << "Algorithm"
              << std::right << std::setw(9) << "Makespan"
              << std::setw(11) << "Throughput"
              << std::setw(9) << "Avg TAT"
              << std::setw(9) << "P95 TAT"
              << std::setw(9) << "Avg Wait"
              << std::setw(9) << "P95 Wait"
              << std::setw(9) << "Avg Resp"
              << std::setw(10) << "CPU Util"
              << std::setw(10) << "Fairness"
              << std::setw(8) << "CtxSw"
              << std::setw(7) << "Migr" << "\n";
    std::cout << std::string(116, '-') << "\n";
    
    for (const auto& res : results) {
        const auto& m = res.metrics;
        std::cout << std::left << std::setw(16) << res.algorithm_name
                  << std::right << std::setw(9) << m.total_time
                  << std::setw(11) << std::fixed << std::setprecision(4) << m.throughput
                  << std::setw(9) << std::fixed << std::setprecision(2) << m.avg_turnaround_time
                  << std::setw(9) << std::fixed << std::setprecision(2) << m.p95_turnaround_time
                  << std::setw(9) << std::fixed << std::setprecision(2) << m.avg_waiting_time
                  << std::setw(9) << std::fixed << std::setprecision(2) << m.p95_waiting_time
                  << std::setw(9) << std::fixed << std::setprecision(2) << m.avg_response_time
                  << std::setw(9) << std::fixed << std::setprecision(1) << (m.cpu_utilization * 100.0) << "%"
                  << std::setw(10) << std::fixed << std::setprecision(4) << m.jains_fairness_index
                  << std::setw(8) << m.total_context_switches
                  << std::setw(7) << m.total_migrations << "\n";
    }
    std::cout << std::string(116, '-') << "\n\n";
}

void MetricsCollector::export_to_csv(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for CSV export: " + filename);
    }
    
    // Per-process metrics table
    file << "# Process Metrics\n";
    file << "pid,name,arrival_time,burst_time,completion_time,turnaround_time,waiting_time,response_time,migrations,starved\n";
    for (const auto& pm : process_metrics_) {
        file << pm.pid << ","
             << pm.name << ","
             << pm.arrival_time << ","
             << pm.burst_time << ","
             << pm.completion_time << ","
             << pm.turnaround_time << ","
             << pm.waiting_time << ","
             << pm.response_time << ","
             << pm.migrations << ","
             << (pm.starved ? 1 : 0) << "\n";
    }
    
    // Core metrics
    file << "\n# Core Metrics\n";
    file << "core_id,speed,busy_time,idle_time,overhead_time,utilization,context_switches\n";
    for (const auto& cm : core_metrics_) {
        file << cm.core_id << ","
             << cm.speed << ","
             << cm.busy_time << ","
             << cm.idle_time << ","
             << cm.overhead_time << ","
             << cm.utilization << ","
             << cm.context_switches << "\n";
    }
    
    // Global summary
    file << "\n# Summary Metrics\n";
    file << "metric,value\n";
    file << "total_processes," << global_metrics_.total_processes << "\n";
    file << "makespan," << global_metrics_.total_time << "\n";
    file << "throughput," << global_metrics_.throughput << "\n";
    file << "cpu_utilization," << global_metrics_.cpu_utilization << "\n";
    file << "avg_turnaround," << global_metrics_.avg_turnaround_time << "\n";
    file << "p95_turnaround," << global_metrics_.p95_turnaround_time << "\n";
    file << "p99_turnaround," << global_metrics_.p99_turnaround_time << "\n";
    file << "avg_waiting," << global_metrics_.avg_waiting_time << "\n";
    file << "p95_waiting," << global_metrics_.p95_waiting_time << "\n";
    file << "p99_waiting," << global_metrics_.p99_waiting_time << "\n";
    file << "avg_response," << global_metrics_.avg_response_time << "\n";
    file << "p95_response," << global_metrics_.p95_response_time << "\n";
    file << "p99_response," << global_metrics_.p99_response_time << "\n";
    file << "jains_fairness," << global_metrics_.jains_fairness_index << "\n";
    file << "context_switches," << global_metrics_.total_context_switches << "\n";
    file << "migrations," << global_metrics_.total_migrations << "\n";
    file << "starved_count," << global_metrics_.starved_process_count << "\n";
}

void MetricsCollector::export_to_json(const std::string& filename, const std::string& algo_name) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for JSON export: " + filename);
    }
    
    file << "{\n";
    file << "  \"algorithm\": \"" << algo_name << "\",\n";
    file << "  \"summary\": {\n";
    file << "    \"total_processes\": " << global_metrics_.total_processes << ",\n";
    file << "    \"makespan\": " << global_metrics_.total_time << ",\n";
    file << "    \"throughput\": " << global_metrics_.throughput << ",\n";
    file << "    \"cpu_utilization\": " << global_metrics_.cpu_utilization << ",\n";
    file << "    \"avg_turnaround_time\": " << global_metrics_.avg_turnaround_time << ",\n";
    file << "    \"p95_turnaround_time\": " << global_metrics_.p95_turnaround_time << ",\n";
    file << "    \"p99_turnaround_time\": " << global_metrics_.p99_turnaround_time << ",\n";
    file << "    \"avg_waiting_time\": " << global_metrics_.avg_waiting_time << ",\n";
    file << "    \"p95_waiting_time\": " << global_metrics_.p95_waiting_time << ",\n";
    file << "    \"p99_waiting_time\": " << global_metrics_.p99_waiting_time << ",\n";
    file << "    \"avg_response_time\": " << global_metrics_.avg_response_time << ",\n";
    file << "    \"p95_response_time\": " << global_metrics_.p95_response_time << ",\n";
    file << "    \"p99_response_time\": " << global_metrics_.p99_response_time << ",\n";
    file << "    \"jains_fairness_index\": " << global_metrics_.jains_fairness_index << ",\n";
    file << "    \"total_context_switches\": " << global_metrics_.total_context_switches << ",\n";
    file << "    \"total_migrations\": " << global_metrics_.total_migrations << ",\n";
    file << "    \"starved_process_count\": " << global_metrics_.starved_process_count << "\n";
    file << "  },\n";
    
    // Core metrics
    file << "  \"cores\": [\n";
    for (size_t i = 0; i < core_metrics_.size(); ++i) {
        const auto& cm = core_metrics_[i];
        file << "    {\n";
        file << "      \"core_id\": " << cm.core_id << ",\n";
        file << "      \"speed\": " << cm.speed << ",\n";
        file << "      \"busy_time\": " << cm.busy_time << ",\n";
        file << "      \"idle_time\": " << cm.idle_time << ",\n";
        file << "      \"overhead_time\": " << cm.overhead_time << ",\n";
        file << "      \"utilization\": " << cm.utilization << ",\n";
        file << "      \"context_switches\": " << cm.context_switches << "\n";
        file << "    }" << (i + 1 < core_metrics_.size() ? "," : "") << "\n";
    }
    file << "  ],\n";
    
    // Process metrics
    file << "  \"processes\": [\n";
    for (size_t i = 0; i < process_metrics_.size(); ++i) {
        const auto& pm = process_metrics_[i];
        file << "    {\n";
        file << "      \"pid\": " << pm.pid << ",\n";
        file << "      \"name\": \"" << pm.name << "\",\n";
        file << "      \"arrival_time\": " << pm.arrival_time << ",\n";
        file << "      \"burst_time\": " << pm.burst_time << ",\n";
        file << "      \"completion_time\": " << pm.completion_time << ",\n";
        file << "      \"turnaround_time\": " << pm.turnaround_time << ",\n";
        file << "      \"waiting_time\": " << pm.waiting_time << ",\n";
        file << "      \"response_time\": " << pm.response_time << ",\n";
        file << "      \"migrations\": " << pm.migrations << ",\n";
        file << "      \"starved\": " << (pm.starved ? "true" : "false") << "\n";
        file << "    }" << (i + 1 < process_metrics_.size() ? "," : "") << "\n";
    }
    file << "  ]\n";
    file << "}\n";
}

void MetricsCollector::export_comparison_csv(const std::vector<AlgorithmResult>& results, const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for CSV comparison export: " + filename);
    }
    
    file << "algorithm,makespan,throughput,avg_tat,p95_tat,p99_tat,avg_wait,p95_wait,p99_wait,avg_resp,p95_resp,p99_resp,cpu_util,jains_fairness,context_switches,migrations,starved_count\n";
    for (const auto& res : results) {
        const auto& m = res.metrics;
        file << res.algorithm_name << ","
             << m.total_time << ","
             << m.throughput << ","
             << m.avg_turnaround_time << ","
             << m.p95_turnaround_time << ","
             << m.p99_turnaround_time << ","
             << m.avg_waiting_time << ","
             << m.p95_waiting_time << ","
             << m.p99_waiting_time << ","
             << m.avg_response_time << ","
             << m.p95_response_time << ","
             << m.p99_response_time << ","
             << m.cpu_utilization << ","
             << m.jains_fairness_index << ","
             << m.total_context_switches << ","
             << m.total_migrations << ","
             << m.starved_process_count << "\n";
    }
}

void MetricsCollector::export_comparison_json(const std::vector<AlgorithmResult>& results, const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for JSON comparison export: " + filename);
    }
    
    file << "{\n  \"algorithms\": [\n";
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& res = results[i];
        const auto& m = res.metrics;
        file << "    {\n";
        file << "      \"name\": \"" << res.algorithm_name << "\",\n";
        file << "      \"makespan\": " << m.total_time << ",\n";
        file << "      \"throughput\": " << m.throughput << ",\n";
        file << "      \"cpu_utilization\": " << m.cpu_utilization << ",\n";
        file << "      \"avg_turnaround_time\": " << m.avg_turnaround_time << ",\n";
        file << "      \"p95_turnaround_time\": " << m.p95_turnaround_time << ",\n";
        file << "      \"p99_turnaround_time\": " << m.p99_turnaround_time << ",\n";
        file << "      \"avg_waiting_time\": " << m.avg_waiting_time << ",\n";
        file << "      \"p95_waiting_time\": " << m.p95_waiting_time << ",\n";
        file << "      \"p99_waiting_time\": " << m.p99_waiting_time << ",\n";
        file << "      \"avg_response_time\": " << m.avg_response_time << ",\n";
        file << "      \"p95_response_time\": " << m.p95_response_time << ",\n";
        file << "      \"p99_response_time\": " << m.p99_response_time << ",\n";
        file << "      \"jains_fairness_index\": " << m.jains_fairness_index << ",\n";
        file << "      \"total_context_switches\": " << m.total_context_switches << ",\n";
        file << "      \"total_migrations\": " << m.total_migrations << ",\n";
        file << "      \"starved_process_count\": " << m.starved_process_count << "\n";
        file << "    }" << (i + 1 < results.size() ? "," : "") << "\n";
    }
    file << "  ]\n}\n";
}

} // namespace scheduler
