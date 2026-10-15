#include "WorkloadLoader.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <random>
#include <algorithm>
#include <cmath>

namespace scheduler {

WorkloadDistribution WorkloadLoader::parse_distribution(const std::string& name) {
    if (name == "exponential" || name == "exp") {
        return WorkloadDistribution::EXPONENTIAL;
    } else if (name == "bimodal" || name == "bim") {
        return WorkloadDistribution::BIMODAL;
    }
    return WorkloadDistribution::UNIFORM;
}

std::string WorkloadLoader::distribution_to_string(WorkloadDistribution dist) {
    switch (dist) {
        case WorkloadDistribution::EXPONENTIAL: return "exponential";
        case WorkloadDistribution::BIMODAL: return "bimodal";
        default: return "uniform";
    }
}

std::vector<std::unique_ptr<Process>> WorkloadLoader::load_from_csv(const std::string& filename) {
    std::vector<std::unique_ptr<Process>> processes;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open workload file: " + filename);
    }
    
    std::string line;
    bool first_line = true;
    
    while (std::getline(file, line)) {
        // Skip header if present
        if (first_line && line.find("pid") != std::string::npos) {
            first_line = false;
            continue;
        }
        first_line = false;
        
        // Skip empty lines
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        std::stringstream ss(line);
        std::string token;
        std::vector<std::string> tokens;
        
        while (std::getline(ss, token, ',')) {
            tokens.push_back(token);
        }
        
        if (tokens.size() < 4) {
            throw std::runtime_error("Invalid CSV line: " + line);
        }
        
        uint32_t pid = std::stoul(tokens[0]);
        std::string name = tokens[1];
        uint32_t arrival = std::stoul(tokens[2]);
        uint32_t burst = std::stoul(tokens[3]);
        int priority = (tokens.size() > 4) ? std::stoi(tokens[4]) : 0;
        
        processes.push_back(std::make_unique<Process>(pid, name, arrival, burst, priority));
    }
    
    return processes;
}

std::vector<std::unique_ptr<Process>> WorkloadLoader::clone_workload(
    const std::vector<std::unique_ptr<Process>>& source) {
    std::vector<std::unique_ptr<Process>> clone;
    clone.reserve(source.size());
    for (const auto& proc : source) {
        clone.push_back(std::make_unique<Process>(
            proc->pid, proc->name, proc->arrival_time, proc->burst_time, proc->priority));
    }
    return clone;
}

std::vector<std::unique_ptr<Process>> WorkloadLoader::generate_random(const GeneratorConfig& config) {
    std::vector<std::unique_ptr<Process>> processes;
    processes.reserve(config.num_processes);
    std::mt19937 rng(config.seed);
    
    std::uniform_int_distribution<int> priority_dist(0, 10);
    
    if (config.distribution == WorkloadDistribution::EXPONENTIAL) {
        std::exponential_distribution<double> inter_arrival_dist(config.arrival_lambda);
        std::exponential_distribution<double> burst_dist(config.burst_lambda);
        
        double current_time = 0.0;
        for (uint32_t i = 0; i < config.num_processes; ++i) {
            if (i > 0) {
                current_time += inter_arrival_dist(rng);
            }
            uint32_t arrival = static_cast<uint32_t>(std::round(current_time));
            
            double burst_val = burst_dist(rng);
            uint32_t burst = std::max(1u, static_cast<uint32_t>(std::round(burst_val)));
            int priority = config.use_priorities ? priority_dist(rng) : 0;
            
            std::string name = "P" + std::to_string(i + 1);
            processes.push_back(std::make_unique<Process>(i + 1, name, arrival, burst, priority));
        }
    } else if (config.distribution == WorkloadDistribution::BIMODAL) {
        std::uniform_int_distribution<uint32_t> arrival_dist(0, config.max_arrival_time);
        std::uniform_real_distribution<double> choice_dist(0.0, 1.0);
        std::uniform_int_distribution<uint32_t> short_burst_dist(config.min_burst_time, config.bimodal_short_max);
        std::uniform_int_distribution<uint32_t> long_burst_dist(config.bimodal_long_min, config.bimodal_long_max);
        
        for (uint32_t i = 0; i < config.num_processes; ++i) {
            uint32_t arrival = arrival_dist(rng);
            uint32_t burst = (choice_dist(rng) < config.bimodal_ratio) 
                ? short_burst_dist(rng) 
                : long_burst_dist(rng);
            int priority = config.use_priorities ? priority_dist(rng) : 0;
            
            std::string name = "P" + std::to_string(i + 1);
            processes.push_back(std::make_unique<Process>(i + 1, name, arrival, burst, priority));
        }
    } else {
        // Uniform distribution (default)
        std::uniform_int_distribution<uint32_t> arrival_dist(0, config.max_arrival_time);
        std::uniform_int_distribution<uint32_t> burst_dist(config.min_burst_time, config.max_burst_time);
        
        for (uint32_t i = 0; i < config.num_processes; ++i) {
            uint32_t arrival = arrival_dist(rng);
            uint32_t burst = burst_dist(rng);
            int priority = config.use_priorities ? priority_dist(rng) : 0;
            
            std::string name = "P" + std::to_string(i + 1);
            processes.push_back(std::make_unique<Process>(i + 1, name, arrival, burst, priority));
        }
    }
    
    // Stable sort by arrival time, tie break by PID
    std::sort(processes.begin(), processes.end(),
              [](const auto& a, const auto& b) {
                  if (a->arrival_time != b->arrival_time) {
                      return a->arrival_time < b->arrival_time;
                  }
                  return a->pid < b->pid;
              });
    
    return processes;
}

void WorkloadLoader::save_to_csv(const std::vector<std::unique_ptr<Process>>& processes,
                                 const std::string& filename) {
    std::ofstream file(filename);
    
    if (!file.is_open()) {
        throw std::runtime_error("Failed to create file: " + filename);
    }
    
    file << "pid,name,arrival_time,burst_time,priority\n";
    
    for (const auto& proc : processes) {
        file << proc->pid << ","
             << proc->name << ","
             << proc->arrival_time << ","
             << proc->burst_time << ","
             << proc->priority << "\n";
    }
}

} // namespace scheduler
