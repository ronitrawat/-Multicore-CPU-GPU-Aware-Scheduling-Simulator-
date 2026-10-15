#include <iostream>
#include <memory>
#include <string>
#include <sstream>
#include <vector>
#include "Process.h"
#include "Core.h"
#include "FCFSScheduler.h"
#include "SJFScheduler.h"
#include "SRTFScheduler.h"
#include "RoundRobinScheduler.h"
#include "PriorityScheduler.h"
#include "MLFQScheduler.h"
#include "SimulationEngine.h"
#include "MetricsCollector.h"
#include "WorkloadLoader.h"
#include "LoadBalancer.h"

// Phase 5 NVIDIA extensions
#include "gpu/WarpScheduler.h"
#include "gpu/CudaStreamGraph.h"
#include "PowerModel.h"
#include "ParallelEngine.h"
#include "BenchmarkHarness.h"

using namespace scheduler;

void print_usage() {
    std::cout << "Multicore CPU & GPU-Aware Scheduling Simulator - Modern C++ (Phase 5)\n";
    std::cout << "Usage: scheduler [options]\n\n";
    std::cout << "Scheduling Algorithm Options:\n";
    std::cout << "  --algo <name>           Scheduling algorithm: fcfs, sjf, srtf, rr, priority,\n";
    std::cout << "                          priority-np, mlfq, all (default: fcfs)\n";
    std::cout << "  --quantum <N>           Time quantum for Round Robin (default: 4)\n";
    std::cout << "  --aging <N>             Aging interval for Priority scheduler (default: 10, 0=disabled)\n";
    std::cout << "  --mlfq-quanta <Q1,Q2..> Time quanta per MLFQ level (default: 2,4,8)\n";
    std::cout << "  --mlfq-boost <N>        MLFQ priority boost interval in ticks (default: 50, 0=disabled)\n\n";
    std::cout << "Hardware & Topology Options:\n";
    std::cout << "  --cores <N>             Number of cores (default: 1)\n";
    std::cout << "  --speeds <S1,S2...>     Core speeds (e.g. 1.0,1.0,1.5,2.0 for big.LITTLE)\n";
    std::cout << "  --load-balancer <LB>    Load balancer: global, percore, speedaware (default: global)\n";
    std::cout << "  --migration-penalty <P> Migration penalty in ticks (default: 1)\n";
    std::cout << "  --ctx-switch <T>        Context switch cost in ticks (default: 0)\n\n";
    std::cout << "Workload Options:\n";
    std::cout << "  --workload <file>       Load workload from CSV file\n";
    std::cout << "  --random <N>            Generate N random processes\n";
    std::cout << "  --distribution <dist>   Distribution: uniform, exponential, bimodal (default: uniform)\n";
    std::cout << "  --seed <S>              Random seed (default: 42)\n";
    std::cout << "  --save-workload <file>  Save generated workload to CSV file\n\n";
    std::cout << "Metrics & Visualization Options:\n";
    std::cout << "  --starvation-threshold <T> Report processes waiting >= T ticks (default: 0=off)\n";
    std::cout << "  --power                 Calculate energy & power consumption report\n";
    std::cout << "  --output <file.csv>     Export metrics to CSV\n";
    std::cout << "  --output-json <file.json> Export metrics to JSON\n";
    std::cout << "  --gantt-output <file.csv> Export Gantt timeline segments to CSV\n";
    std::cout << "  --gantt                 Display ASCII Gantt chart in terminal\n\n";
    std::cout << "NVIDIA & Performance Extensions:\n";
    std::cout << "  --gpu-sim               Simulate NVIDIA Streaming Multiprocessor (SM) warp scheduling\n";
    std::cout << "  --cuda-streams          Simulate multi-stream CUDA task dependency graph (DAG)\n";
    std::cout << "  --parallel              Run comparisons / parameter sweeps across host threads\n";
    std::cout << "  --benchmark             Run scalability & sensitivity benchmark suite (up to 50k)\n";
    std::cout << "  --help                  Show this help message\n\n";
}

// Parse core speeds from comma-separated string
std::vector<double> parse_speeds(const std::string& speeds_str, int num_cores) {
    std::vector<double> speeds;
    std::stringstream ss(speeds_str);
    std::string speed_str;
    
    while (std::getline(ss, speed_str, ',')) {
        try {
            double speed = std::stod(speed_str);
            speeds.push_back(speed);
        } catch (const std::exception&) {
            std::cerr << "Invalid speed value: " << speed_str << "\n";
            return {};
        }
    }
    
    while (static_cast<int>(speeds.size()) < num_cores) {
        speeds.push_back(speeds.empty() ? 1.0 : speeds.back());
    }
    
    return speeds;
}

std::vector<uint32_t> parse_quanta(const std::string& quanta_str) {
    std::vector<uint32_t> quanta;
    std::stringstream ss(quanta_str);
    std::string q_str;
    while (std::getline(ss, q_str, ',')) {
        try {
            quanta.push_back(std::stoul(q_str));
        } catch (const std::exception&) {
            return {2, 4, 8};
        }
    }
    return quanta.empty() ? std::vector<uint32_t>{2, 4, 8} : quanta;
}

std::unique_ptr<ILoadBalancer> create_load_balancer(const std::string& type, uint32_t migration_penalty) {
    if (type == "percore") {
        return std::make_unique<PerCoreBalancer>(migration_penalty);
    } else if (type == "speedaware") {
        return std::make_unique<SpeedAwareBalancer>(migration_penalty);
    }
    return std::make_unique<GlobalQueueBalancer>(migration_penalty);
}

std::unique_ptr<IScheduler> create_scheduler(const std::string& name, 
                                            uint32_t quantum, 
                                            uint32_t aging_interval,
                                            const std::vector<uint32_t>& mlfq_quanta,
                                            uint32_t mlfq_boost) {
    if (name == "fcfs") {
        return std::make_unique<FCFSScheduler>();
    } else if (name == "sjf") {
        return std::make_unique<SJFScheduler>();
    } else if (name == "srtf") {
        return std::make_unique<SRTFScheduler>();
    } else if (name == "rr") {
        return std::make_unique<RoundRobinScheduler>(quantum);
    } else if (name == "priority") {
        return std::make_unique<PriorityScheduler>(true, aging_interval);
    } else if (name == "priority-np") {
        return std::make_unique<PriorityScheduler>(false, aging_interval);
    } else if (name == "mlfq") {
        return std::make_unique<MLFQScheduler>(mlfq_quanta, mlfq_boost);
    }
    return nullptr;
}

void run_gpu_simulation_demo() {
    using namespace scheduler::gpu;
    std::cout << "\n================ Running NVIDIA Streaming Multiprocessor (SM) Simulation ================\n";
    
    // Create SM with 32 warps (1024 threads)
    for (auto policy : {WarpPolicy::GREEDY_THEN_OLDEST, WarpPolicy::ROUND_ROBIN}) {
        StreamingMultiprocessor sm(0, 32, policy);
        
        // Add 16 warps, each with a mix of compute and high-latency memory instructions
        for (uint32_t wid = 1; wid <= 16; ++wid) {
            Warp warp;
            warp.warp_id = wid;
            warp.block_id = wid / 4;
            
            // 4 compute instructions, 1 memory fetch (latency 20 cycles), 4 compute, 1 memory fetch (latency 30 cycles), 2 compute
            warp.instructions.push_back({false, 0});
            warp.instructions.push_back({false, 0});
            warp.instructions.push_back({false, 0});
            warp.instructions.push_back({false, 0});
            warp.instructions.push_back({true, 20}); // Memory stall
            warp.instructions.push_back({false, 0});
            warp.instructions.push_back({false, 0});
            warp.instructions.push_back({false, 0});
            warp.instructions.push_back({true, 30}); // Memory stall
            warp.instructions.push_back({false, 0});
            warp.instructions.push_back({false, 0});
            
            sm.add_warp(warp);
        }
        
        sm.run();
        sm.print_report();
    }
}

void run_cuda_streams_demo() {
    using namespace scheduler::gpu;
    std::cout << "\n================ Running CUDA Multi-Stream Task Dependency Simulation ================\n";
    
    CudaStreamGraphEngine engine(2, 2); // 2 Kernel Engines, 2 Copy Engines (DMA)
    
    // Stream 1: Pipeline stage for Batch A (HtoD -> Kernel -> DtoH)
    uint32_t a_copy_in = engine.add_task("MemcpyHtoD_A", 1, CudaTaskType::MEMCPY_H2D, 10);
    uint32_t a_kernel  = engine.add_task("KernelCompute_A", 1, CudaTaskType::KERNEL_EXECUTION, 25);
    uint32_t a_copy_out= engine.add_task("MemcpyDtoH_A", 1, CudaTaskType::MEMCPY_D2H, 10);
    
    // Stream 2: Pipeline stage for Batch B running concurrently
    uint32_t b_copy_in = engine.add_task("MemcpyHtoD_B", 2, CudaTaskType::MEMCPY_H2D, 10);
    uint32_t b_kernel  = engine.add_task("KernelCompute_B", 2, CudaTaskType::KERNEL_EXECUTION, 25);
    uint32_t b_copy_out= engine.add_task("MemcpyDtoH_B", 2, CudaTaskType::MEMCPY_D2H, 10);
    
    // Stream 3: Aggregation kernel dependent on both Batch A and Batch B completing
    engine.add_task("AggregateKernel", 3, CudaTaskType::KERNEL_EXECUTION, 15, {a_copy_out, b_copy_out});
    
    (void)a_copy_in; (void)a_kernel; (void)b_copy_in; (void)b_kernel;
    
    engine.run();
    engine.print_summary();
}

int main(int argc, char** argv) {
    std::string workload_file;
    std::string save_workload_file;
    std::string algo = "fcfs";
    std::string load_balancer_type = "global";
    std::string speeds_str;
    std::string dist_str = "uniform";
    std::string output_csv;
    std::string output_json;
    std::string gantt_output_csv;
    
    int num_random = 0;
    uint32_t seed = 42;
    uint32_t quantum = 4;
    uint32_t aging_interval = 10;
    std::vector<uint32_t> mlfq_quanta = {2, 4, 8};
    uint32_t mlfq_boost = 50;
    int num_cores = 1;
    uint32_t ctx_switch_cost = 0;
    uint32_t migration_penalty = 1;
    uint32_t starvation_threshold = 0;
    bool show_gantt = false;
    bool enable_power = false;
    bool run_parallel = false;
    
    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help") {
            print_usage();
            return 0;
        } else if (arg == "--benchmark") {
            BenchmarkHarness::print_benchmarks();
            return 0;
        } else if (arg == "--gpu-sim") {
            run_gpu_simulation_demo();
            return 0;
        } else if (arg == "--cuda-streams") {
            run_cuda_streams_demo();
            return 0;
        } else if (arg == "--power") {
            enable_power = true;
        } else if (arg == "--parallel") {
            run_parallel = true;
        } else if (arg == "--algo" && i + 1 < argc) {
            algo = argv[++i];
        } else if (arg == "--quantum" && i + 1 < argc) {
            quantum = std::stoul(argv[++i]);
        } else if (arg == "--aging" && i + 1 < argc) {
            aging_interval = std::stoul(argv[++i]);
        } else if (arg == "--mlfq-quanta" && i + 1 < argc) {
            mlfq_quanta = parse_quanta(argv[++i]);
        } else if (arg == "--mlfq-boost" && i + 1 < argc) {
            mlfq_boost = std::stoul(argv[++i]);
        } else if (arg == "--workload" && i + 1 < argc) {
            workload_file = argv[++i];
        } else if (arg == "--save-workload" && i + 1 < argc) {
            save_workload_file = argv[++i];
        } else if (arg == "--random" && i + 1 < argc) {
            num_random = std::stoi(argv[++i]);
        } else if (arg == "--distribution" && i + 1 < argc) {
            dist_str = argv[++i];
        } else if (arg == "--seed" && i + 1 < argc) {
            seed = std::stoul(argv[++i]);
        } else if (arg == "--cores" && i + 1 < argc) {
            num_cores = std::stoi(argv[++i]);
        } else if (arg == "--speeds" && i + 1 < argc) {
            speeds_str = argv[++i];
        } else if (arg == "--load-balancer" && i + 1 < argc) {
            load_balancer_type = argv[++i];
        } else if (arg == "--migration-penalty" && i + 1 < argc) {
            migration_penalty = std::stoul(argv[++i]);
        } else if (arg == "--ctx-switch" && i + 1 < argc) {
            ctx_switch_cost = std::stoul(argv[++i]);
        } else if (arg == "--starvation-threshold" && i + 1 < argc) {
            starvation_threshold = std::stoul(argv[++i]);
        } else if (arg == "--output" && i + 1 < argc) {
            output_csv = argv[++i];
        } else if (arg == "--output-json" && i + 1 < argc) {
            output_json = argv[++i];
        } else if (arg == "--gantt-output" && i + 1 < argc) {
            gantt_output_csv = argv[++i];
        } else if (arg == "--gantt") {
            show_gantt = true;
        }
    }
    
    try {
        // Load or generate workload
        std::vector<std::unique_ptr<Process>> base_workload;
        
        if (!workload_file.empty()) {
            std::cout << "Loading workload from: " << workload_file << "\n";
            base_workload = WorkloadLoader::load_from_csv(workload_file);
        } else if (num_random > 0) {
            WorkloadDistribution dist = WorkloadLoader::parse_distribution(dist_str);
            std::cout << "Generating " << num_random << " processes (" 
                      << WorkloadLoader::distribution_to_string(dist) 
                      << " distribution, seed: " << seed << ")\n";
                      
            WorkloadLoader::GeneratorConfig config;
            config.num_processes = num_random;
            config.seed = seed;
            config.distribution = dist;
            config.use_priorities = true;
            
            base_workload = WorkloadLoader::generate_random(config);
            
            if (!save_workload_file.empty()) {
                WorkloadLoader::save_to_csv(base_workload, save_workload_file);
                std::cout << "Saved generated workload to: " << save_workload_file << "\n";
            }
        } else {
            std::cout << "No workload specified. Using standard default benchmark workload.\n";
            base_workload.push_back(std::make_unique<Process>(1, "P1", 0, 10, 3));
            base_workload.push_back(std::make_unique<Process>(2, "P2", 1, 4, 1));
            base_workload.push_back(std::make_unique<Process>(3, "P3", 2, 8, 4));
            base_workload.push_back(std::make_unique<Process>(4, "P4", 3, 2, 2));
            base_workload.push_back(std::make_unique<Process>(5, "P5", 4, 6, 5));
        }
        
        std::cout << "Processes: " << base_workload.size() << "\n";
        
        // Parse core speeds
        std::vector<double> speeds;
        if (!speeds_str.empty()) {
            speeds = parse_speeds(speeds_str, num_cores);
            if (speeds.empty()) return 1;
        } else {
            speeds.assign(num_cores, 1.0);
        }
        
        std::cout << "Cores: " << num_cores << " (speeds: ";
        for (size_t i = 0; i < speeds.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << speeds[i] << "x";
        }
        std::cout << ")\n";
        std::cout << "Load Balancer: " << load_balancer_type << "\n";
        std::cout << "Migration Penalty: " << migration_penalty << " ticks\n";
        std::cout << "Context Switch Cost: " << ctx_switch_cost << " ticks\n\n";
        
        // Mode 1: Comparison mode (--algo all)
        if (algo == "all") {
            std::vector<std::string> algorithms = {
                "fcfs", "sjf", "srtf", "rr", "priority", "priority-np", "mlfq"
            };
            
            std::vector<AlgorithmResult> comparison_results;
            
            if (run_parallel) {
                std::vector<ParallelSimulationJob> jobs;
                for (const auto& a_name : algorithms) {
                    ParallelSimulationJob job;
                    job.algorithm_name = a_name;
                    job.quantum = quantum;
                    job.aging_interval = aging_interval;
                    job.mlfq_quanta = mlfq_quanta;
                    job.mlfq_boost = mlfq_boost;
                    job.num_cores = num_cores;
                    job.core_speeds = speeds;
                    job.load_balancer_type = load_balancer_type;
                    job.migration_penalty = migration_penalty;
                    job.ctx_switch_cost = ctx_switch_cost;
                    jobs.push_back(job);
                }
                
                ParallelSimulationEngine parallel_engine;
                auto p_results = parallel_engine.run_batch(base_workload, jobs);
                comparison_results = p_results.results;
                ParallelSimulationEngine::print_parallel_report(p_results);
            } else {
                for (const auto& algo_name : algorithms) {
                    auto run_workload = WorkloadLoader::clone_workload(base_workload);
                    
                    std::vector<Core> cores;
                    for (int i = 0; i < num_cores; ++i) {
                        cores.emplace_back(i, speeds[i]);
                    }
                    
                    auto lb = create_load_balancer(load_balancer_type, migration_penalty);
                    auto sched = create_scheduler(algo_name, quantum, aging_interval, mlfq_quanta, mlfq_boost);
                    
                    SimulationEngine engine(std::move(sched), std::move(cores), 
                                           ctx_switch_cost, false, std::move(lb));
                    engine.run(run_workload);
                    
                    MetricsCollector metrics(starvation_threshold);
                    for (const auto& proc : run_workload) {
                        metrics.collect_process(*proc);
                    }
                    metrics.collect_cores(engine.cores());
                    metrics.compute_global_metrics(engine.current_time());
                    
                    AlgorithmResult res;
                    res.algorithm_name = algo_name;
                    res.metrics = metrics.global_metrics();
                    res.core_metrics = metrics.core_metrics();
                    comparison_results.push_back(res);
                }
            }
            
            MetricsCollector::print_comparison_table(comparison_results);
            
            if (!output_csv.empty()) {
                MetricsCollector::export_comparison_csv(comparison_results, output_csv);
                std::cout << "Saved comparison CSV to: " << output_csv << "\n";
            }
            if (!output_json.empty()) {
                MetricsCollector::export_comparison_json(comparison_results, output_json);
                std::cout << "Saved comparison JSON to: " << output_json << "\n";
            }
            
            return 0;
        }
        
        // Mode 2: Single algorithm run
        std::vector<Core> cores;
        for (int i = 0; i < num_cores; ++i) {
            cores.emplace_back(i, speeds[i]);
        }
        
        auto load_balancer = create_load_balancer(load_balancer_type, migration_penalty);
        auto scheduler = create_scheduler(algo, quantum, aging_interval, mlfq_quanta, mlfq_boost);
        
        if (!scheduler) {
            std::cerr << "Unknown algorithm: " << algo << "\n";
            std::cerr << "Available: fcfs, sjf, srtf, rr, priority, priority-np, mlfq, all\n";
            return 1;
        }
        
        std::cout << "Using scheduler: " << scheduler->config_string() << "\n\n";
        
        bool need_gantt = show_gantt || !gantt_output_csv.empty();
        SimulationEngine engine(std::move(scheduler), std::move(cores), 
                               ctx_switch_cost, need_gantt, std::move(load_balancer));
        engine.run(base_workload);
        
        MetricsCollector metrics(starvation_threshold);
        for (const auto& proc : base_workload) {
            metrics.collect_process(*proc);
        }
        metrics.collect_cores(engine.cores());
        metrics.compute_global_metrics(engine.current_time());
        
        metrics.print_detailed();
        metrics.print_summary();
        
        if (enable_power) {
            PowerModel power_model;
            auto power_report = power_model.calculate_power(
                engine.cores(), 
                static_cast<uint32_t>(base_workload.size()), 
                metrics.global_metrics().total_time, 
                metrics.global_metrics().throughput);
            power_model.print_power_report(power_report);
        }
        
        if (show_gantt && engine.has_gantt_chart()) {
            engine.gantt_chart().print_ascii();
        }
        
        if (!gantt_output_csv.empty() && engine.has_gantt_chart()) {
            engine.gantt_chart().export_csv(gantt_output_csv);
            std::cout << "\nExported Gantt timeline to: " << gantt_output_csv << "\n";
        }
        
        if (!output_csv.empty()) {
            metrics.export_to_csv(output_csv);
            std::cout << "Exported metrics to CSV: " << output_csv << "\n";
        }
        
        if (!output_json.empty()) {
            metrics.export_to_json(output_json, algo);
            std::cout << "Exported metrics to JSON: " << output_json << "\n";
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
