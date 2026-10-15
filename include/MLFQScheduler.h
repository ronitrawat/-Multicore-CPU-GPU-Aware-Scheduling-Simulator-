#pragma once

#include "IScheduler.h"
#include <vector>
#include <deque>
#include <string>
#include <unordered_map>

namespace scheduler {

// Multilevel Feedback Queue (MLFQ) Scheduler
// Features:
// - Multiple priority levels (0 = highest priority)
// - Configurable time quantum per level (e.g. Q0=2, Q1=4, Q2=8...)
// - Processes that exhaust their quantum are demoted to the next lower queue
// - Periodic priority boost: all processes moved back to Q0 after boost_interval ticks
// - Preemptive: a process arriving or ready in a higher level preempts lower level jobs
class MLFQScheduler : public IScheduler {
public:
    // Default: 3 levels (quantum 2, 4, 8) with priority boost every 50 ticks
    explicit MLFQScheduler(std::vector<uint32_t> quanta = {2, 4, 8}, 
                           uint32_t boost_interval = 50);
    
    std::string name() const override { return "MLFQ"; }
    bool is_preemptive() const override { return true; }
    std::string config_string() const override;
    
    void on_arrival(Process* proc) override;
    Process* pick_next(Core* core, uint32_t current_time) override;
    Process* peek_next(Core* core, uint32_t current_time) override;
    void on_completion(Process* proc) override;
    void on_tick(uint32_t current_time) override;
    
    // Config accessors
    const std::vector<uint32_t>& quanta() const { return quanta_; }
    uint32_t boost_interval() const { return boost_interval_; }
    size_t num_levels() const { return queues_.size(); }
    
    // Process queue level query
    int get_process_level(uint32_t pid) const;

private:
    std::vector<uint32_t> quanta_;
    uint32_t boost_interval_;
    uint32_t ticks_since_boost_;
    
    // Multi-level queues: index 0 is highest priority
    std::vector<std::deque<Process*>> queues_;
    
    // Tracking current level and quantum for each process
    struct ProcessStateTrack {
        size_t current_level;
        uint32_t quantum_remaining;
    };
    std::unordered_map<uint32_t, ProcessStateTrack> process_state_;
    
    // Priority boost: reset all processes to top queue Q0
    void priority_boost();
    
    // Helper to find highest priority non-empty queue
    int highest_ready_level() const;
};

} // namespace scheduler
