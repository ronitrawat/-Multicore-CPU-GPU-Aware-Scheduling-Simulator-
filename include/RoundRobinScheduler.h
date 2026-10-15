#pragma once

#include "IScheduler.h"
#include <deque>
#include <map>

namespace scheduler {

// Round Robin (RR) scheduler
// Preemptive time-sharing: each process gets a fixed time quantum
// Fair scheduling, good for interactive systems
class RoundRobinScheduler : public IScheduler {
public:
    explicit RoundRobinScheduler(uint32_t quantum);
    
    std::string name() const override { return "RR"; }
    bool is_preemptive() const override { return true; }
    std::string config_string() const override;
    
    void on_arrival(Process* proc) override;
    Process* pick_next(Core* core, uint32_t current_time) override;
    void on_completion(Process* proc) override;
    void on_tick(uint32_t current_time) override;

private:
    uint32_t quantum_;
    std::deque<Process*> ready_queue_;
    
    // Track remaining quantum for currently running process on each core
    std::map<int, uint32_t> core_quantum_remaining_;
};

} // namespace scheduler
