#include "Core.h"
#include "Process.h"

namespace scheduler {

Core::Core(int id, double speed)
    : id_(id),
      speed_(speed),
      current_process_(nullptr),
      cs_cost_(0),
      cs_remaining_(0),
      busy_time_(0),
      idle_time_(0),
      context_switches_(0),
      cs_overhead_time_(0) {}

bool Core::assign_process(Process* proc, uint32_t migration_penalty) {
    if (proc == current_process_) {
        return false;  // No context switch
    }
    
    bool had_process = (current_process_ != nullptr);
    
    // Check if this is a migration (process ran on different core before)
    if (proc != nullptr && proc->last_core_id != -1 && proc->last_core_id != id_) {
        proc->migration_count++;
        proc->migration_penalty_remaining = migration_penalty;
    }
    
    current_process_ = proc;
    
    // Context switch occurs if we had a different process before
    if (had_process || proc != nullptr) {
        context_switches_++;
        cs_remaining_ = cs_cost_;
    }
    
    return true;
}

void Core::clear_process() {
    current_process_ = nullptr;
}

double Core::tick() {
    // Handle context switch overhead first
    if (cs_remaining_ > 0) {
        cs_remaining_--;
        cs_overhead_time_++;
        return 0.0;  // No useful work during context switch
    }
    
    // Handle migration penalty
    if (current_process_ != nullptr && current_process_->migration_penalty_remaining > 0) {
        current_process_->migration_penalty_remaining--;
        cs_overhead_time_++;  // Migration penalty counts as overhead
        return 0.0;  // No useful work during migration penalty
    }
    
    // Do useful work if process is assigned
    if (current_process_ != nullptr) {
        busy_time_++;
        return speed_;  // Work done = speed multiplier
    } else {
        idle_time_++;
        return 0.0;
    }
}

} // namespace scheduler
