#include "RoundRobinScheduler.h"
#include "Process.h"
#include "Core.h"
#include <algorithm>

namespace scheduler {

RoundRobinScheduler::RoundRobinScheduler(uint32_t quantum)
    : quantum_(quantum) {}

std::string RoundRobinScheduler::config_string() const {
    return "RR(q=" + std::to_string(quantum_) + ")";
}

void RoundRobinScheduler::on_arrival(Process* proc) {
    proc->state = ProcessState::READY;
    proc->ready_enter_time = proc->arrival_time;
    ready_queue_.push_back(proc);
}

Process* RoundRobinScheduler::pick_next(Core* core, uint32_t current_time) {
    int core_id = core->id();
    
    // Get currently running process
    Process* current = core->is_idle() ? nullptr : core->current_process();
    
    // Check if current process has exhausted its quantum or is complete
    bool quantum_expired = false;
    if (current != nullptr && !current->is_complete()) {
        auto it = core_quantum_remaining_.find(core_id);
        if (it != core_quantum_remaining_.end() && it->second == 0) {
            quantum_expired = true;
        }
    }
    
    // If quantum expired, preempt and move to back of queue
    if (quantum_expired && current != nullptr && !current->is_complete()) {
        current->state = ProcessState::READY;
        current->ready_enter_time = current_time;
        ready_queue_.push_back(current);
        current = nullptr;
    }
    
    // If still running the same process, continue
    if (current != nullptr && !current->is_complete() && !quantum_expired) {
        // Decrement quantum
        if (core_quantum_remaining_.find(core_id) != core_quantum_remaining_.end()) {
            core_quantum_remaining_[core_id]--;
        }
        return current;
    }
    
    // Pick next process from queue
    if (ready_queue_.empty()) {
        return nullptr;
    }
    
    Process* next = ready_queue_.front();
    ready_queue_.pop_front();
    
    next->state = ProcessState::RUNNING;
    if (current_time > next->ready_enter_time) {
        next->waiting_time += (current_time - next->ready_enter_time);
    }
    
    // Reset quantum for this core (1 unit consumed in the upcoming tick)
    core_quantum_remaining_[core_id] = (quantum_ > 0) ? (quantum_ - 1) : 0;
    
    // Record first response time if this is the first time running
    if (next->first_response_time == UINT32_MAX) {
        next->first_response_time = current_time;
    }
    
    return next;
}

void RoundRobinScheduler::on_completion(Process* proc) {
    proc->state = ProcessState::TERMINATED;
    
    // Remove from ready queue if somehow still there
    auto it = std::find(ready_queue_.begin(), ready_queue_.end(), proc);
    if (it != ready_queue_.end()) {
        ready_queue_.erase(it);
    }
}

void RoundRobinScheduler::on_tick(uint32_t /*current_time*/) {
    // O(1) performance optimization: waiting time is accumulated accurately
    // upon state transitions in pick_next(), eliminating O(N) tick loops.
}

} // namespace scheduler
