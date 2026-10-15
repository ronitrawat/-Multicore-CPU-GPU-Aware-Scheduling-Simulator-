#pragma once

#include <string>
#include <cstdint>

namespace scheduler {

enum class ProcessState {
    NEW,
    READY,
    RUNNING,
    TERMINATED
};

struct Process {
    uint32_t pid;
    std::string name;
    
    // Timing
    uint32_t arrival_time;
    uint32_t burst_time;        // Total work units required
    double remaining_time;      // Work units left (double supports fractional core speeds)
    
    // Scheduling
    int priority;               // Lower number = higher priority
    ProcessState state;
    
    // Metrics
    uint32_t first_response_time;  // When first scheduled (UINT32_MAX if not yet)
    uint32_t completion_time;
    uint32_t waiting_time;         // Total time spent in READY state
    uint32_t ready_enter_time;    // Timestamp when process entered READY state
    
    // Multicore tracking
    int last_core_id;              // -1 if never run
    uint32_t age_counter;          // For aging mechanism (ticks waiting)
    uint32_t migration_count;      // Number of times migrated between cores
    uint32_t migration_penalty_remaining;  // Ticks of migration penalty left
    
    // Constructor
    Process(uint32_t pid, const std::string& name, uint32_t arrival, 
            uint32_t burst, int priority = 0);
    
    // Helpers
    bool is_complete() const { return remaining_time <= 1e-6; }
    uint32_t turnaround_time() const;  // completion - arrival
    uint32_t response_time() const;     // first_response - arrival
};

} // namespace scheduler
