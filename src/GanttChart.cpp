#include "GanttChart.h"
#include "Process.h"
#include "Core.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <map>
#include <algorithm>

namespace scheduler {

void GanttChart::record_tick(uint32_t time, const std::vector<Core>& cores) {
    for (const auto& core : cores) {
        GanttSegment seg;
        seg.start_time = time;
        seg.end_time = time + 1;
        seg.core_id = core.id();
        
        if (core.is_context_switching()) {
            seg.pid = GanttSegment::CS_PID;
            seg.name = "CS";
        } else if (core.current_process() != nullptr) {
            seg.pid = core.current_process()->pid;
            seg.name = core.current_process()->name;
        } else {
            seg.pid = GanttSegment::IDLE_PID;
            seg.name = "IDLE";
        }
        
        segments_.push_back(seg);
    }
}

void GanttChart::consolidate_segments() {
    if (segments_.empty()) return;
    
    // Sort by core, then time
    std::sort(segments_.begin(), segments_.end(),
              [](const GanttSegment& a, const GanttSegment& b) {
                  if (a.core_id != b.core_id) return a.core_id < b.core_id;
                  return a.start_time < b.start_time;
              });
    
    // Merge consecutive segments with same process on same core
    std::vector<GanttSegment> consolidated;
    GanttSegment current = segments_[0];
    
    for (size_t i = 1; i < segments_.size(); ++i) {
        if (segments_[i].core_id == current.core_id &&
            segments_[i].pid == current.pid &&
            segments_[i].start_time == current.end_time) {
            // Merge
            current.end_time = segments_[i].end_time;
        } else {
            // Save current and start new
            consolidated.push_back(current);
            current = segments_[i];
        }
    }
    consolidated.push_back(current);
    
    segments_ = consolidated;
}

std::vector<GanttSegment> GanttChart::get_core_segments(int core_id) const {
    std::vector<GanttSegment> result;
    for (const auto& seg : segments_) {
        if (seg.core_id == core_id) {
            result.push_back(seg);
        }
    }
    return result;
}

void GanttChart::print_ascii(uint32_t max_width) const {
    if (segments_.empty()) {
        std::cout << "No Gantt chart data available.\n";
        return;
    }
    
    // Consolidate first
    auto chart = *this;
    chart.consolidate_segments();
    
    // Find all cores and total time
    std::map<int, std::vector<GanttSegment>> core_segments;
    uint32_t total_time = 0;
    
    for (const auto& seg : chart.segments_) {
        core_segments[seg.core_id].push_back(seg);
        total_time = std::max(total_time, seg.end_time);
    }
    
    std::cout << "\n=== Gantt Chart ===\n\n";
    
    // Print each core
    for (const auto& [core_id, segs] : core_segments) {
        chart.print_core_timeline(core_id, total_time, max_width);
        std::cout << "\n";
    }
}

void GanttChart::print_core_timeline(int core_id, uint32_t total_time, uint32_t max_width) const {
    auto segs = get_core_segments(core_id);
    if (segs.empty()) return;
    
    std::cout << "Core " << core_id << ": ";
    
    // Calculate scaling factor
    double scale = (total_time > max_width) 
        ? static_cast<double>(max_width) / total_time 
        : 1.0;
    
    // Print timeline
    for (const auto& seg : segs) {
        uint32_t duration = seg.end_time - seg.start_time;
        int width = std::max(1, static_cast<int>(duration * scale));
        
        // Choose symbol based on type
        char symbol;
        if (seg.pid == GanttSegment::IDLE_PID) {
            symbol = '.';
        } else if (seg.pid == GanttSegment::CS_PID) {
            symbol = '|';
        } else {
            // Use letters for processes (cycle through A-Z)
            symbol = 'A' + ((seg.pid - 1) % 26);
        }
        
        for (int i = 0; i < width; ++i) {
            std::cout << symbol;
        }
    }
    
    std::cout << "\n";
    
    // Print legend for this core
    std::cout << "        ";
    std::map<uint32_t, std::string> unique_processes;
    for (const auto& seg : segs) {
        if (seg.pid != GanttSegment::IDLE_PID && seg.pid != GanttSegment::CS_PID) {
            char symbol = 'A' + ((seg.pid - 1) % 26);
            unique_processes[seg.pid] = seg.name + "(" + std::string(1, symbol) + ")";
        }
    }
    
    bool first = true;
    for (const auto& [pid, label] : unique_processes) {
        if (!first) std::cout << ", ";
        std::cout << label;
        first = false;
    }
    if (!unique_processes.empty()) {
        std::cout << ", .(idle), |(CS)";
    }
}

void GanttChart::export_csv(const std::string& filename) const {
    auto chart = *this;
    chart.consolidate_segments();
    
    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to create file: " + filename);
    }
    
    file << "core_id,pid,name,start_time,end_time,duration\n";
    
    for (const auto& seg : chart.segments_) {
        file << seg.core_id << ","
             << seg.pid << ","
             << seg.name << ","
             << seg.start_time << ","
             << seg.end_time << ","
             << (seg.end_time - seg.start_time) << "\n";
    }
}

} // namespace scheduler
