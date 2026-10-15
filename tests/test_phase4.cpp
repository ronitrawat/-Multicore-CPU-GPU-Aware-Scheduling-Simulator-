#include <gtest/gtest.h>
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
#include <memory>
#include <vector>
#include <fstream>
#include <cmath>

using namespace scheduler;

// =========================================================================
// 1. MLFQ (Multilevel Feedback Queue) Scheduler Tests
// =========================================================================

// Test basic MLFQ operation: short process completes in Q0 without demotion
TEST(MLFQTest, ShortJobCompletesInTopQueue) {
    std::vector<std::unique_ptr<Process>> workload;
    workload.push_back(std::make_unique<Process>(1, "P1", 0, 2, 0)); // Burst = 2, fits in Q0 (quantum=2)
    workload.push_back(std::make_unique<Process>(2, "P2", 0, 1, 0)); // Burst = 1, fits in Q0
    
    std::vector<Core> cores;
    cores.emplace_back(0, 1.0);
    
    auto scheduler = std::make_unique<MLFQScheduler>(std::vector<uint32_t>{2, 4, 8}, 50);
    SimulationEngine engine(std::move(scheduler), std::move(cores), 0);
    
    engine.run(workload);
    
    EXPECT_EQ(workload[0]->completion_time, 2u);
    EXPECT_EQ(workload[1]->completion_time, 3u);
    EXPECT_EQ(workload[0]->waiting_time, 0u);
    EXPECT_EQ(workload[1]->waiting_time, 2u);
}

// Test demotion: long process exhausts Q0 quantum and is demoted to Q1
TEST(MLFQTest, QuantumExpirationCausesDemotion) {
    std::vector<std::unique_ptr<Process>> workload;
    // P1: burst 8, arrives at 0.
    // Q0 has quantum 2, Q1 has quantum 4, Q2 has quantum 8
    workload.push_back(std::make_unique<Process>(1, "P1", 0, 8, 0));
    
    // P2: burst 1, arrives at t=3 (while P1 is in Q1)
    workload.push_back(std::make_unique<Process>(2, "P2", 3, 1, 0));
    
    std::vector<Core> cores;
    cores.emplace_back(0, 1.0);
    
    auto scheduler = std::make_unique<MLFQScheduler>(std::vector<uint32_t>{2, 4, 8}, 50);
    SimulationEngine engine(std::move(scheduler), std::move(cores), 0);
    
    engine.run(workload);
    
    // P1 runs t=0..2 in Q0 (demoted to Q1), runs t=2..3 in Q1.
    // At t=3, P2 arrives in Q0. P2 preempts P1 (since Q0 > Q1).
    // P2 completes at t=4.
    // P1 resumes at t=4 and finishes at t=9.
    EXPECT_EQ(workload[1]->completion_time, 4u);
    EXPECT_EQ(workload[0]->completion_time, 9u);
}

// Test priority boost: prevents starvation of compute-bound jobs in lower queues
TEST(MLFQTest, PriorityBoostPreventsStarvation) {
    std::vector<std::unique_ptr<Process>> workload;
    // P1 is a long job that quickly falls to lower queues
    workload.push_back(std::make_unique<Process>(1, "P1", 0, 15, 0));
    
    // P2 and P3 arrive periodically in Q0
    workload.push_back(std::make_unique<Process>(2, "P2", 2, 2, 0));
    workload.push_back(std::make_unique<Process>(3, "P3", 5, 2, 0));
    
    std::vector<Core> cores;
    cores.emplace_back(0, 1.0);
    
    // Priority boost every 8 ticks
    auto scheduler = std::make_unique<MLFQScheduler>(std::vector<uint32_t>{2, 4, 8}, 8);
    SimulationEngine engine(std::move(scheduler), std::move(cores), 0);
    
    engine.run(workload);
    
    EXPECT_TRUE(workload[0]->is_complete());
    EXPECT_TRUE(workload[1]->is_complete());
    EXPECT_TRUE(workload[2]->is_complete());
}

// =========================================================================
// 2. Advanced Metrics & Fairness Tests
// =========================================================================

// Test percentiles computation (P95 and P99)
TEST(MetricsTest, PercentilesAccuracy) {
    MetricsCollector metrics;
    
    // Create 100 processes with known waiting times 1..100
    for (uint32_t i = 1; i <= 100; ++i) {
        Process p(i, "P" + std::to_string(i), 0, 10, 0);
        p.first_response_time = i;
        p.completion_time = i + 10;
        p.waiting_time = i;
        metrics.collect_process(p);
    }
    
    metrics.compute_global_metrics(110);
    const auto& g = metrics.global_metrics();
    
    // P95 of [1..100] is 95.05, P99 is 99.01
    EXPECT_NEAR(g.p95_waiting_time, 95.05, 0.5);
    EXPECT_NEAR(g.p99_waiting_time, 99.01, 0.5);
    EXPECT_EQ(g.min_waiting_time, 1u);
    EXPECT_EQ(g.max_waiting_time, 100u);
}

// Test Jain's Fairness Index
TEST(MetricsTest, JainsFairnessIndex) {
    // Case 1: Perfectly equal normalized turnaround times -> J = 1.0
    {
        MetricsCollector metrics;
        for (uint32_t i = 1; i <= 4; ++i) {
            Process p(i, "P" + std::to_string(i), 0, 5, 0);
            p.completion_time = 10; // TAT = 10, Burst = 5, Normalized TAT = 2.0
            metrics.collect_process(p);
        }
        metrics.compute_global_metrics(10);
        EXPECT_NEAR(metrics.global_metrics().jains_fairness_index, 1.0, 1e-4);
    }
    
    // Case 2: Unequal turnaround times -> J < 1.0
    {
        MetricsCollector metrics;
        Process p1(1, "P1", 0, 1, 0); p1.completion_time = 1; // Norm = 1.0
        Process p2(2, "P2", 0, 1, 0); p2.completion_time = 9; // Norm = 9.0
        metrics.collect_process(p1);
        metrics.collect_process(p2);
        metrics.compute_global_metrics(10);
        
        // J = (1 + 9)^2 / (2 * (1^2 + 9^2)) = 100 / (2 * 82) = 100 / 164 ≈ 0.6097
        EXPECT_NEAR(metrics.global_metrics().jains_fairness_index, 100.0 / 164.0, 1e-3);
    }
}

// Test Starvation Detection
TEST(MetricsTest, StarvationThresholdReporting) {
    MetricsCollector metrics(15); // Threshold = 15 ticks
    
    Process p1(1, "P1", 0, 10, 0); p1.waiting_time = 5;  // Not starved
    Process p2(2, "P2", 0, 10, 0); p2.waiting_time = 20; // Starved (20 >= 15)
    Process p3(3, "P3", 0, 10, 0); p3.waiting_time = 15; // Starved (15 >= 15)
    
    metrics.collect_process(p1);
    metrics.collect_process(p2);
    metrics.collect_process(p3);
    metrics.compute_global_metrics(30);
    
    EXPECT_EQ(metrics.global_metrics().starved_process_count, 2u);
    EXPECT_FALSE(metrics.process_metrics()[0].starved);
    EXPECT_TRUE(metrics.process_metrics()[1].starved);
    EXPECT_TRUE(metrics.process_metrics()[2].starved);
}

// =========================================================================
// 3. Workload Generator Tests
// =========================================================================

// Test random generation reproducibility with fixed seed
TEST(WorkloadGeneratorTest, SeedReproducibility) {
    WorkloadLoader::GeneratorConfig cfg1;
    cfg1.num_processes = 25;
    cfg1.seed = 12345;
    cfg1.distribution = WorkloadDistribution::UNIFORM;
    
    WorkloadLoader::GeneratorConfig cfg2 = cfg1;
    
    auto workload1 = WorkloadLoader::generate_random(cfg1);
    auto workload2 = WorkloadLoader::generate_random(cfg2);
    
    ASSERT_EQ(workload1.size(), workload2.size());
    for (size_t i = 0; i < workload1.size(); ++i) {
        EXPECT_EQ(workload1[i]->pid, workload2[i]->pid);
        EXPECT_EQ(workload1[i]->arrival_time, workload2[i]->arrival_time);
        EXPECT_EQ(workload1[i]->burst_time, workload2[i]->burst_time);
        EXPECT_EQ(workload1[i]->priority, workload2[i]->priority);
    }
}

// Test Exponential and Bimodal distributions generate valid processes
TEST(WorkloadGeneratorTest, DistributionShapes) {
    WorkloadLoader::GeneratorConfig exp_cfg;
    exp_cfg.num_processes = 50;
    exp_cfg.seed = 42;
    exp_cfg.distribution = WorkloadDistribution::EXPONENTIAL;
    auto exp_workload = WorkloadLoader::generate_random(exp_cfg);
    EXPECT_EQ(exp_workload.size(), 50u);
    for (const auto& proc : exp_workload) {
        EXPECT_GE(proc->burst_time, 1u);
    }
    
    WorkloadLoader::GeneratorConfig bim_cfg;
    bim_cfg.num_processes = 50;
    bim_cfg.seed = 42;
    bim_cfg.distribution = WorkloadDistribution::BIMODAL;
    auto bim_workload = WorkloadLoader::generate_random(bim_cfg);
    EXPECT_EQ(bim_workload.size(), 50u);
    for (const auto& proc : bim_workload) {
        EXPECT_GE(proc->burst_time, 1u);
    }
}

// Test Workload Cloning integrity
TEST(WorkloadGeneratorTest, WorkloadCloning) {
    std::vector<std::unique_ptr<Process>> original;
    original.push_back(std::make_unique<Process>(1, "P1", 0, 10, 2));
    original.push_back(std::make_unique<Process>(2, "P2", 5, 20, 1));
    
    auto clone = WorkloadLoader::clone_workload(original);
    ASSERT_EQ(clone.size(), original.size());
    
    // Modify original and ensure clone is isolated
    original[0]->remaining_time = 0.0;
    EXPECT_EQ(clone[0]->remaining_time, 10.0);
    EXPECT_EQ(clone[0]->pid, 1u);
    EXPECT_EQ(clone[1]->priority, 1);
}

// =========================================================================
// 4. CSV & JSON Export Tests
// =========================================================================

TEST(ExportTest, CSVAndJSONGeneration) {
    MetricsCollector metrics(10);
    Process p(1, "P1", 0, 5, 1);
    p.completion_time = 5;
    p.waiting_time = 0;
    p.first_response_time = 0;
    metrics.collect_process(p);
    
    std::vector<Core> cores;
    cores.emplace_back(0, 1.0);
    metrics.collect_cores(cores);
    metrics.compute_global_metrics(5);
    
    std::string csv_file = "test_metrics_export.csv";
    std::string json_file = "test_metrics_export.json";
    
    metrics.export_to_csv(csv_file);
    metrics.export_to_json(json_file, "fcfs");
    
    // Check CSV file exists and contains headers
    std::ifstream csv_in(csv_file);
    ASSERT_TRUE(csv_in.is_open());
    std::string first_line;
    std::getline(csv_in, first_line);
    EXPECT_FALSE(first_line.empty());
    csv_in.close();
    
    // Check JSON file exists and contains key fields
    std::ifstream json_in(json_file);
    ASSERT_TRUE(json_in.is_open());
    std::stringstream buffer;
    buffer << json_in.rdbuf();
    std::string json_content = buffer.str();
    json_in.close();
    
    EXPECT_NE(json_content.find("\"algorithm\": \"fcfs\""), std::string::npos);
    EXPECT_NE(json_content.find("\"total_processes\": 1"), std::string::npos);
    EXPECT_NE(json_content.find("\"jains_fairness_index\":"), std::string::npos);
    
    // Cleanup temporary files
    std::remove(csv_file.c_str());
    std::remove(json_file.c_str());
}

// =========================================================================
// 5. Comparison Mode All Algorithms Integration Test
// =========================================================================

TEST(ComparisonTest, RunAllAlgorithmsOnSharedWorkload) {
    std::vector<std::unique_ptr<Process>> base;
    base.push_back(std::make_unique<Process>(1, "P1", 0, 8, 3));
    base.push_back(std::make_unique<Process>(2, "P2", 1, 4, 1));
    base.push_back(std::make_unique<Process>(3, "P3", 2, 9, 4));
    base.push_back(std::make_unique<Process>(4, "P4", 3, 5, 2));
    
    std::vector<std::unique_ptr<IScheduler>> schedulers;
    schedulers.push_back(std::make_unique<FCFSScheduler>());
    schedulers.push_back(std::make_unique<SJFScheduler>());
    schedulers.push_back(std::make_unique<SRTFScheduler>());
    schedulers.push_back(std::make_unique<RoundRobinScheduler>(4));
    schedulers.push_back(std::make_unique<PriorityScheduler>(true, 10));
    schedulers.push_back(std::make_unique<PriorityScheduler>(false, 10));
    schedulers.push_back(std::make_unique<MLFQScheduler>(std::vector<uint32_t>{2, 4, 8}, 50));
    
    for (auto& sched : schedulers) {
        auto workload = WorkloadLoader::clone_workload(base);
        std::vector<Core> cores;
        cores.emplace_back(0, 1.0);
        cores.emplace_back(1, 1.5);
        
        auto lb = std::make_unique<GlobalQueueBalancer>(1);
        SimulationEngine engine(std::move(sched), std::move(cores), 0, false, std::move(lb));
        engine.run(workload);
        
        for (const auto& proc : workload) {
            EXPECT_TRUE(proc->is_complete());
        }
        EXPECT_GT(engine.current_time(), 0u);
    }
}
