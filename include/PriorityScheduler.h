#pragma once

#include "IScheduler.h"
#include <vector>
#include <algorithm>

namespace scheduler {

// Priority Scheduler with configurable preemption and aging
// Lower priority number = higher priority (0 is highest)
// Supports aging to prevent starvation
class PriorityScheduler : public IScheduler {
public:
    explicit PriorityScheduler(bool preemptive = true, uint32_t aging_interval = 10);
    
    std::string name() const override;
    bool is_preemptive() const override { return preemptive_; }
    std::string config_string() const override;
    
    void on_arrival(Process* proc) override;
    Process* pick_next(Core* core, uint32_t current_time) override;
    Process* peek_next(Core* core, uint32_t current_time) override;
    void on_completion(Process* proc) override;
    void on_tick(uint32_t current_time) override;

private:
    bool preemptive_;
    uint32_t aging_interval_;  // Ticks between aging updates (0 = no aging)
    std::vector<Process*> ready_queue_;
    
    // Track original priorities for aging reset
    struct ProcessAging {
        Process* process;
        int original_priority;
        uint32_t wait_ticks;
    };
    std::vector<ProcessAging> aging_tracker_;
    
    // Sort by effective priority (after aging), then arrival, then PID
    void sort_ready_queue();
    
    // Age processes to prevent starvation
    void apply_aging();
    
    // Reset priority when process starts running
    void reset_priority(Process* proc);
    
    // Get aging info for a process
    ProcessAging* find_aging_info(Process* proc);
};

} // namespace scheduler