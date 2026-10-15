#include <gtest/gtest.h>
#include "Process.h"
#include "Core.h"
#include "FCFSScheduler.h"
#include "SimulationEngine.h"
#include "MetricsCollector.h"
#include <memory>
#include <vector>

using namespace scheduler;

// Test basic FCFS with textbook example
// P1: arrival=0, burst=24
// P2: arrival=0, burst=3  
// P3: arrival=0, burst=3
// Expected order: P1, P2, P3 (by PID since all arrive at same time)
// Expected waiting times: P1=0, P2=24, P3=27
// Expected turnaround: P1=24, P2=27, P3=30
TEST(FCFSTest, TextbookExample) {
    std::vector<std::unique_ptr<Process>> workload;
    workload.push_back(std::make_unique<Process>(1, "P1", 0, 24, 0));
    workload.push_back(std::make_unique<Process>(2, "P2", 0, 3, 0));
    workload.push_back(std::make_unique<Process>(3, "P3", 0, 3, 0));
    
    std::vector<Core> cores;
    cores.emplace_back(0, 1.0);
    
    auto scheduler = std::make_unique<FCFSScheduler>();
    SimulationEngine engine(std::move(scheduler), std::move(cores), 0);
    
    engine.run(workload);
    
    // Verify completion times
    EXPECT_EQ(workload[0]->completion_time, 24u);
    EXPECT_EQ(workload[1]->completion_time, 27u);
    EXPECT_EQ(workload[2]->completion_time, 30u);
    
    // Verify waiting times
    EXPECT_EQ(workload[0]->waiting_time, 0u);
    EXPECT_EQ(workload[1]->waiting_time, 24u);
    EXPECT_EQ(workload[2]->waiting_time, 27u);
    
    // Verify turnaround times
    EXPECT_EQ(workload[0]->turnaround_time(), 24u);
    EXPECT_EQ(workload[1]->turnaround_time(), 27u);
    EXPECT_EQ(workload[2]->turnaround_time(), 30u);
    
    // Verify response times (should equal waiting time for non-preemptive)
    EXPECT_EQ(workload[0]->response_time(), 0u);
    EXPECT_EQ(workload[1]->response_time(), 24u);
    EXPECT_EQ(workload[2]->response_time(), 27u);
}

// Test FCFS with different arrival times
// P1: arrival=0, burst=8
// P2: arrival=1, burst=4
// P3: arrival=2, burst=9
// P4: arrival=3, burst=5
// Expected order: P1, P2, P3, P4
TEST(FCFSTest, DifferentArrivals) {
    std::vector<std::unique_ptr<Process>> workload;
    workload.push_back(std::make_unique<Process>(1, "P1", 0, 8, 0));
    workload.push_back(std::make_unique<Process>(2, "P2", 1, 4, 0));
    workload.push_back(std::make_unique<Process>(3, "P3", 2, 9, 0));
    workload.push_back(std::make_unique<Process>(4, "P4", 3, 5, 0));
    
    std::vector<Core> cores;
    cores.emplace_back(0, 1.0);
    
    auto scheduler = std::make_unique<FCFSScheduler>();
    SimulationEngine engine(std::move(scheduler), std::move(cores), 0);
    
    engine.run(workload);
    
    // P1: completes at 8, waiting=0, turnaround=8
    EXPECT_EQ(workload[0]->completion_time, 8u);
    EXPECT_EQ(workload[0]->waiting_time, 0u);
    
    // P2: completes at 12 (waits from 1 to 8 = 7), turnaround=12-1=11
    EXPECT_EQ(workload[1]->completion_time, 12u);
    EXPECT_EQ(workload[1]->waiting_time, 7u);
    EXPECT_EQ(workload[1]->turnaround_time(), 11u);
    
    // P3: completes at 21 (waits from 2 to 12 = 10), turnaround=21-2=19
    EXPECT_EQ(workload[2]->completion_time, 21u);
    EXPECT_EQ(workload[2]->waiting_time, 10u);
    EXPECT_EQ(workload[2]->turnaround_time(), 19u);
    
    // P4: completes at 26 (waits from 3 to 21 = 18), turnaround=26-3=23
    EXPECT_EQ(workload[3]->completion_time, 26u);
    EXPECT_EQ(workload[3]->waiting_time, 18u);
    EXPECT_EQ(workload[3]->turnaround_time(), 23u);
}

// Test with context switch overhead
TEST(FCFSTest, ContextSwitchOverhead) {
    std::vector<std::unique_ptr<Process>> workload;
    workload.push_back(std::make_unique<Process>(1, "P1", 0, 5, 0));
    workload.push_back(std::make_unique<Process>(2, "P2", 0, 3, 0));
    
    std::vector<Core> cores;
    cores.emplace_back(0, 1.0);
    
    auto scheduler = std::make_unique<FCFSScheduler>();
    uint32_t cs_cost = 1;  // 1 tick per context switch
    SimulationEngine engine(std::move(scheduler), std::move(cores), cs_cost);
    
    engine.run(workload);
    
    // P1: 1 (CS) + 5 (run) = completes at 6
    EXPECT_EQ(workload[0]->completion_time, 6u);
    
    // P2: waits until 6, then 1 (CS) + 3 (run) = completes at 10
    EXPECT_EQ(workload[1]->completion_time, 10u);
    
    // Core should have 2 context switches
    EXPECT_EQ(engine.cores()[0].context_switch_count(), 2u);
    EXPECT_EQ(engine.cores()[0].context_switch_overhead_time(), 2u);
}

// Test single process (edge case)
TEST(FCFSTest, SingleProcess) {
    std::vector<std::unique_ptr<Process>> workload;
    workload.push_back(std::make_unique<Process>(1, "P1", 0, 10, 0));
    
    std::vector<Core> cores;
    cores.emplace_back(0, 1.0);
    
    auto scheduler = std::make_unique<FCFSScheduler>();
    SimulationEngine engine(std::move(scheduler), std::move(cores), 0);
    
    engine.run(workload);
    
    EXPECT_EQ(workload[0]->completion_time, 10u);
    EXPECT_EQ(workload[0]->waiting_time, 0u);
    EXPECT_EQ(workload[0]->turnaround_time(), 10u);
}

// Test metrics collection
TEST(MetricsTest, BasicMetrics) {
    std::vector<std::unique_ptr<Process>> workload;
    workload.push_back(std::make_unique<Process>(1, "P1", 0, 24, 0));
    workload.push_back(std::make_unique<Process>(2, "P2", 0, 3, 0));
    workload.push_back(std::make_unique<Process>(3, "P3", 0, 3, 0));
    
    std::vector<Core> cores;
    cores.emplace_back(0, 1.0);
    
    auto scheduler = std::make_unique<FCFSScheduler>();
    SimulationEngine engine(std::move(scheduler), std::move(cores), 0);
    
    engine.run(workload);
    
    MetricsCollector metrics;
    for (const auto& proc : workload) {
        metrics.collect_process(*proc);
    }
    metrics.collect_cores(engine.cores());
    metrics.compute_global_metrics(engine.current_time());
    
    const auto& global = metrics.global_metrics();
    
    EXPECT_EQ(global.total_processes, 3u);
    EXPECT_EQ(global.total_time, 30u);
    
    // Average waiting time: (0 + 24 + 27) / 3 = 17
    EXPECT_DOUBLE_EQ(global.avg_waiting_time, 17.0);
    
    // Average turnaround: (24 + 27 + 30) / 3 = 27
    EXPECT_DOUBLE_EQ(global.avg_turnaround_time, 27.0);
    
    // CPU utilization: 30 busy / 30 total = 100%
    EXPECT_DOUBLE_EQ(global.cpu_utilization, 1.0);
    
    // Throughput: 3 processes / 30 ticks = 0.1
    EXPECT_DOUBLE_EQ(global.throughput, 0.1);
}
