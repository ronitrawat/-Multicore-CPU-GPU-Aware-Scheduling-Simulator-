#pragma once

#include "IScheduler.h"
#include <vector>
#include <algorithm>

namespace scheduler {

// Shortest Remaining Time First (SRTF) scheduler
// Preemptive version of SJF: can preempt currently running process
// if a new arrival has less remaining time
// Optimal for minimizing average waiting time (among all algorithms)
class SRTFScheduler : public IScheduler {
public:
    SRTFScheduler() = default;
    
    std::string name() const override { return "SRTF"; }
    bool is_preemptive() const override { return true; }
    
    void on_arrival(Process* proc) override;
    Process* pick_next(Core* core, uint32_t current_time) override;
    void on_completion(Process* proc) override;
    void on_tick(uint32_t current_time) override;

private:
    std::vector<Process*> ready_queue_;
    
    // Sort by remaining time (shortest first), then arrival, then PID
    void sort_ready_queue();
};

} // namespace scheduler
