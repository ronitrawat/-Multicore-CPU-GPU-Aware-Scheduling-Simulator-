#include "BenchmarkHarness.h"
#include "WorkloadLoader.h"
#include "FCFSScheduler.h"
#include "RoundRobinScheduler.h"
#include "SimulationEngine.h"
#include "MetricsCollector.h"
#include "Core.h"
#include <chrono>
#include <iostream>
#include <iomanip>

namespace scheduler {

std::vector<BenchmarkThroughputResult> BenchmarkHarness::run_scalability_sweep(
    const std::vector<uint32_t>& counts) {
    
    std::vector<BenchmarkThroughputResult> results;
    
    for (uint32_t count : counts) {
        WorkloadLoader::GeneratorConfig cfg;
        cfg.num_processes = count;
        cfg.seed = 42;
        cfg.max_arrival_time = count / 2;
        cfg.min_burst_time = 1;
        cfg.max_burst_time = 10;
        
        auto workload = WorkloadLoader::generate_random(cfg);
        
        std::vector<Core> cores;
        cores.emplace_back(0, 1.0);
        cores.emplace_back(1, 1.0);
        
        auto sched = std::make_unique<FCFSScheduler>();
        SimulationEngine engine(std::move(sched), std::move(cores), 0);
        engine.set_silent(true);
        
        auto start = std::chrono::high_resolution_clock::now();
        engine.run(workload);
        auto end = std::chrono::high_resolution_clock::now();
        
        std::chrono::duration<double> diff = end - start;
        
        BenchmarkThroughputResult res;
        res.process_count = count;
        res.elapsed_seconds = diff.count();
        res.processes_per_second = diff.count() > 0 ? (count / diff.count()) : 0.0;
        res.simulated_makespan = engine.current_time();
        
        results.push_back(res);
    }
    
    return results;
}

std::vector<QuantumSweepResult> BenchmarkHarness::run_quantum_sweep(
    uint32_t num_processes, const std::vector<uint32_t>& quanta) {
    
    std::vector<QuantumSweepResult> results;
    
    WorkloadLoader::GeneratorConfig cfg;
    cfg.num_processes = num_processes;
    cfg.seed = 42;
    cfg.max_arrival_time = 20;
    cfg.min_burst_time = 2;
    cfg.max_burst_time = 20;
    auto base_workload = WorkloadLoader::generate_random(cfg);
    
    for (uint32_t q : quanta) {
        auto workload = WorkloadLoader::clone_workload(base_workload);
        std::vector<Core> cores;
        cores.emplace_back(0, 1.0);
        
        auto sched = std::make_unique<RoundRobinScheduler>(q);
        SimulationEngine engine(std::move(sched), std::move(cores), 1); // 1 tick CS overhead
        engine.set_silent(true);
        engine.run(workload);
        
        MetricsCollector metrics;
        for (const auto& p : workload) metrics.collect_process(*p);
        metrics.collect_cores(engine.cores());
        metrics.compute_global_metrics(engine.current_time());
        
        QuantumSweepResult res;
        res.quantum = q;
        res.makespan = metrics.global_metrics().total_time;
        res.avg_turnaround = metrics.global_metrics().avg_turnaround_time;
        res.avg_waiting = metrics.global_metrics().avg_waiting_time;
        res.avg_response = metrics.global_metrics().avg_response_time;
        res.context_switches = metrics.global_metrics().total_context_switches;
        
        results.push_back(res);
    }
    
    return results;
}

std::vector<CoreScalingResult> BenchmarkHarness::run_core_scaling_sweep(
    uint32_t num_processes, const std::vector<int>& core_counts) {
    
    std::vector<CoreScalingResult> results;
    
    WorkloadLoader::GeneratorConfig cfg;
    cfg.num_processes = num_processes;
    cfg.seed = 42;
    cfg.max_arrival_time = 10;
    cfg.min_burst_time = 4;
    cfg.max_burst_time = 20;
    auto base_workload = WorkloadLoader::generate_random(cfg);
    
    uint32_t single_core_makespan = 0;
    
    for (int nc : core_counts) {
        auto workload = WorkloadLoader::clone_workload(base_workload);
        std::vector<Core> cores;
        for (int i = 0; i < nc; ++i) cores.emplace_back(i, 1.0);
        
        auto sched = std::make_unique<FCFSScheduler>();
        SimulationEngine engine(std::move(sched), std::move(cores), 0);
        engine.set_silent(true);
        engine.run(workload);
        
        MetricsCollector metrics;
        for (const auto& p : workload) metrics.collect_process(*p);
        metrics.collect_cores(engine.cores());
        metrics.compute_global_metrics(engine.current_time());
        
        uint32_t ms = metrics.global_metrics().total_time;
        if (nc == 1) single_core_makespan = ms;
        
        CoreScalingResult res;
        res.num_cores = nc;
        res.makespan = ms;
        res.speedup = ms > 0 ? (static_cast<double>(single_core_makespan) / ms) : 1.0;
        res.cpu_utilization = metrics.global_metrics().cpu_utilization;
        
        results.push_back(res);
    }
    
    return results;
}

void BenchmarkHarness::print_benchmarks() {
    std::cout << "\n================= BENCHMARK 1: Scalability Sweep (Up to 50,000 Tasks) =================\n\n";
    auto scale_results = run_scalability_sweep({100, 1000, 10000, 50000});
    std::cout << std::setw(12) << "Processes"
              << std::setw(16) << "Wall Clock"
              << std::setw(22) << "Throughput (proc/sec)"
              << std::setw(16) << "Sim Makespan" << "\n";
    std::cout << std::string(66, '-') << "\n";
    for (const auto& r : scale_results) {
        std::cout << std::setw(12) << r.process_count
                  << std::setw(13) << std::fixed << std::setprecision(4) << (r.elapsed_seconds * 1000.0) << " ms"
                  << std::setw(22) << std::fixed << std::setprecision(0) << r.processes_per_second
                  << std::setw(16) << r.simulated_makespan << "\n";
    }
    std::cout << "\n" << std::flush;

    std::cout << "================= BENCHMARK 2: Quantum Sensitivity Analysis =================\n\n";
    auto q_results = run_quantum_sweep(40, {1, 2, 4, 8, 16, 32});
    std::cout << std::setw(8) << "Quantum"
              << std::setw(10) << "Makespan"
              << std::setw(12) << "Avg TAT"
              << std::setw(12) << "Avg Wait"
              << std::setw(12) << "Avg Resp"
              << std::setw(10) << "CtxSw" << "\n";
    std::cout << std::string(64, '-') << "\n";
    for (const auto& r : q_results) {
        std::cout << std::setw(8) << r.quantum
                  << std::setw(10) << r.makespan
                  << std::setw(12) << std::fixed << std::setprecision(2) << r.avg_turnaround
                  << std::setw(12) << std::fixed << std::setprecision(2) << r.avg_waiting
                  << std::setw(12) << std::fixed << std::setprecision(2) << r.avg_response
                  << std::setw(10) << r.context_switches << "\n";
    }
    std::cout << "\n" << std::flush;

    std::cout << "================= BENCHMARK 3: Multicore Scaling Efficiency =================\n\n";
    auto core_results = run_core_scaling_sweep(80, {1, 2, 4, 8, 16});
    std::cout << std::setw(8) << "Cores"
              << std::setw(12) << "Makespan"
              << std::setw(12) << "Speedup"
              << std::setw(14) << "Efficiency"
              << std::setw(12) << "CPU Util" << "\n";
    std::cout << std::string(58, '-') << "\n";
    for (const auto& r : core_results) {
        double eff = (r.num_cores > 0) ? (r.speedup / r.num_cores * 100.0) : 0.0;
        std::cout << std::setw(8) << r.num_cores
                  << std::setw(12) << r.makespan
                  << std::setw(10) << std::fixed << std::setprecision(2) << r.speedup << "x"
                  << std::setw(13) << std::fixed << std::setprecision(1) << eff << "%"
                  << std::setw(11) << std::fixed << std::setprecision(1) << (r.cpu_utilization * 100.0) << "%\n";
    }
    std::cout << std::string(58, '-') << "\n\n" << std::flush;
}

} // namespace scheduler
