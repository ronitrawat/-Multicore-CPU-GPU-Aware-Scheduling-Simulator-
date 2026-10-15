#include "SimulationEngine.h"
#include <algorithm>
#include <numeric>
#include <iostream>

namespace scheduler {

SimulationEngine::SimulationEngine(std::unique_ptr<IScheduler> scheduler,
                                   std::vector<Core> cores,
                                   uint32_t context_switch_cost,
                                   bool enable_gantt,
                                   std::unique_ptr<ILoadBalancer> load_balancer)
    : scheduler_(std::move(scheduler)),
      cores_(std::move(cores)),
      context_switch_cost_(context_switch_cost),
      enable_gantt_(enable_gantt),
      load_balancer_(std::move(load_balancer)),
      current_time_(0) {
    
    // Set context switch cost for all cores
    for (auto& core : cores_) {
        core.set_context_switch_cost(context_switch_cost_);
    }
    
    // Default load balancer if none provided
    if (!load_balancer_) {
        load_balancer_ = std::make_unique<GlobalQueueBalancer>(1);
    }
}

void SimulationEngine::run(std::vector<std::unique_ptr<Process>>& workload) {
    if (workload.empty()) {
        std::cout << "Warning: Empty workload\n";
        return;
    }
    
    // Store raw pointers for tracking
    all_processes_.clear();
    for (const auto& proc : workload) {
        all_processes_.push_back(proc.get());
    }
    
    // Schedule all arrival events
    schedule_initial_events(workload);
    
    // Main event loop
    while (!event_queue_.empty()) {
        Event event = event_queue_.top();
        event_queue_.pop();
        
        current_time_ = event.time;
        process_event(event);
        
        // Stop if all processes are complete
        if (completed_processes_.size() == all_processes_.size()) {
            break;
        }
    }
    
    if (!silent_) {
        std::cout << "Simulation completed at time " << current_time_ << "\n";
    }
}

void SimulationEngine::schedule_initial_events(std::vector<std::unique_ptr<Process>>& workload) {
    // Add arrival events
    for (const auto& proc : workload) {
        Event arrival;
        arrival.time = proc->arrival_time;
        arrival.type = EventType::PROCESS_ARRIVAL;
        arrival.process = proc.get();
        event_queue_.push(arrival);
    }
    
    // Add initial tick event
    Event tick_event;
    tick_event.time = 0;
    tick_event.type = EventType::TICK;
    tick_event.process = nullptr;
    event_queue_.push(tick_event);
}

void SimulationEngine::process_event(const Event& event) {
    switch (event.type) {
        case EventType::PROCESS_ARRIVAL:
            handle_process_arrival(event.process);
            break;
            
        case EventType::TICK:
            tick();
            
            // Schedule next tick if simulation isn't done
            if (completed_processes_.size() < all_processes_.size()) {
                Event next_tick;
                next_tick.time = current_time_ + 1;
                next_tick.type = EventType::TICK;
                next_tick.process = nullptr;
                event_queue_.push(next_tick);
            }
            break;
    }
}

void SimulationEngine::handle_process_arrival(Process* proc) {
    scheduler_->on_arrival(proc);
}

void SimulationEngine::tick() {
    // Perform load balancing periodically (every 10 ticks)
    if (load_balancer_ && current_time_ % 10 == 0) {
        load_balancer_->balance_load(cores_, scheduler_.get());
    }
    
    // Identify idle and busy cores
    std::vector<size_t> idle_cores;
    std::vector<size_t> busy_cores;
    for (size_t i = 0; i < cores_.size(); ++i) {
        if (cores_[i].is_idle()) {
            idle_cores.push_back(i);
        } else {
            busy_cores.push_back(i);
        }
    }
    
    // 1. Dispatch work to idle cores first (fastest idle cores get first pick)
    std::stable_sort(idle_cores.begin(), idle_cores.end(), [&](size_t a, size_t b) {
        return cores_[a].speed() > cores_[b].speed();
    });
    
    for (size_t core_idx : idle_cores) {
        auto& core = cores_[core_idx];
        
        // If speed-aware load balancing is active, verify candidate optimality
        if (load_balancer_ && load_balancer_->name() == "SpeedAware") {
            Process* candidate = scheduler_->peek_next(&core, current_time_);
            if (candidate != nullptr) {
                int assigned_core = load_balancer_->assign_core(candidate, cores_);
                if (assigned_core != -1 && assigned_core != core.id()) {
                    continue; // Leave idle for better suited core
                }
            }
        }
        
        Process* to_run = scheduler_->pick_next(&core, current_time_);
        if (to_run != nullptr) {
            uint32_t migration_penalty = 0;
            if (load_balancer_ && to_run->last_core_id != -1 && to_run->last_core_id != core.id()) {
                migration_penalty = load_balancer_->get_migration_penalty();
            }
            core.assign_process(to_run, migration_penalty);
            to_run->last_core_id = core.id();
            if (to_run->first_response_time == UINT32_MAX) {
                to_run->first_response_time = current_time_;
            }
        }
    }
    
    // Let scheduler update waiting time and aging for processes STILL waiting in ready queue
    scheduler_->on_tick(current_time_);
    
    // 2. Busy cores: check for preemption only if not already in context switch / migration
    for (size_t core_idx : busy_cores) {
        auto& core = cores_[core_idx];
        if (core.is_context_switching()) {
            continue;
        }
        if (core.current_process() != nullptr && core.current_process()->migration_penalty_remaining > 0) {
            continue;
        }
        
        Process* to_run = scheduler_->pick_next(&core, current_time_);
        if (to_run != nullptr && to_run != core.current_process()) {
            uint32_t migration_penalty = 0;
            if (load_balancer_ && to_run->last_core_id != -1 && to_run->last_core_id != core.id()) {
                migration_penalty = load_balancer_->get_migration_penalty();
            }
            core.assign_process(to_run, migration_penalty);
            to_run->last_core_id = core.id();
            if (to_run->first_response_time == UINT32_MAX) {
                to_run->first_response_time = current_time_;
            }
        }
    }
    
    // 3. Tick all cores and apply work performed
    for (auto& core : cores_) {
        Process* proc = core.current_process();
        double work_done = core.tick();
        
        if (proc != nullptr && work_done > 0.0) {
            if (proc->remaining_time <= work_done + 1e-6) {
                proc->remaining_time = 0.0;
            } else {
                proc->remaining_time -= work_done;
            }
        }
    }
    
    // Record Gantt chart data
    if (enable_gantt_) {
        gantt_chart_.record_tick(current_time_, cores_);
    }
    
    // Check for process completions
    check_completions();
}

void SimulationEngine::check_completions() {
    for (auto& core : cores_) {
        Process* proc = core.current_process();
        if (proc != nullptr && proc->is_complete()) {
            // Completion time is end of current tick (current_time + 1)
            proc->completion_time = current_time_ + 1;
            proc->state = ProcessState::TERMINATED;
            completed_processes_.push_back(proc);
            
            scheduler_->on_completion(proc);
            core.clear_process();
        }
    }
}

} // namespace scheduler
