#include "SRTFScheduler.h"
#include "Process.h"
#include "Core.h"

namespace scheduler {

void SRTFScheduler::on_arrival(Process* proc) {
    proc->state = ProcessState::READY;
    proc->ready_enter_time = proc->arrival_time;
    ready_queue_.push_back(proc);
    sort_ready_queue();
}

Process* SRTFScheduler::pick_next(Core* core, uint32_t current_time) {
    // Get currently running process (if any)
    Process* current = core->is_idle() ? nullptr : core->current_process();
    
    // If current process exists and not complete, add it back to ready queue for comparison
    if (current != nullptr && !current->is_complete()) {
        // Check if it's already in the queue (shouldn't be, but defensive)
        auto it = std::find(ready_queue_.begin(), ready_queue_.end(), current);
        if (it == ready_queue_.end()) {
            current->state = ProcessState::READY;
            current->ready_enter_time = current_time;
            ready_queue_.push_back(current);
            sort_ready_queue();
        }
    }
    
    // Pick the process with shortest remaining time
    if (ready_queue_.empty()) {
        return nullptr;
    }
    
    Process* next = ready_queue_.front();
    ready_queue_.erase(ready_queue_.begin());
    
    next->state = ProcessState::RUNNING;
    if (current_time > next->ready_enter_time) {
        next->waiting_time += (current_time - next->ready_enter_time);
    }
    
    // Record first response time if this is the first time running
    if (next->first_response_time == UINT32_MAX) {
        next->first_response_time = current_time;
    }
    
    return next;
}

void SRTFScheduler::on_completion(Process* proc) {
    proc->state = ProcessState::TERMINATED;
    
    // Remove from ready queue if somehow still there
    auto it = std::find(ready_queue_.begin(), ready_queue_.end(), proc);
    if (it != ready_queue_.end()) {
        ready_queue_.erase(it);
    }
}

void SRTFScheduler::on_tick(uint32_t /*current_time*/) {
    // O(1) performance optimization: waiting time is accumulated accurately
    // upon state transitions in pick_next(), eliminating O(N) tick loops.
}

void SRTFScheduler::sort_ready_queue() {
    std::sort(ready_queue_.begin(), ready_queue_.end(),
              [](const Process* a, const Process* b) {
                  // Primary: shortest remaining time
                  if (std::abs(a->remaining_time - b->remaining_time) > 1e-6) {
                      return a->remaining_time < b->remaining_time;
                  }
                  // Tie-break: earlier arrival
                  if (a->arrival_time != b->arrival_time) {
                      return a->arrival_time < b->arrival_time;
                  }
                  // Tie-break: lower PID
                  return a->pid < b->pid;
              });
}

} // namespace scheduler
