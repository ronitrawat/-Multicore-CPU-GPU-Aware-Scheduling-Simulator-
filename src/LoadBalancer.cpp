#include "LoadBalancer.h"
#include "Process.h"
#include "Core.h"
#include <algorithm>
#include <limits>

namespace scheduler {

// ========== Global Queue Balancer ==========

GlobalQueueBalancer::GlobalQueueBalancer(uint32_t migration_penalty)
    : migration_penalty_(migration_penalty) {}

int GlobalQueueBalancer::assign_core(Process* proc, const std::vector<Core>& cores) {
    // Always assign to least loaded core
    return find_least_loaded_core(cores);
}

void GlobalQueueBalancer::balance_load(std::vector<Core>& cores, IScheduler* scheduler) {
    // Global queue strategy doesn't need explicit load balancing
    // since all assignments go through assign_core which picks least loaded
}

int GlobalQueueBalancer::find_least_loaded_core(const std::vector<Core>& cores) const {
    if (cores.empty()) return -1;
    
    int best_core = 0;
    uint32_t min_load = cores[0].busy_time() + cores[0].context_switch_overhead_time();
    
    for (size_t i = 1; i < cores.size(); ++i) {
        uint32_t load = cores[i].busy_time() + cores[i].context_switch_overhead_time();
        if (load < min_load) {
            min_load = load;
            best_core = static_cast<int>(i);
        }
    }
    
    return best_core;
}

// ========== Per-Core Balancer ==========

PerCoreBalancer::PerCoreBalancer(uint32_t migration_penalty, uint32_t steal_threshold)
    : migration_penalty_(migration_penalty), steal_threshold_(steal_threshold) {}

int PerCoreBalancer::assign_core(Process* proc, const std::vector<Core>& cores) {
    // Use affinity-based assignment
    return find_affinity_core(proc, cores);
}

void PerCoreBalancer::balance_load(std::vector<Core>& cores, IScheduler* scheduler) {
    // Work stealing: if one core is much more loaded than another, steal work
    // This would require scheduler cooperation and is simplified here
    // In a real implementation, this would move processes between per-core queues
}

int PerCoreBalancer::find_affinity_core(Process* proc, const std::vector<Core>& cores) const {
    if (cores.empty()) return -1;
    
    // First preference: same core as last time (cache locality)
    if (proc->last_core_id >= 0 && 
        proc->last_core_id < static_cast<int>(cores.size()) &&
        cores[proc->last_core_id].is_idle()) {
        return proc->last_core_id;
    }
    
    // Second preference: core with same speed as last time
    if (proc->last_core_id >= 0 && proc->last_core_id < static_cast<int>(cores.size())) {
        double preferred_speed = cores[proc->last_core_id].speed();
        for (size_t i = 0; i < cores.size(); ++i) {
            if (cores[i].speed() == preferred_speed && cores[i].is_idle()) {
                return static_cast<int>(i);
            }
        }
    }
    
    // Fallback: least loaded core
    int best_core = 0;
    uint32_t min_load = cores[0].busy_time();
    
    for (size_t i = 1; i < cores.size(); ++i) {
        uint32_t load = cores[i].busy_time();
        if (load < min_load) {
            min_load = load;
            best_core = static_cast<int>(i);
        }
    }
    
    return best_core;
}

// ========== Speed-Aware Balancer ==========

SpeedAwareBalancer::SpeedAwareBalancer(uint32_t migration_penalty)
    : migration_penalty_(migration_penalty) {}

int SpeedAwareBalancer::assign_core(Process* proc, const std::vector<Core>& cores) {
    return find_optimal_core(proc, cores);
}

void SpeedAwareBalancer::balance_load(std::vector<Core>& cores, IScheduler* scheduler) {
    // Speed-aware balancing: move long-running processes to fast cores,
    // short processes can stay on slow cores
    // This is simplified - real implementation would require scheduler cooperation
}

int SpeedAwareBalancer::find_optimal_core(Process* proc, const std::vector<Core>& cores) const {
    if (cores.empty()) return -1;
    
    int best_core = 0;
    double min_completion_time = estimate_completion_time(proc, cores[0]);
    
    for (size_t i = 1; i < cores.size(); ++i) {
        double completion_time = estimate_completion_time(proc, cores[i]);
        if (completion_time < min_completion_time) {
            min_completion_time = completion_time;
            best_core = static_cast<int>(i);
        }
    }
    
    return best_core;
}

double SpeedAwareBalancer::estimate_completion_time(Process* proc, const Core& core) const {
    if (core.speed() <= 0.0) return 1e9;
    double work_time = proc->remaining_time / core.speed();
    
    // Add estimated delay until core finishes its currently running process
    if (!core.is_idle() && core.current_process() != nullptr) {
        work_time += core.current_process()->remaining_time / core.speed();
    }
    
    return work_time;
}

} // namespace scheduler