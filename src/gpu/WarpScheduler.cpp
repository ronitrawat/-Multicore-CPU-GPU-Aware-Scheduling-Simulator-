#include "gpu/WarpScheduler.h"
#include <iostream>
#include <iomanip>
#include <algorithm>

namespace scheduler::gpu {

StreamingMultiprocessor::StreamingMultiprocessor(uint32_t sm_id, 
                                               uint32_t max_resident_warps,
                                               WarpPolicy policy)
    : sm_id_(sm_id),
      max_resident_warps_(max_resident_warps),
      policy_(policy),
      last_scheduled_warp_idx_(0),
      current_cycle_(0) {}

void StreamingMultiprocessor::add_warp(Warp warp) {
    if (resident_warps_.size() < max_resident_warps_) {
        warp.creation_cycle = current_cycle_;
        warp.state = WarpState::READY;
        resident_warps_.push_back(std::move(warp));
    }
}

int StreamingMultiprocessor::select_eligible_warp() {
    if (resident_warps_.empty()) return -1;
    
    if (policy_ == WarpPolicy::GREEDY_THEN_OLDEST) {
        // 1. Greedy: if previous warp is still ready, prioritize continuing it
        if (last_scheduled_warp_idx_ < resident_warps_.size()) {
            const auto& prev = resident_warps_[last_scheduled_warp_idx_];
            if (prev.state == WarpState::READY && !prev.is_done()) {
                return static_cast<int>(last_scheduled_warp_idx_);
            }
        }
        
        // 2. Oldest: select earliest created eligible ready warp
        int oldest_idx = -1;
        uint64_t oldest_cycle = UINT64_MAX;
        
        for (size_t i = 0; i < resident_warps_.size(); ++i) {
            const auto& w = resident_warps_[i];
            if (w.state == WarpState::READY && !w.is_done()) {
                if (w.creation_cycle < oldest_cycle) {
                    oldest_cycle = w.creation_cycle;
                    oldest_idx = static_cast<int>(i);
                }
            }
        }
        return oldest_idx;
    } else {
        // Round Robin: search cyclically from next index
        size_t n = resident_warps_.size();
        for (size_t i = 1; i <= n; ++i) {
            size_t idx = (last_scheduled_warp_idx_ + i) % n;
            if (resident_warps_[idx].state == WarpState::READY && !resident_warps_[idx].is_done()) {
                return static_cast<int>(idx);
            }
        }
        return -1;
    }
}

bool StreamingMultiprocessor::step_cycle() {
    current_cycle_++;
    
    // Count active (non-completed) warps for occupancy tracking
    uint32_t active_count = 0;
    for (const auto& w : resident_warps_) {
        if (!w.is_done()) {
            active_count++;
        }
    }
    
    if (active_count == 0) {
        return false; // All warps completed
    }
    
    metrics_.average_active_warps += active_count;
    
    // 1. Progress memory stalls for all waiting warps
    for (auto& w : resident_warps_) {
        if (w.state == WarpState::STALLED_MEMORY) {
            w.total_stall_cycles++;
            if (w.stall_cycles_remaining > 0) {
                w.stall_cycles_remaining--;
            }
            if (w.stall_cycles_remaining == 0) {
                w.state = WarpState::READY;
            }
        }
    }
    
    // 2. Select eligible warp to issue instruction
    int selected = select_eligible_warp();
    
    if (selected >= 0) {
        auto& w = resident_warps_[selected];
        last_scheduled_warp_idx_ = selected;
        
        const auto& inst = w.instructions[w.pc];
        w.pc++;
        w.total_issued_instructions++;
        metrics_.instructions_issued++;
        
        if (inst.is_memory_access) {
            w.state = WarpState::STALLED_MEMORY;
            w.stall_cycles_remaining = inst.memory_latency_cycles;
            metrics_.memory_stall_cycles++;
        } else {
            w.state = w.is_done() ? WarpState::COMPLETED : WarpState::READY;
        }
    } else {
        // No eligible warp could issue this cycle (pipeline stall)
        metrics_.idle_no_eligible_warp_cycles++;
    }
    
    metrics_.total_cycles = current_cycle_;
    return true;
}

void StreamingMultiprocessor::run() {
    while (step_cycle()) {}
    
    if (metrics_.total_cycles > 0) {
        metrics_.average_active_warps /= static_cast<double>(metrics_.total_cycles);
        metrics_.warp_occupancy = max_resident_warps_ > 0 
            ? metrics_.average_active_warps / max_resident_warps_ 
            : 0.0;
        metrics_.instruction_issue_rate = static_cast<double>(metrics_.instructions_issued) / metrics_.total_cycles;
        
        double unhidden_stall_fraction = static_cast<double>(metrics_.idle_no_eligible_warp_cycles) / metrics_.total_cycles;
        metrics_.latency_hiding_efficiency = std::max(0.0, 1.0 - unhidden_stall_fraction);
    }
}

void StreamingMultiprocessor::print_report() const {
    std::cout << "\n================= NVIDIA Streaming Multiprocessor (SM " << sm_id_ << ") Simulation =================\n\n";
    std::cout << "Scheduling Policy      : " << (policy_ == WarpPolicy::GREEDY_THEN_OLDEST ? "Greedy-Then-Oldest (GTO)" : "Round Robin") << "\n";
    std::cout << "Max Resident Warps     : " << max_resident_warps_ << " (" << (max_resident_warps_ * 32) << " threads)\n";
    std::cout << "Resident Warps Loaded  : " << resident_warps_.size() << " (" << (resident_warps_.size() * 32) << " threads)\n";
    std::cout << "Total Elapsed Cycles   : " << metrics_.total_cycles << "\n";
    std::cout << "Instructions Issued    : " << metrics_.instructions_issued << "\n";
    std::cout << "IPC (Instructions/Cyc) : " << std::fixed << std::setprecision(3) << metrics_.instruction_issue_rate << "\n";
    std::cout << "SM Warp Occupancy      : " << std::fixed << std::setprecision(1) << (metrics_.warp_occupancy * 100.0) << "%\n";
    std::cout << "Pipeline Stall Cycles  : " << metrics_.idle_no_eligible_warp_cycles 
              << " (" << std::fixed << std::setprecision(1) << (static_cast<double>(metrics_.idle_no_eligible_warp_cycles) / std::max(1ULL, metrics_.total_cycles) * 100.0) << "%)\n";
    std::cout << "Latency Hiding Effic.  : " << std::fixed << std::setprecision(1) << (metrics_.latency_hiding_efficiency * 100.0) << "%\n";
    std::cout << std::string(84, '-') << "\n\n";
}

} // namespace scheduler::gpu
