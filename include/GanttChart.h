#pragma once

#include <vector>
#include <string>
#include <cstdint>

namespace scheduler {

class Process;
class Core;

// Represents a single execution segment in the Gantt chart
struct GanttSegment {
    uint32_t start_time;
    uint32_t end_time;
    int core_id;
    uint32_t pid;          // 0 means idle, UINT32_MAX means context switching
    std::string name;
    
    static const uint32_t IDLE_PID = 0;
    static const uint32_t CS_PID = UINT32_MAX;
};

class GanttChart {
public:
    // Record what's running on each core at each time
    void record_tick(uint32_t time, const std::vector<Core>& cores);
    
    // Generate ASCII representation
    void print_ascii(uint32_t max_width = 100) const;
    
    // Export to CSV for external visualization
    void export_csv(const std::string& filename) const;
    
    // Get segments for a specific core
    std::vector<GanttSegment> get_core_segments(int core_id) const;

private:
    std::vector<GanttSegment> segments_;
    
    // Consolidate consecutive ticks into segments
    void consolidate_segments();
    
    // Helper to print a single core's timeline
    void print_core_timeline(int core_id, uint32_t total_time, uint32_t max_width) const;
};

} // namespace scheduler
