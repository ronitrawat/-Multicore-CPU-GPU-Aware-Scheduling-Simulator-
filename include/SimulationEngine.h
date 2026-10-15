#pragma once

#include <vector>
#include <memory>
#include <queue>
#include "Process.h"
#include "Core.h"
#include "IScheduler.h"
#include "GanttChart.h"
#include "LoadBalancer.h"

namespace scheduler {

// Event types for the simulation
enum class EventType {
    PROCESS_ARRIVAL,
    TICK
};

struct Event {
    uint32_t time;
    EventType type;
    Process* process;  // Relevant process (if any)
    
    // Priority queue orders by time (earliest first)
    // When timestamp is identical, PROCESS_ARRIVAL must be processed before TICK
    bool operator>(const Event& other) const {
        if (time != other.time) {
            return time > other.time;
        }
        if (type != other.type) {
            return type == EventType::TICK;
        }
        if (process && other.process) {
            return process->pid > other.process->pid;
        }
        return false;
    }
};

class SimulationEngine {
public:
    SimulationEngine(std::unique_ptr<IScheduler> scheduler, 
                     std::vector<Core> cores,
                     uint32_t context_switch_cost = 0,
                     bool enable_gantt = false,
                     std::unique_ptr<ILoadBalancer> load_balancer = nullptr);
    
    // Run the simulation with the given workload
    void run(std::vector<std::unique_ptr<Process>>& workload);
    
    // Get results
    const std::vector<Process*>& completed_processes() const { return completed_processes_; }
    const std::vector<Core>& cores() const { return cores_; }
    uint32_t current_time() const { return current_time_; }
    const GanttChart& gantt_chart() const { return gantt_chart_; }
    bool has_gantt_chart() const { return enable_gantt_; }
    const ILoadBalancer* load_balancer() const { return load_balancer_.get(); }
    void set_silent(bool silent) { silent_ = silent; }
    
private:
    void schedule_initial_events(std::vector<std::unique_ptr<Process>>& workload);
    void process_event(const Event& event);
    void tick();
    void handle_process_arrival(Process* proc);
    void check_completions();
    
    std::unique_ptr<IScheduler> scheduler_;
    std::vector<Core> cores_;
    uint32_t context_switch_cost_;
    bool enable_gantt_;
    bool silent_ = false;
    std::unique_ptr<ILoadBalancer> load_balancer_;
    
    // Event queue (priority queue, earliest event first)
    std::priority_queue<Event, std::vector<Event>, std::greater<Event>> event_queue_;
    
    // Tracking
    uint32_t current_time_;
    std::vector<Process*> completed_processes_;
    std::vector<Process*> all_processes_;  // Raw pointers to all processes
    GanttChart gantt_chart_;
};

} // namespace scheduler
