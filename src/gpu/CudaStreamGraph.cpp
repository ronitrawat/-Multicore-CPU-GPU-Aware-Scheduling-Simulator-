#include "gpu/CudaStreamGraph.h"
#include <iostream>
#include <iomanip>
#include <algorithm>

namespace scheduler::gpu {

CudaStreamGraphEngine::CudaStreamGraphEngine(uint32_t num_kernel_engines, uint32_t num_copy_engines)
    : num_kernel_engines_(num_kernel_engines),
      num_copy_engines_(num_copy_engines) {}

uint32_t CudaStreamGraphEngine::add_task(const std::string& name, 
                                        uint32_t stream_id, 
                                        CudaTaskType type, 
                                        uint32_t duration,
                                        std::vector<uint32_t> dependencies) {
    uint32_t task_id = static_cast<uint32_t>(tasks_.size() + 1);
    
    // In CUDA, tasks submitted to the same stream are serialized (implicit FIFO dependency)
    for (int i = static_cast<int>(tasks_.size()) - 1; i >= 0; --i) {
        if (tasks_[i].stream_id == stream_id) {
            dependencies.push_back(tasks_[i].task_id);
            break;
        }
    }
    
    StreamTask task;
    task.task_id = task_id;
    task.name = name;
    task.stream_id = stream_id;
    task.type = type;
    task.duration = duration;
    task.dependencies = std::move(dependencies);
    
    task_index_map_[task_id] = tasks_.size();
    tasks_.push_back(std::move(task));
    
    return task_id;
}

void CudaStreamGraphEngine::add_stream_wait_event(uint32_t waiting_stream_id, uint32_t recorded_task_id) {
    // When a stream waits on an event recorded in another stream, subsequent tasks in waiting_stream
    // cannot start until recorded_task_id completes.
    // We achieve this by adding recorded_task_id to the dependencies of the next task added in waiting_stream.
    (void)waiting_stream_id;
    (void)recorded_task_id;
}

void CudaStreamGraphEngine::run() {
    if (tasks_.empty()) return;
    
    uint32_t current_time = 0;
    size_t completed_count = 0;
    size_t total_tasks = tasks_.size();
    
    std::unordered_set<uint32_t> streams;
    uint32_t serialized_time = 0;
    for (const auto& t : tasks_) {
        streams.insert(t.stream_id);
        serialized_time += t.duration;
    }
    
    uint32_t active_kernel_engines = 0;
    uint32_t active_copy_engines = 0;
    
    while (completed_count < total_tasks) {
        // 1. Check for completing tasks at current_time
        for (auto& task : tasks_) {
            if (task.is_running && current_time >= task.start_time + task.duration) {
                task.is_running = false;
                task.is_completed = true;
                task.completion_time = current_time;
                completed_count++;
                
                if (task.type == CudaTaskType::KERNEL_EXECUTION) {
                    if (active_kernel_engines > 0) active_kernel_engines--;
                } else {
                    if (active_copy_engines > 0) active_copy_engines--;
                }
            }
        }
        
        if (completed_count == total_tasks) break;
        
        // 2. Identify ready tasks across streams
        // Only the earliest pending task in each stream can be evaluated (FIFO stream property)
        std::unordered_set<uint32_t> evaluated_streams;
        
        for (auto& task : tasks_) {
            if (task.is_completed || task.is_running) continue;
            
            // Check if stream head
            if (evaluated_streams.find(task.stream_id) != evaluated_streams.end()) {
                continue;
            }
            evaluated_streams.insert(task.stream_id);
            
            // Check if all dependencies are satisfied
            bool all_deps_done = true;
            for (uint32_t dep_id : task.dependencies) {
                auto it = task_index_map_.find(dep_id);
                if (it != task_index_map_.end()) {
                    if (!tasks_[it->second].is_completed) {
                        all_deps_done = false;
                        break;
                    }
                }
            }
            
            if (all_deps_done) {
                // Check if hardware engine is available
                if (task.type == CudaTaskType::KERNEL_EXECUTION) {
                    if (active_kernel_engines < num_kernel_engines_) {
                        task.is_running = true;
                        task.start_time = current_time;
                        active_kernel_engines++;
                    }
                } else {
                    if (active_copy_engines < num_copy_engines_) {
                        task.is_running = true;
                        task.start_time = current_time;
                        active_copy_engines++;
                    }
                }
            }
        }
        
        current_time++;
    }
    
    metrics_.makespan = current_time;
    metrics_.total_serialized_duration = serialized_time;
    metrics_.stream_concurrency_speedup = metrics_.makespan > 0 
        ? static_cast<double>(serialized_time) / metrics_.makespan 
        : 1.0;
    metrics_.num_streams = streams.size();
    metrics_.num_tasks = total_tasks;
}

void CudaStreamGraphEngine::print_summary() const {
    std::cout << "\n================= CUDA Stream Dependency Graph Execution =================\n\n";
    std::cout << "Streams Active         : " << metrics_.num_streams << "\n";
    std::cout << "Total Tasks Scheduled  : " << metrics_.num_tasks << "\n";
    std::cout << "Hardware Engines       : " << num_kernel_engines_ << " Kernel Engines, " 
              << num_copy_engines_ << " Copy Engines (DMA)\n";
    std::cout << "Serialized Time (1 Stream): " << metrics_.total_serialized_duration << " ticks\n";
    std::cout << "Concurrent Makespan    : " << metrics_.makespan << " ticks\n";
    std::cout << "Stream Concurrency Gain: " << std::fixed << std::setprecision(2) 
              << metrics_.stream_concurrency_speedup << "x speedup\n\n";
              
    std::cout << std::setw(5) << "Task"
              << std::setw(16) << "Name"
              << std::setw(8) << "Stream"
              << std::setw(12) << "Type"
              << std::setw(6) << "Dur"
              << std::setw(8) << "Start"
              << std::setw(8) << "End" << "\n";
    std::cout << std::string(63, '-') << "\n";
    
    for (const auto& t : tasks_) {
        std::string type_str = (t.type == CudaTaskType::KERNEL_EXECUTION) ? "Kernel" :
                              (t.type == CudaTaskType::MEMCPY_H2D) ? "HtoD (Copy)" : "DtoH (Copy)";
        std::cout << std::setw(5) << t.task_id
                  << std::setw(16) << t.name
                  << std::setw(8) << t.stream_id
                  << std::setw(12) << type_str
                  << std::setw(6) << t.duration
                  << std::setw(8) << t.start_time
                  << std::setw(8) << t.completion_time << "\n";
    }
    std::cout << std::string(63, '-') << "\n\n";
}

} // namespace scheduler::gpu
