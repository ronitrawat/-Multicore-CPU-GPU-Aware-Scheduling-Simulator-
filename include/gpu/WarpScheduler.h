#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <memory>

namespace scheduler::gpu {

enum class WarpState {
    READY,
    RUNNING,
    STALLED_MEMORY,
    COMPLETED
};

enum class WarpPolicy {
    ROUND_ROBIN,
    GREEDY_THEN_OLDEST // GTO: NVIDIA hardware default
};

struct SimulatedInstruction {
    bool is_memory_access;
    uint32_t memory_latency_cycles; // e.g., 50-200 cycles for global DRAM
};

struct Warp {
    uint32_t warp_id;
    uint32_t block_id;
    uint32_t thread_count = 32; // Hardware standard 32 threads per warp
    
    std::vector<SimulatedInstruction> instructions;
    size_t pc = 0; // Program counter index
    
    WarpState state = WarpState::READY;
    uint32_t stall_cycles_remaining = 0;
    uint64_t creation_cycle = 0;
    
    uint64_t total_issued_instructions = 0;
    uint64_t total_stall_cycles = 0;
    
    bool is_done() const { return pc >= instructions.size(); }
};

struct SMMetrics {
    uint64_t total_cycles = 0;
    uint64_t instructions_issued = 0;
    uint64_t memory_stall_cycles = 0;
    uint64_t idle_no_eligible_warp_cycles = 0;
    double average_active_warps = 0.0;
    double warp_occupancy = 0.0;             // active warps / max resident warps
    double instruction_issue_rate = 0.0;     // instructions / total cycles
    double latency_hiding_efficiency = 0.0;  // % of memory stalls hidden by other warps
};

class StreamingMultiprocessor {
public:
    StreamingMultiprocessor(uint32_t sm_id, 
                           uint32_t max_resident_warps = 32,
                           WarpPolicy policy = WarpPolicy::GREEDY_THEN_OLDEST);
                           
    void add_warp(Warp warp);
    
    // Simulate one hardware clock cycle
    bool step_cycle();
    
    // Run until all warps complete execution
    void run();
    
    // Accessors
    const SMMetrics& metrics() const { return metrics_; }
    WarpPolicy policy() const { return policy_; }
    uint32_t sm_id() const { return sm_id_; }
    void print_report() const;

private:
    uint32_t sm_id_;
    uint32_t max_resident_warps_;
    WarpPolicy policy_;
    
    std::vector<Warp> resident_warps_;
    size_t last_scheduled_warp_idx_ = 0;
    uint64_t current_cycle_ = 0;
    
    SMMetrics metrics_;
    
    // Select warp to issue next according to policy
    int select_eligible_warp();
};

} // namespace scheduler::gpu
