#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace scheduler::gpu {

enum class CudaTaskType {
    KERNEL_EXECUTION,
    MEMCPY_H2D, // Host to Device transfer
    MEMCPY_D2H  // Device to Host transfer
};

struct StreamTask {
    uint32_t task_id;
    std::string name;
    uint32_t stream_id;
    CudaTaskType type;
    uint32_t duration; // Execution time units
    
    std::vector<uint32_t> dependencies; // Task IDs that must complete before this starts
    
    // Execution timeline
    uint32_t start_time = 0;
    uint32_t completion_time = 0;
    bool is_completed = false;
    bool is_running = false;
};

struct CudaStreamMetrics {
    uint32_t makespan = 0;                 // Total execution time with concurrent streams
    uint32_t total_serialized_duration = 0; // Sequential execution time
    double stream_concurrency_speedup = 1.0;
    size_t num_streams = 0;
    size_t num_tasks = 0;
};

class CudaStreamGraphEngine {
public:
    CudaStreamGraphEngine(uint32_t num_kernel_engines = 2, uint32_t num_copy_engines = 2);
    
    uint32_t add_task(const std::string& name, 
                      uint32_t stream_id, 
                      CudaTaskType type, 
                      uint32_t duration,
                      std::vector<uint32_t> dependencies = {});
                      
    // Helper to simulate cudaStreamWaitEvent
    void add_stream_wait_event(uint32_t waiting_stream_id, uint32_t recorded_task_id);
    
    // Execute the CUDA stream dependency graph
    void run();
    
    const CudaStreamMetrics& metrics() const { return metrics_; }
    const std::vector<StreamTask>& tasks() const { return tasks_; }
    void print_summary() const;

private:
    uint32_t num_kernel_engines_;
    uint32_t num_copy_engines_;
    
    std::vector<StreamTask> tasks_;
    std::unordered_map<uint32_t, size_t> task_index_map_;
    
    CudaStreamMetrics metrics_;
};

} // namespace scheduler::gpu
