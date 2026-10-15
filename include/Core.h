#pragma once

#include <cstdint>
#include <memory>

namespace scheduler {

class Process;

class Core {
public:
    explicit Core(int id, double speed = 1.0);
    
    // Identification
    int id() const { return id_; }
    double speed() const { return speed_; }
    
    // State
    bool is_idle() const { return current_process_ == nullptr; }
    Process* current_process() const { return current_process_; }
    
    // Assign a process to this core (returns true if context switch occurred)
    // migration_penalty: cost in ticks if process is migrating from different core
    bool assign_process(Process* proc, uint32_t migration_penalty = 0);
    
    // Remove current process (on completion or preemption)
    void clear_process();
    
    // Perform work for one time unit
    // Returns the actual work done (speed multiplier applied)
    double tick();
    
    // Metrics
    uint32_t busy_time() const { return busy_time_; }
    uint32_t idle_time() const { return idle_time_; }
    uint32_t context_switch_count() const { return context_switches_; }
    uint32_t context_switch_overhead_time() const { return cs_overhead_time_; }
    
    // For context switch overhead simulation
    void set_context_switch_cost(uint32_t cost) { cs_cost_ = cost; }
    bool is_context_switching() const { return cs_remaining_ > 0; }
    uint32_t context_switch_remaining() const { return cs_remaining_; }

private:
    int id_;
    double speed_;
    Process* current_process_;
    
    // Context switch tracking
    uint32_t cs_cost_;         // Ticks per context switch
    uint32_t cs_remaining_;    // Ticks left in current context switch
    
    // Metrics
    uint32_t busy_time_;
    uint32_t idle_time_;
    uint32_t context_switches_;
    uint32_t cs_overhead_time_;  // Time spent context switching
};

} // namespace scheduler
