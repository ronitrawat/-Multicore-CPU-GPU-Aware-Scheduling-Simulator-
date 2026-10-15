#pragma once

#include "IScheduler.h"
#include <deque>

namespace scheduler {

// First-Come, First-Served (FCFS) scheduler
// Non-preemptive, processes run to completion in arrival order
class FCFSScheduler : public IScheduler {
public:
    FCFSScheduler() = default;
    
    std::string name() const override { return "FCFS"; }
    bool is_preemptive() const override { return false; }
    
    void on_arrival(Process* proc) override;
    Process* pick_next(Core* core, uint32_t current_time) override;
    void on_completion(Process* proc) override;
    void on_tick(uint32_t current_time) override;

private:
    std::deque<Process*> ready_queue_;
};

} // namespace scheduler
