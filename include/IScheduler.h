#pragma once

#include <vector>
#include <memory>
#include <string>

namespace scheduler {

class Process;
class Core;

// Strategy interface for scheduling algorithms
class IScheduler {
public:
    virtual ~IScheduler() = default;
    
    // Algorithm identification
    virtual std::string name() const = 0;
    
    // Is this algorithm preemptive?
    virtual bool is_preemptive() const = 0;
    
    // Called when a process arrives
    virtual void on_arrival(Process* proc) = 0;
    
    // Pick the next process to run on a given core
    // Returns nullptr if no process is ready
    // The scheduler can decide to preempt the current process or continue it
    virtual Process* pick_next(Core* core, uint32_t current_time) = 0;
    
    // Peek at the next process that would be scheduled for a core without dequeuing
    virtual Process* peek_next(Core* core, uint32_t current_time) {
        (void)core; (void)current_time;
        return nullptr;
    }
    
    // Called when a process completes
    virtual void on_completion(Process* proc) = 0;
    
    // Called on each simulation tick (for aging, bookkeeping, etc.)
    virtual void on_tick(uint32_t current_time) = 0;
    
    // Get configuration string (e.g., "RR(q=4)" for Round Robin with quantum 4)
    virtual std::string config_string() const { return name(); }
};

} // namespace scheduler
