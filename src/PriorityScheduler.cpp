#include "PriorityScheduler.h"
#include "Process.h"
#include "Core.h"
#include <algorithm>

namespace scheduler {

PriorityScheduler::PriorityScheduler(bool preemptive, uint32_t aging_interval)
    : preemptive_(preemptive), aging_interval_(aging_interval) {}

std::string PriorityScheduler::name() const {
    return preemptive_ ? "Priority" : "Priority-NP";
}

std::string PriorityScheduler::config_string() const {
    std::string base = name();
    if (aging_interval_ > 0) {
        base += "(aging=" + std::to_string(aging_interval_) + ")";
    }
    return base;
}

void PriorityScheduler::on_arrival(Process* proc) {
    proc->state = ProcessState::READY;
    proc->ready_enter_time = proc->arrival_time;
    ready_queue_.push_back(proc);
    
    // Initialize aging tracking
    ProcessAging aging;
    aging.process = proc;
    aging.original_priority = proc->priority;
    aging.wait_ticks = 0;
    aging_tracker_.push_back(aging);
    
    sort_ready_queue();
}

Process* PriorityScheduler::pick_next(Core* core, uint32_t current_time) {
    Process* current = core->is_idle() ? nullptr : core->current_process();
    
    // If core already has a running process that is not complete
    if (current != nullptr && !current->is_complete()) {
        if (!preemptive_) {
            return current;
        }
        
        // Preempt only if there is a process with strictly higher priority (lower priority number)
        if (!ready_queue_.empty() && ready_queue_.front()->priority < current->priority) {
            current->state = ProcessState::READY;
            current->ready_enter_time = current_time;
            ready_queue_.push_back(current);
            sort_ready_queue();
            
            Process* next = ready_queue_.front();
            ready_queue_.erase(ready_queue_.begin());
            next->state = ProcessState::RUNNING;
            if (current_time > next->ready_enter_time) {
                next->waiting_time += (current_time - next->ready_enter_time);
            }
            
            if (next->first_response_time == UINT32_MAX) {
                next->first_response_time = current_time;
            }
            reset_priority(next);
            return next;
        }
        
        // No higher priority process waiting; continue current process
        return current;
    }
    
    // Core is idle or previous process completed: pick highest priority process
    if (ready_queue_.empty()) {
        return nullptr;
    }
    
    Process* next = ready_queue_.front();
    ready_queue_.erase(ready_queue_.begin());
    
    next->state = ProcessState::RUNNING;
    if (current_time > next->ready_enter_time) {
        next->waiting_time += (current_time - next->ready_enter_time);
    }
    
    if (next->first_response_time == UINT32_MAX) {
        next->first_response_time = current_time;
    }
    
    reset_priority(next);
    return next;
}

Process* PriorityScheduler::peek_next(Core* core, uint32_t current_time) {
    (void)current_time;
    Process* current = core->is_idle() ? nullptr : core->current_process();
    if (current != nullptr && !current->is_complete()) {
        if (!preemptive_) {
            return current;
        }
        if (!ready_queue_.empty() && ready_queue_.front()->priority < current->priority) {
            return ready_queue_.front();
        }
        return current;
    }
    return ready_queue_.empty() ? nullptr : ready_queue_.front();
}

void PriorityScheduler::on_completion(Process* proc) {
    proc->state = ProcessState::TERMINATED;
    
    // Remove from ready queue if somehow still there
    auto it = std::find(ready_queue_.begin(), ready_queue_.end(), proc);
    if (it != ready_queue_.end()) {
        ready_queue_.erase(it);
    }
    
    // Remove from aging tracker
    aging_tracker_.erase(
        std::remove_if(aging_tracker_.begin(), aging_tracker_.end(),
                      [proc](const ProcessAging& aging) {
                          return aging.process == proc;
                      }),
        aging_tracker_.end());
}

void PriorityScheduler::on_tick(uint32_t current_time) {
    if (aging_interval_ == 0) {
        return;
    }
    
    for (Process* proc : ready_queue_) {
        ProcessAging* aging = find_aging_info(proc);
        if (aging) {
            aging->wait_ticks++;
        }
    }
    
    // Apply aging if interval is set and it's time
    if (current_time > 0 && current_time % aging_interval_ == 0) {
        apply_aging();
        sort_ready_queue();
    }
}

void PriorityScheduler::sort_ready_queue() {
    std::sort(ready_queue_.begin(), ready_queue_.end(),
              [](const Process* a, const Process* b) {
                  // Primary: higher priority (lower number)
                  if (a->priority != b->priority) {
                      return a->priority < b->priority;
                  }
                  // Tie-break: earlier arrival
                  if (a->arrival_time != b->arrival_time) {
                      return a->arrival_time < b->arrival_time;
                  }
                  // Tie-break: lower PID
                  return a->pid < b->pid;
              });
}

void PriorityScheduler::apply_aging() {
    if (aging_interval_ == 0) return;
    
    for (auto& aging : aging_tracker_) {
        Process* proc = aging.process;
        
        // Only age processes that are waiting (in ready queue)
        if (proc->state == ProcessState::READY) {
            // Improve priority by 1 for every aging_interval_ ticks waited
            int priority_boost = static_cast<int>(aging.wait_ticks / aging_interval_);
            
            if (priority_boost > 0) {
                // Improve priority (decrease number), but don't go below 0
                int new_priority = aging.original_priority - priority_boost;
                proc->priority = std::max(0, new_priority);
                
                // Reset wait ticks for next aging period
                aging.wait_ticks = 0;
            }
        }
    }
}

void PriorityScheduler::reset_priority(Process* proc) {
    ProcessAging* aging = find_aging_info(proc);
    if (aging) {
        proc->priority = aging->original_priority;
        aging->wait_ticks = 0;
    }
}

PriorityScheduler::ProcessAging* PriorityScheduler::find_aging_info(Process* proc) {
    auto it = std::find_if(aging_tracker_.begin(), aging_tracker_.end(),
                          [proc](const ProcessAging& aging) {
                              return aging.process == proc;
                          });
    return (it != aging_tracker_.end()) ? &(*it) : nullptr;
}

} // namespace scheduler