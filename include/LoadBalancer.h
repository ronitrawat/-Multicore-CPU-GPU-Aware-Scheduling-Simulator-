#pragma once

#include <vector>
#include <cstdint>
#include <string>

namespace scheduler {

class Process;
class Core;
class IScheduler;

// Load balancing strategy interface
class ILoadBalancer {
public:
    virtual ~ILoadBalancer() = default;
    
    // Decide which core should run a given process
    // Returns core ID, or -1 if no suitable core
    virtual int assign_core(Process* proc, const std::vector<Core>& cores) = 0;
    
    // Perform load balancing (work stealing, migration, etc.)
    // Called periodically during simulation
    virtual void balance_load(std::vector<Core>& cores, IScheduler* scheduler) = 0;
    
    // Get migration penalty for moving process between cores
    virtual uint32_t get_migration_penalty() const = 0;
    
    virtual std::string name() const = 0;
};

// Global queue: All processes in single queue, assign to least loaded core
class GlobalQueueBalancer : public ILoadBalancer {
public:
    explicit GlobalQueueBalancer(uint32_t migration_penalty = 1);
    
    int assign_core(Process* proc, const std::vector<Core>& cores) override;
    void balance_load(std::vector<Core>& cores, IScheduler* scheduler) override;
    uint32_t get_migration_penalty() const override { return migration_penalty_; }
    std::string name() const override { return "GlobalQueue"; }

private:
    uint32_t migration_penalty_;
    
    // Find core with lowest load
    int find_least_loaded_core(const std::vector<Core>& cores) const;
};

// Per-core queues: Each core has its own queue, with work stealing
class PerCoreBalancer : public ILoadBalancer {
public:
    explicit PerCoreBalancer(uint32_t migration_penalty = 2, 
                            uint32_t steal_threshold = 2);
    
    int assign_core(Process* proc, const std::vector<Core>& cores) override;
    void balance_load(std::vector<Core>& cores, IScheduler* scheduler) override;
    uint32_t get_migration_penalty() const override { return migration_penalty_; }
    std::string name() const override { return "PerCore"; }

private:
    uint32_t migration_penalty_;
    uint32_t steal_threshold_;  // Min load difference to trigger stealing
    
    // Affinity-based assignment (prefer core with same speed, then least loaded)
    int find_affinity_core(Process* proc, const std::vector<Core>& cores) const;
};

// Speed-aware: Assign processes based on remaining work and core speed
class SpeedAwareBalancer : public ILoadBalancer {
public:
    explicit SpeedAwareBalancer(uint32_t migration_penalty = 1);
    
    int assign_core(Process* proc, const std::vector<Core>& cores) override;
    void balance_load(std::vector<Core>& cores, IScheduler* scheduler) override;
    uint32_t get_migration_penalty() const override { return migration_penalty_; }
    std::string name() const override { return "SpeedAware"; }

private:
    uint32_t migration_penalty_;
    
    // Find core that will complete process fastest (considering current load)
    int find_optimal_core(Process* proc, const std::vector<Core>& cores) const;
    
    // Estimate completion time on a given core
    double estimate_completion_time(Process* proc, const Core& core) const;
};

} // namespace scheduler