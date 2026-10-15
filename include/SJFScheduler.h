#pragma once

#include "IScheduler.h"
#include <vector>
#include <algorithm>

namespace scheduler {

// Shortest Job First (SJF) scheduler
// Non-preemptive: selects process with shortest burst time
// Optimal for minimizing average waiting time (among non-preemptive algorithms)
class SJFScheduler : public IScheduler {
public:
    SJFScheduler() = default;
    
    std::string name() const override { return "SJF"; }
    bool is_preemptive() const override { return false; }
    
    void on_arrival(Process* proc) override;
    Process* pick_next(Core* core, uint32_t current_time) override;
    void on_completion(Process* proc) override;
    void on_tick(uint32_t current_time) override;

private:
    std::vector<Process*> ready_queue_;
    
    // Sort by burst time (shortest first), then arrival time, then PID
    void sort_ready_queue();
};

} // namespace scheduler
