#include <gtest/gtest.h>
#include "Process.h"
#include "Core.h"
#include "SJFScheduler.h"
#include "SRTFScheduler.h"
#include "RoundRobinScheduler.h"
#include "SimulationEngine.h"
#include "MetricsCollector.h"
#include <memory>
#include <vector>

using namespace scheduler;

// ========== SJF Tests ==========

// Test SJF with textbook example
// P1: arrival=0, burst=6
// P2: arrival=2, burst=8
// P3: arrival=4, burst=7
// P4: arrival=5, burst=3
// Expected order: P1, P4, P3, P2 (shortest first after arrival)
TEST(SJFTest, TextbookExample) {
    std::vector<std::unique_ptr<Process>> workload;
    workload.push_back(std::make_unique<Process>(1, "P1", 0, 6, 0));
    workload.push_back(std::make_unique<Process>(2, "P2", 2, 8, 0));
    workload.push_back(std::make_unique<Process>(3, "P3", 4, 7, 0));
    workload.push_back(std::make_unique<Process>(4, "P4", 5, 3, 0));
    
    std::vector<Core> cores;
    cores.emplace_back(0, 1.0);
    
    auto scheduler = std::make_unique<SJFScheduler>();
    SimulationEngine engine(std::move(scheduler), std::move(cores), 0);
    
    engine.run(workload);
    
    // P1: completes at 6, waiting=0
    EXPECT_EQ(workload[0]->completion_time, 6u);
    EXPECT_EQ(workload[0]->turnaround_time() - workload[0]->burst_time, 0u);
    
    // P4: shortest remaining (3), completes at 9
    EXPECT_EQ(workload[3]->completion_time, 9u);
    
    // P3: next shortest (7), completes at 16
    EXPECT_EQ(workload[2]->completion_time, 16u);
    
    // P2: longest (8), completes last at 24
    EXPECT_EQ(workload[1]->completion_time, 24u);
    
    // Average waiting time should be better than FCFS
    MetricsCollector metrics;
    for (const auto& proc : workload) {
        metrics.collect_process(*proc);
    }
    metrics.collect_cores(engine.cores());
    metrics.compute_global_metrics(engine.current_time());
    
    EXPECT_DOUBLE_EQ(metrics.global_metrics().avg_waiting_time, 5.0);
}

// Test SJF with simultaneous arrivals
TEST(SJFTest, SimultaneousArrivals) {
    std::vector<std::unique_ptr<Process>> workload;
    workload.push_back(std::make_unique<Process>(1, "P1", 0, 10, 0));
    workload.push_back(std::make_unique<Process>(2, "P2", 0, 5, 0));
    workload.push_back(std::make_unique<Process>(3, "P3", 0, 2, 0));
    
    std::vector<Core> cores;
    cores.emplace_back(0, 1.0);
    
    auto scheduler = std::make_unique<SJFScheduler>();
    SimulationEngine engine(std::move(scheduler), std::move(cores), 0);
    
    engine.run(workload);
    
    // Should run in order: P3(2), P2(5), P1(10)
    EXPECT_EQ(workload[2]->completion_time, 2u);   // P3 first
    EXPECT_EQ(workload[1]->completion_time, 7u);   // P2 second
    EXPECT_EQ(workload[0]->completion_time, 17u);  // P1 last
}

// ========== SRTF Tests ==========

// Test SRTF preemption
// P1 starts, P2 arrives with shorter remaining time, should preempt
TEST(SRTFTest, PreemptionBasic) {
    std::vector<std::unique_ptr<Process>> workload;
    workload.push_back(std::make_unique<Process>(1, "P1", 0, 8, 0));
    workload.push_back(std::make_unique<Process>(2, "P2", 1, 4, 0));
    workload.push_back(std::make_unique<Process>(3, "P3", 2, 9, 0));
    workload.push_back(std::make_unique<Process>(4, "P4", 3, 5, 0));
    
    std::vector<Core> cores;
    cores.emplace_back(0, 1.0);
    
    auto scheduler = std::make_unique<SRTFScheduler>();
    SimulationEngine engine(std::move(scheduler), std::move(cores), 0);
    
    engine.run(workload);
    
    // P2 should complete first (arrives at 1, runs to completion)
    EXPECT_EQ(workload[1]->completion_time, 5u);
    
    // P4 should complete next (shortest remaining after P2)
    EXPECT_EQ(workload[3]->completion_time, 10u);
    
    // P1 should resume and complete
    EXPECT_EQ(workload[0]->completion_time, 17u);
    
    // P3 should complete last (longest)
    EXPECT_EQ(workload[2]->completion_time, 26u);
}

// Test SRTF optimal average waiting time
TEST(SRTFTest, OptimalWaitingTime) {
    std::vector<std::unique_ptr<Process>> workload;
    workload.push_back(std::make_unique<Process>(1, "P1", 0, 6, 0));
    workload.push_back(std::make_unique<Process>(2, "P2", 2, 8, 0));
    workload.push_back(std::make_unique<Process>(3, "P3", 4, 7, 0));
    workload.push_back(std::make_unique<Process>(4, "P4", 5, 3, 0));
    
    std::vector<Core> cores;
    cores.emplace_back(0, 1.0);
    
    auto scheduler = std::make_unique<SRTFScheduler>();
    SimulationEngine engine(std::move(scheduler), std::move(cores), 0);
    
    engine.run(workload);
    
    MetricsCollector metrics;
    for (const auto& proc : workload) {
        metrics.collect_process(*proc);
    }
    metrics.collect_cores(engine.cores());
    metrics.compute_global_metrics(engine.current_time());
    
    // SRTF should give optimal (or very good) average waiting time
    EXPECT_LE(metrics.global_metrics().avg_waiting_time, 5.5);
}

// ========== Round Robin Tests ==========

// Test RR with quantum = 2
TEST(RoundRobinTest, Quantum2) {
    std::vector<std::unique_ptr<Process>> workload;
    workload.push_back(std::make_unique<Process>(1, "P1", 0, 5, 0));
    workload.push_back(std::make_unique<Process>(2, "P2", 0, 3, 0));
    workload.push_back(std::make_unique<Process>(3, "P3", 0, 4, 0));
    
    std::vector<Core> cores;
    cores.emplace_back(0, 1.0);
    
    auto scheduler = std::make_unique<RoundRobinScheduler>(2);
    SimulationEngine engine(std::move(scheduler), std::move(cores), 0);
    
    engine.run(workload);
    
    // All should complete
    EXPECT_TRUE(workload[0]->is_complete());
    EXPECT_TRUE(workload[1]->is_complete());
    EXPECT_TRUE(workload[2]->is_complete());
    
    // Total time should be sum of bursts
    EXPECT_EQ(engine.current_time(), 11u);
    
    // Response times should be low (everyone gets CPU quickly)
    EXPECT_LE(workload[0]->response_time(), 2u);
    EXPECT_LE(workload[1]->response_time(), 2u);
    EXPECT_LE(workload[2]->response_time(), 4u);
}

// Test RR with large quantum (should behave like FCFS)
TEST(RoundRobinTest, LargeQuantum) {
    std::vector<std::unique_ptr<Process>> workload;
    workload.push_back(std::make_unique<Process>(1, "P1", 0, 5, 0));
    workload.push_back(std::make_unique<Process>(2, "P2", 0, 3, 0));
    workload.push_back(std::make_unique<Process>(3, "P3", 0, 4, 0));
    
    std::vector<Core> cores;
    cores.emplace_back(0, 1.0);
    
    // Quantum larger than any burst
    auto scheduler = std::make_unique<RoundRobinScheduler>(100);
    SimulationEngine engine(std::move(scheduler), std::move(cores), 0);
    
    engine.run(workload);
    
    // Should behave like FCFS (no preemption)
    EXPECT_EQ(workload[0]->completion_time, 5u);
    EXPECT_EQ(workload[1]->completion_time, 8u);
    EXPECT_EQ(workload[2]->completion_time, 12u);
}

// Test RR fairness with different burst times
TEST(RoundRobinTest, Fairness) {
    std::vector<std::unique_ptr<Process>> workload;
    workload.push_back(std::make_unique<Process>(1, "P1", 0, 10, 0));
    workload.push_back(std::make_unique<Process>(2, "P2", 0, 2, 0));
    
    std::vector<Core> cores;
    cores.emplace_back(0, 1.0);
    
    auto scheduler = std::make_unique<RoundRobinScheduler>(1);
    SimulationEngine engine(std::move(scheduler), std::move(cores), 0);
    
    engine.run(workload);
    
    // Short process should complete much sooner than long one
    EXPECT_LT(workload[1]->completion_time, workload[0]->completion_time);
    
    // Short process should not wait excessively
    uint32_t p2_waiting = workload[1]->turnaround_time() - workload[1]->burst_time;
    EXPECT_LE(p2_waiting, 3u);
}

// ========== Comparison Tests ==========

// Compare all algorithms on same workload
TEST(ComparisonTest, AllAlgorithms) {
    // Define a test workload
    auto create_workload = []() {
        std::vector<std::unique_ptr<Process>> workload;
        workload.push_back(std::make_unique<Process>(1, "P1", 0, 6, 0));
        workload.push_back(std::make_unique<Process>(2, "P2", 2, 8, 0));
        workload.push_back(std::make_unique<Process>(3, "P3", 4, 7, 0));
        workload.push_back(std::make_unique<Process>(4, "P4", 5, 3, 0));
        return workload;
    };
    
    // Test SJF
    auto workload_sjf = create_workload();
    std::vector<Core> cores_sjf;
    cores_sjf.emplace_back(0, 1.0);
    auto scheduler_sjf = std::make_unique<SJFScheduler>();
    SimulationEngine engine_sjf(std::move(scheduler_sjf), std::move(cores_sjf), 0);
    engine_sjf.run(workload_sjf);
    
    MetricsCollector metrics_sjf;
    for (const auto& proc : workload_sjf) {
        metrics_sjf.collect_process(*proc);
    }
    metrics_sjf.collect_cores(engine_sjf.cores());
    metrics_sjf.compute_global_metrics(engine_sjf.current_time());
    
    // Test SRTF
    auto workload_srtf = create_workload();
    std::vector<Core> cores_srtf;
    cores_srtf.emplace_back(0, 1.0);
    auto scheduler_srtf = std::make_unique<SRTFScheduler>();
    SimulationEngine engine_srtf(std::move(scheduler_srtf), std::move(cores_srtf), 0);
    engine_srtf.run(workload_srtf);
    
    MetricsCollector metrics_srtf;
    for (const auto& proc : workload_srtf) {
        metrics_srtf.collect_process(*proc);
    }
    metrics_srtf.collect_cores(engine_srtf.cores());
    metrics_srtf.compute_global_metrics(engine_srtf.current_time());
    
    // Test RR
    auto workload_rr = create_workload();
    std::vector<Core> cores_rr;
    cores_rr.emplace_back(0, 1.0);
    auto scheduler_rr = std::make_unique<RoundRobinScheduler>(4);
    SimulationEngine engine_rr(std::move(scheduler_rr), std::move(cores_rr), 0);
    engine_rr.run(workload_rr);
    
    MetricsCollector metrics_rr;
    for (const auto& proc : workload_rr) {
        metrics_rr.collect_process(*proc);
    }
    metrics_rr.collect_cores(engine_rr.cores());
    metrics_rr.compute_global_metrics(engine_rr.current_time());
    
    // All should complete in same total time (no idle time)
    EXPECT_EQ(engine_sjf.current_time(), 23u);
    EXPECT_EQ(engine_srtf.current_time(), 23u);
    EXPECT_EQ(engine_rr.current_time(), 23u);
    
    // SRTF should have best (or equal to SJF) average waiting time
    EXPECT_LE(metrics_srtf.global_metrics().avg_waiting_time,
              metrics_sjf.global_metrics().avg_waiting_time + 0.01);
    
    // RR should have better response time than SJF
    EXPECT_LT(metrics_rr.global_metrics().avg_response_time,
              metrics_sjf.global_metrics().avg_response_time);
}

// ========== Context Switch Tests ==========

// Test RR with context switch overhead
TEST(ContextSwitchTest, RoundRobinOverhead) {
    std::vector<std::unique_ptr<Process>> workload;
    workload.push_back(std::make_unique<Process>(1, "P1", 0, 4, 0));
    workload.push_back(std::make_unique<Process>(2, "P2", 0, 4, 0));
    
    std::vector<Core> cores;
    cores.emplace_back(0, 1.0);
    
    auto scheduler = std::make_unique<RoundRobinScheduler>(2);
    uint32_t cs_cost = 1;
    SimulationEngine engine(std::move(scheduler), std::move(cores), cs_cost);
    
    engine.run(workload);
    
    // Should have multiple context switches
    EXPECT_GE(engine.cores()[0].context_switch_count(), 2u);
    
    // Overhead should be reflected in metrics
    EXPECT_GT(engine.cores()[0].context_switch_overhead_time(), 0u);
    
    // Utilization should be less than 100% due to overhead
    MetricsCollector metrics;
    for (const auto& proc : workload) {
        metrics.collect_process(*proc);
    }
    metrics.collect_cores(engine.cores());
    metrics.compute_global_metrics(engine.current_time());
    
    EXPECT_LT(metrics.global_metrics().cpu_utilization, 1.0);
}

// ========== Edge Cases ==========

// Test single process (all algorithms should behave identically)
TEST(EdgeCaseTest, SingleProcess) {
    auto test_single = [](std::unique_ptr<IScheduler> scheduler) {
        std::vector<std::unique_ptr<Process>> workload;
        workload.push_back(std::make_unique<Process>(1, "P1", 0, 10, 0));
        
        std::vector<Core> cores;
        cores.emplace_back(0, 1.0);
        
        SimulationEngine engine(std::move(scheduler), std::move(cores), 0);
        engine.run(workload);
        
        EXPECT_EQ(workload[0]->completion_time, 10u);
        EXPECT_EQ(workload[0]->turnaround_time(), 10u);
        EXPECT_EQ(workload[0]->turnaround_time() - workload[0]->burst_time, 0u);
    };
    
    test_single(std::make_unique<SJFScheduler>());
    test_single(std::make_unique<SRTFScheduler>());
    test_single(std::make_unique<RoundRobinScheduler>(4));
}

// Test empty arrivals at start (idle time)
TEST(EdgeCaseTest, DelayedArrivals) {
    std::vector<std::unique_ptr<Process>> workload;
    workload.push_back(std::make_unique<Process>(1, "P1", 10, 5, 0));
    workload.push_back(std::make_unique<Process>(2, "P2", 15, 3, 0));
    
    std::vector<Core> cores;
    cores.emplace_back(0, 1.0);
    
    auto scheduler = std::make_unique<SJFScheduler>();
    SimulationEngine engine(std::move(scheduler), std::move(cores), 0);
    
    engine.run(workload);
    
    // Should have idle time at the beginning
    EXPECT_GT(engine.cores()[0].idle_time(), 0u);
    
    // Processes should complete correctly
    EXPECT_EQ(workload[0]->completion_time, 15u);
    EXPECT_EQ(workload[1]->completion_time, 18u);
}
