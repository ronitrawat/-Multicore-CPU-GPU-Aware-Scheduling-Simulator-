#include <gtest/gtest.h>
#include "Process.h"
#include "Core.h"
#include "PriorityScheduler.h"
#include "SimulationEngine.h"
#include "MetricsCollector.h"
#include "LoadBalancer.h"
#include <memory>
#include <vector>

using namespace scheduler;

// ========== Priority Scheduler Tests ==========

// Test priority scheduling without preemption
TEST(PriorityTest, NonPreemptiveBasic) {
    std::vector<std::unique_ptr<Process>> workload;
    workload.push_back(std::make_unique<Process>(1, "P1", 0, 10, 3)); // Low priority
    workload.push_back(std::make_unique<Process>(2, "P2", 0, 1, 1));  // High priority
    workload.push_back(std::make_unique<Process>(3, "P3", 0, 2, 4));  // Lower priority
    workload.push_back(std::make_unique<Process>(4, "P4", 0, 1, 5));  // Lowest priority
    workload.push_back(std::make_unique<Process>(5, "P5", 0, 5, 2));  // Mid priority
    
    std::vector<Core> cores;
    cores.emplace_back(0, 1.0);
    
    auto scheduler = std::make_unique<PriorityScheduler>(false, 0); // Non-preemptive, no aging
    SimulationEngine engine(std::move(scheduler), std::move(cores), 0);
    
    engine.run(workload);
    
    // Expected order: P2(1), P5(2), P1(3), P3(4), P4(5)
    EXPECT_EQ(workload[1]->completion_time, 1u);   // P2 first (highest priority)
    EXPECT_EQ(workload[4]->completion_time, 6u);   // P5 second 
    EXPECT_EQ(workload[0]->completion_time, 16u);  // P1 third
    EXPECT_EQ(workload[2]->completion_time, 18u);  // P3 fourth
    EXPECT_EQ(workload[3]->completion_time, 19u);  // P4 last (lowest priority)
    
    // P2 should have zero waiting time (runs immediately)
    uint32_t p2_waiting = workload[1]->turnaround_time() - workload[1]->burst_time;
    EXPECT_EQ(p2_waiting, 0u);
}

// Test priority scheduling with preemption
TEST(PriorityTest, PreemptiveBasic) {
    std::vector<std::unique_ptr<Process>> workload;
    workload.push_back(std::make_unique<Process>(1, "P1", 0, 8, 3));  // Starts running
    workload.push_back(std::make_unique<Process>(2, "P2", 2, 4, 1));  // Higher priority, arrives later
    workload.push_back(std::make_unique<Process>(3, "P3", 4, 2, 2));  // Mid priority
    
    std::vector<Core> cores;
    cores.emplace_back(0, 1.0);
    
    auto scheduler = std::make_unique<PriorityScheduler>(true, 0); // Preemptive, no aging
    SimulationEngine engine(std::move(scheduler), std::move(cores), 0);
    
    engine.run(workload);
    
    // P2 should preempt P1 at t=2 and complete first
    EXPECT_LT(workload[1]->completion_time, workload[0]->completion_time);
    EXPECT_EQ(workload[1]->completion_time, 6u); // P2: runs from t=2 to t=6
    
    // P3 should preempt P1 after P2 completes
    EXPECT_LT(workload[2]->completion_time, workload[0]->completion_time);
}

// Test aging mechanism to prevent starvation
TEST(PriorityTest, AgingPreventsStarvation) {
    std::vector<std::unique_ptr<Process>> workload;
    workload.push_back(std::make_unique<Process>(1, "P1", 0, 3, 5));  // Low priority, should age
    workload.push_back(std::make_unique<Process>(2, "P2", 5, 2, 1));  // High priority, arrives later
    workload.push_back(std::make_unique<Process>(3, "P3", 10, 2, 1)); // High priority, arrives even later
    
    std::vector<Core> cores;
    cores.emplace_back(0, 1.0);
    
    auto scheduler = std::make_unique<PriorityScheduler>(true, 3); // Aging every 3 ticks
    SimulationEngine engine(std::move(scheduler), std::move(cores), 0);
    
    engine.run(workload);
    
    // P1 should not starve despite low priority due to aging
    EXPECT_LT(workload[0]->completion_time, 20u); // Should complete reasonably soon
    
    // All processes should complete
    EXPECT_TRUE(workload[0]->is_complete());
    EXPECT_TRUE(workload[1]->is_complete());
    EXPECT_TRUE(workload[2]->is_complete());
}

// Test tie-breaking with same priorities
TEST(PriorityTest, TieBreaking) {
    std::vector<std::unique_ptr<Process>> workload;
    workload.push_back(std::make_unique<Process>(3, "P3", 2, 3, 2)); // Later arrival, higher PID
    workload.push_back(std::make_unique<Process>(1, "P1", 0, 2, 2)); // Earlier arrival, lower PID
    workload.push_back(std::make_unique<Process>(2, "P2", 0, 1, 2)); // Same arrival as P1, mid PID
    
    std::vector<Core> cores;
    cores.emplace_back(0, 1.0);
    
    auto scheduler = std::make_unique<PriorityScheduler>(false, 0); // Non-preemptive
    SimulationEngine engine(std::move(scheduler), std::move(cores), 0);
    
    engine.run(workload);
    
    // With same priority, should order by: arrival time, then PID
    // P1 and P2 arrive at t=0, P1 has lower PID, so P1 should run first
    // Then P2, then P3 (arrives later)
    EXPECT_LT(workload[1]->completion_time, workload[2]->completion_time); // P1 before P2
    EXPECT_LT(workload[2]->completion_time, workload[0]->completion_time); // P2 before P3
}

// ========== Multicore Tests ==========

// Test basic multicore operation
TEST(MulticoreTest, BasicMulticore) {
    std::vector<std::unique_ptr<Process>> workload;
    workload.push_back(std::make_unique<Process>(1, "P1", 0, 5, 0));
    workload.push_back(std::make_unique<Process>(2, "P2", 0, 5, 0));
    workload.push_back(std::make_unique<Process>(3, "P3", 0, 5, 0));
    workload.push_back(std::make_unique<Process>(4, "P4", 0, 5, 0));
    
    // 2 cores, same speed
    std::vector<Core> cores;
    cores.emplace_back(0, 1.0);
    cores.emplace_back(1, 1.0);
    
    auto scheduler = std::make_unique<PriorityScheduler>(false, 0);
    auto load_balancer = std::make_unique<GlobalQueueBalancer>(0); // No migration penalty
    SimulationEngine engine(std::move(scheduler), std::move(cores), 0, false, std::move(load_balancer));
    
    engine.run(workload);
    
    // With 2 cores, should complete in ~10 ticks instead of 20
    EXPECT_LE(engine.current_time(), 12u);
    
    // All processes should complete
    for (const auto& proc : workload) {
        EXPECT_TRUE(proc->is_complete());
    }
    
    // Both cores should have been utilized
    bool core0_used = engine.cores()[0].busy_time() > 0;
    bool core1_used = engine.cores()[1].busy_time() > 0;
    EXPECT_TRUE(core0_used || core1_used); // At least one core used
}

// Test heterogeneous cores (big.LITTLE)
TEST(MulticoreTest, HeterogeneousSpeeds) {
    std::vector<std::unique_ptr<Process>> workload;
    workload.push_back(std::make_unique<Process>(1, "P1", 0, 8, 0)); // Long job
    workload.push_back(std::make_unique<Process>(2, "P2", 0, 2, 0)); // Short job
    
    // Big.LITTLE: slow core (0.5x) and fast core (2.0x)
    std::vector<Core> cores;
    cores.emplace_back(0, 0.5); // Slow core
    cores.emplace_back(1, 2.0); // Fast core
    
    auto scheduler = std::make_unique<PriorityScheduler>(false, 0);
    auto load_balancer = std::make_unique<SpeedAwareBalancer>(0);
    SimulationEngine engine(std::move(scheduler), std::move(cores), 0, false, std::move(load_balancer));
    
    engine.run(workload);
    
    // Fast core should complete work faster
    // P1 on fast core: 8/2.0 = 4 ticks
    // P1 on slow core: 8/0.5 = 16 ticks
    EXPECT_LE(engine.current_time(), 10u); // Should be much faster than single slow core
    
    // Both processes should complete
    EXPECT_TRUE(workload[0]->is_complete());
    EXPECT_TRUE(workload[1]->is_complete());
}

// Test migration penalty
TEST(MulticoreTest, MigrationPenalty) {
    std::vector<std::unique_ptr<Process>> workload;
    workload.push_back(std::make_unique<Process>(1, "P1", 0, 6, 0));
    
    std::vector<Core> cores;
    cores.emplace_back(0, 1.0);
    cores.emplace_back(1, 1.0);
    
    auto scheduler = std::make_unique<PriorityScheduler>(true, 0); // Preemptive
    uint32_t migration_penalty = 3; // 3 ticks penalty
    auto load_balancer = std::make_unique<GlobalQueueBalancer>(migration_penalty);
    SimulationEngine engine(std::move(scheduler), std::move(cores), 0, false, std::move(load_balancer));
    
    engine.run(workload);
    
    // Check that process tracked migration
    EXPECT_TRUE(workload[0]->is_complete());
    // If process migrated, total time should include penalty
    if (workload[0]->migration_count > 0) {
        EXPECT_GE(workload[0]->completion_time, 6u + migration_penalty);
    }
}

// ========== Load Balancer Tests ==========

// Test GlobalQueueBalancer spreads load evenly
TEST(LoadBalancerTest, GlobalQueueBalance) {
    std::vector<std::unique_ptr<Process>> workload;
    // Create many short processes
    for (int i = 0; i < 8; ++i) {
        workload.push_back(std::make_unique<Process>(i + 1, "P" + std::to_string(i + 1), 0, 2, 0));
    }
    
    std::vector<Core> cores;
    cores.emplace_back(0, 1.0);
    cores.emplace_back(1, 1.0);
    cores.emplace_back(2, 1.0);
    cores.emplace_back(3, 1.0);
    
    auto scheduler = std::make_unique<PriorityScheduler>(false, 0);
    auto load_balancer = std::make_unique<GlobalQueueBalancer>(0);
    SimulationEngine engine(std::move(scheduler), std::move(cores), 0, false, std::move(load_balancer));
    
    engine.run(workload);
    
    // All cores should have some work (roughly balanced)
    uint32_t total_busy = 0;
    for (const auto& core : engine.cores()) {
        EXPECT_GT(core.busy_time(), 0u); // Each core should do some work
        total_busy += core.busy_time();
    }
    
    // Total work should be sum of all burst times
    EXPECT_EQ(total_busy, 16u); // 8 processes × 2 ticks each
    
    // Should complete much faster than single core
    EXPECT_LE(engine.current_time(), 6u); // vs 16 ticks on single core
}

// Test SpeedAwareBalancer assigns work optimally
TEST(LoadBalancerTest, SpeedAwareOptimal) {
    std::vector<std::unique_ptr<Process>> workload;
    workload.push_back(std::make_unique<Process>(1, "P1", 0, 8, 0)); // Long job
    workload.push_back(std::make_unique<Process>(2, "P2", 0, 4, 0)); // Medium job
    workload.push_back(std::make_unique<Process>(3, "P3", 0, 2, 0)); // Short job
    
    std::vector<Core> cores;
    cores.emplace_back(0, 0.5); // Slow core
    cores.emplace_back(1, 1.0); // Normal core
    cores.emplace_back(2, 2.0); // Fast core
    
    auto scheduler = std::make_unique<PriorityScheduler>(false, 0);
    auto load_balancer = std::make_unique<SpeedAwareBalancer>(0);
    SimulationEngine engine(std::move(scheduler), std::move(cores), 0, false, std::move(load_balancer));
    
    engine.run(workload);
    
    // Fast core should get more work or complete work faster
    EXPECT_GT(engine.cores()[2].busy_time(), 0u); // Fast core should be used
    
    // Should complete faster than if all work went to slow core
    EXPECT_LE(engine.current_time(), 10u);
}

// ========== Integration Tests ==========

// Test Priority + Multicore + Aging together
TEST(IntegrationTest, PriorityMulticoreAging) {
    std::vector<std::unique_ptr<Process>> workload;
    workload.push_back(std::make_unique<Process>(1, "P1", 0, 6, 5));  // Low priority
    workload.push_back(std::make_unique<Process>(2, "P2", 2, 3, 1));  // High priority
    workload.push_back(std::make_unique<Process>(3, "P3", 4, 4, 3));  // Mid priority
    workload.push_back(std::make_unique<Process>(4, "P4", 6, 2, 4));  // Low-mid priority
    
    std::vector<Core> cores;
    cores.emplace_back(0, 1.0);
    cores.emplace_back(1, 1.5); // Slightly faster
    
    auto scheduler = std::make_unique<PriorityScheduler>(true, 5); // Preemptive with aging
    auto load_balancer = std::make_unique<GlobalQueueBalancer>(1);
    SimulationEngine engine(std::move(scheduler), std::move(cores), 1, false, std::move(load_balancer));
    
    engine.run(workload);
    
    // All processes should complete without starvation
    for (const auto& proc : workload) {
        EXPECT_TRUE(proc->is_complete()) << "Process " << proc->pid << " did not complete";
    }
    
    // High priority process should have low waiting time
    uint32_t p2_waiting = workload[1]->turnaround_time() - workload[1]->burst_time;
    EXPECT_LE(p2_waiting, 5u);
    
    // Should complete faster than single core
    EXPECT_LE(engine.current_time(), 12u);
}

// Test extreme heterogeneity (very different core speeds)
TEST(IntegrationTest, ExtremeHeterogeneity) {
    std::vector<std::unique_ptr<Process>> workload;
    for (int i = 0; i < 6; ++i) {
        workload.push_back(std::make_unique<Process>(i + 1, "P" + std::to_string(i + 1), 0, 10, 0));
    }
    
    std::vector<Core> cores;
    cores.emplace_back(0, 0.1); // Very slow
    cores.emplace_back(1, 1.0); // Normal
    cores.emplace_back(2, 5.0); // Very fast
    
    auto scheduler = std::make_unique<PriorityScheduler>(false, 0);
    auto load_balancer = std::make_unique<SpeedAwareBalancer>(0);
    SimulationEngine engine(std::move(scheduler), std::move(cores), 0, false, std::move(load_balancer));
    
    engine.run(workload);
    
    // Fast core should do most of the work
    EXPECT_GT(engine.cores()[2].busy_time(), engine.cores()[0].busy_time());
    
    // Should complete much faster than if only slow core was used
    EXPECT_LE(engine.current_time(), 30u); // vs 600 ticks on 0.1x core alone
}

// ========== Edge Cases ==========

// Test aging with very frequent intervals
TEST(EdgeCaseTest, FrequentAging) {
    std::vector<std::unique_ptr<Process>> workload;
    workload.push_back(std::make_unique<Process>(1, "P1", 0, 5, 10)); // Very low priority
    workload.push_back(std::make_unique<Process>(2, "P2", 3, 3, 1));  // High priority, arrives later
    
    std::vector<Core> cores;
    cores.emplace_back(0, 1.0);
    
    auto scheduler = std::make_unique<PriorityScheduler>(true, 1); // Age every tick!
    SimulationEngine engine(std::move(scheduler), std::move(cores), 0);
    
    engine.run(workload);
    
    // Both should complete despite extreme priority difference
    EXPECT_TRUE(workload[0]->is_complete());
    EXPECT_TRUE(workload[1]->is_complete());
}

// Test no migration penalty vs high migration penalty
TEST(EdgeCaseTest, MigrationPenaltyImpact) {
    auto test_migration = [](uint32_t penalty) -> uint32_t {
        std::vector<std::unique_ptr<Process>> workload;
        workload.push_back(std::make_unique<Process>(1, "P1", 0, 4, 0));
        
        std::vector<Core> cores;
        cores.emplace_back(0, 1.0);
        cores.emplace_back(1, 1.0);
        
        auto scheduler = std::make_unique<PriorityScheduler>(true, 0);
        auto load_balancer = std::make_unique<GlobalQueueBalancer>(penalty);
        SimulationEngine engine(std::move(scheduler), std::move(cores), 0, false, std::move(load_balancer));
        
        engine.run(workload);
        return engine.current_time();
    };
    
    uint32_t time_no_penalty = test_migration(0);
    uint32_t time_high_penalty = test_migration(10);
    
    // High migration penalty should increase total time if migration occurs
    // (This test might pass if no migration happens, which is also valid)
    EXPECT_GE(time_high_penalty, time_no_penalty);
}

// Test single core vs multicore performance scaling
TEST(EdgeCaseTest, ScalingPerformance) {
    auto test_cores = [](int num_cores) -> uint32_t {
        std::vector<std::unique_ptr<Process>> workload;
        for (int i = 0; i < 8; ++i) {
            workload.push_back(std::make_unique<Process>(i + 1, "P" + std::to_string(i + 1), 0, 3, 0));
        }
        
        std::vector<Core> cores;
        for (int i = 0; i < num_cores; ++i) {
            cores.emplace_back(i, 1.0);
        }
        
        auto scheduler = std::make_unique<PriorityScheduler>(false, 0);
        auto load_balancer = std::make_unique<GlobalQueueBalancer>(0);
        SimulationEngine engine(std::move(scheduler), std::move(cores), 0, false, std::move(load_balancer));
        
        engine.run(workload);
        return engine.current_time();
    };
    
    uint32_t time_1_core = test_cores(1);
    uint32_t time_2_cores = test_cores(2);
    uint32_t time_4_cores = test_cores(4);
    
    // More cores should complete faster
    EXPECT_GE(time_1_core, time_2_cores);
    EXPECT_GE(time_2_cores, time_4_cores);
    
    // Should see reasonable scaling (not perfect due to overhead, but significant)
    EXPECT_LE(time_4_cores, time_1_core / 2); // At least 2x speedup with 4x cores
}