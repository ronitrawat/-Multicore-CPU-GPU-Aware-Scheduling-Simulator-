#include "SJFScheduler.h"
#include "Process.h"
#include "Core.h"

namespace scheduler {

void SJFScheduler::on_arrival(Process* proc) {
    proc->state = ProcessState::READY;
    proc->ready_enter_time = proc->arrival_time;
    ready_queue_.push_back(proc);
    sort_ready_queue();
}

Process* SJFScheduler::pick_next(Core* core, uint32_t current_time) {
    // If core is busy with a process, continue running it (non-preemptive)
    if (!core->is_idle()) {
        Process* current = core->current_process();
        if (current != nullptr && !current->is_complete()) {
            return current;
        }
    }
    
    // Pick the shortest job from the queue
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

void SJFScheduler::on_completion(Process* proc) {
    proc->state = ProcessState::TERMINATED;
}

void SJFScheduler::on_tick(uint32_t /*current_time*/) {
    // O(1) performance optimization: waiting time is accumulated accurately
    // upon state transitions in pick_next(), eliminating O(N) tick loops.
}

void SJFScheduler::sort_ready_queue() {
    std::sort(ready_queue_.begin(), ready_queue_.end(),
              [](const Process* a, const Process* b) {
                  // Primary: shortest burst time
                  if (a->burst_time != b->burst_time) {
                      return a->burst_time < b->burst_time;
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
