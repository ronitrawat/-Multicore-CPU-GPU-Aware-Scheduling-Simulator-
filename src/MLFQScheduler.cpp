#include "MLFQScheduler.h"
#include "Process.h"
#include "Core.h"
#include <algorithm>

namespace scheduler {

MLFQScheduler::MLFQScheduler(std::vector<uint32_t> quanta, uint32_t boost_interval)
    : quanta_(quanta.empty() ? std::vector<uint32_t>{2, 4, 8} : std::move(quanta)),
      boost_interval_(boost_interval),
      ticks_since_boost_(0) {
    queues_.resize(quanta_.size());
}

std::string MLFQScheduler::config_string() const {
    std::string s = "MLFQ(levels=" + std::to_string(quanta_.size()) + ",quanta=[";
    for (size_t i = 0; i < quanta_.size(); ++i) {
        if (i > 0) s += ",";
        s += std::to_string(quanta_[i]);
    }
    s += "],boost=" + std::to_string(boost_interval_) + ")";
    return s;
}

void MLFQScheduler::on_arrival(Process* proc) {
    proc->state = ProcessState::READY;
    proc->ready_enter_time = proc->arrival_time;
    // New processes always enter the top priority queue (level 0)
    queues_[0].push_back(proc);
    process_state_[proc->pid] = {0, quanta_[0]};
}

int MLFQScheduler::highest_ready_level() const {
    for (size_t i = 0; i < queues_.size(); ++i) {
        if (!queues_[i].empty()) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

Process* MLFQScheduler::pick_next(Core* core, uint32_t current_time) {
    Process* current = core->is_idle() ? nullptr : core->current_process();
    
    if (current != nullptr && !current->is_complete()) {
        auto it = process_state_.find(current->pid);
        if (it != process_state_.end()) {
            // Check if quantum expired
            if (it->second.quantum_remaining == 0) {
                // Quantum expired: demote to next lower priority level if not already at bottom
                if (it->second.current_level + 1 < queues_.size()) {
                    it->second.current_level++;
                }
                it->second.quantum_remaining = quanta_[it->second.current_level];
                current->state = ProcessState::READY;
                current->ready_enter_time = current_time;
                queues_[it->second.current_level].push_back(current);
                current = nullptr;
            } else {
                // Check if a process in a strictly higher queue is waiting
                int highest_level = highest_ready_level();
                if (highest_level != -1 && static_cast<size_t>(highest_level) < it->second.current_level) {
                    // Preempt currently running process for higher priority level job
                    current->state = ProcessState::READY;
                    current->ready_enter_time = current_time;
                    queues_[it->second.current_level].push_back(current);
                    current = nullptr;
                } else {
                    // Continue running current process
                    it->second.quantum_remaining--;
                    return current;
                }
            }
        }
    }
    
    // Pick next process from highest priority non-empty queue
    int next_level = highest_ready_level();
    if (next_level == -1) {
        return nullptr;
    }
    
    Process* next = queues_[next_level].front();
    queues_[next_level].pop_front();
    next->state = ProcessState::RUNNING;
    if (current_time > next->ready_enter_time) {
        next->waiting_time += (current_time - next->ready_enter_time);
    }
    
    if (next->first_response_time == UINT32_MAX) {
        next->first_response_time = current_time;
    }
    
    // Decrement 1 quantum unit for the upcoming tick
    auto& state = process_state_[next->pid];
    state.current_level = static_cast<size_t>(next_level);
    state.quantum_remaining = (quanta_[next_level] > 0) ? (quanta_[next_level] - 1) : 0;
    
    return next;
}

Process* MLFQScheduler::peek_next(Core* core, uint32_t current_time) {
    (void)current_time;
    Process* current = core->is_idle() ? nullptr : core->current_process();
    
    if (current != nullptr && !current->is_complete()) {
        auto it = process_state_.find(current->pid);
        if (it != process_state_.end()) {
            if (it->second.quantum_remaining > 0) {
                int highest_level = highest_ready_level();
                if (highest_level == -1 || static_cast<size_t>(highest_level) >= it->second.current_level) {
                    return current;
                }
            }
        }
    }
    
    int next_level = highest_ready_level();
    if (next_level == -1) {
        return nullptr;
    }
    return queues_[next_level].front();
}

void MLFQScheduler::on_completion(Process* proc) {
    proc->state = ProcessState::TERMINATED;
    for (auto& queue : queues_) {
        auto it = std::find(queue.begin(), queue.end(), proc);
        if (it != queue.end()) {
            queue.erase(it);
        }
    }
    process_state_.erase(proc->pid);
}

void MLFQScheduler::on_tick(uint32_t current_time) {
    (void)current_time;
    // O(1) performance optimization: waiting time is accumulated accurately
    // upon state transitions in pick_next(), eliminating O(N) tick loops.
    
    // Check priority boost (reset all ready processes to top queue Q0)
    if (boost_interval_ > 0) {
        ticks_since_boost_++;
        if (ticks_since_boost_ >= boost_interval_) {
            priority_boost();
            ticks_since_boost_ = 0;
        }
    }
}

void MLFQScheduler::priority_boost() {
    // Move all processes from lower priority queues (level > 0) into queue 0
    for (size_t lvl = 1; lvl < queues_.size(); ++lvl) {
        while (!queues_[lvl].empty()) {
            Process* proc = queues_[lvl].front();
            queues_[lvl].pop_front();
            queues_[0].push_back(proc);
            
            auto it = process_state_.find(proc->pid);
            if (it != process_state_.end()) {
                it->second.current_level = 0;
                it->second.quantum_remaining = quanta_[0];
            }
        }
    }
}

int MLFQScheduler::get_process_level(uint32_t pid) const {
    auto it = process_state_.find(pid);
    if (it != process_state_.end()) {
        return static_cast<int>(it->second.current_level);
    }
    return -1;
}

} // namespace scheduler
