#pragma once

#include <vector>
#include <cstdint>
#include <string>

namespace scheduler {

class Core;

struct CorePowerMetrics {
    int core_id;
    double speed;
    double busy_power_watts;
    double idle_power_watts;
    double total_energy_joules;
    double average_power_watts;
};

struct SystemPowerReport {
    double total_energy_joules = 0.0;
    double average_power_watts = 0.0;
    double peak_power_watts = 0.0;
    double energy_per_process_joules = 0.0;
    double performance_per_watt = 0.0; // Throughput / Average Power
    std::vector<CorePowerMetrics> core_power_metrics;
};

class PowerModel {
public:
    // Power parameters in Watts:
    // P_static = leakage baseline (e.g. 1.0W)
    // P_dynamic_base = dynamic power at 1.0x speed (e.g. 15.0W)
    // Dynamic power scales with speed^3 (C * V^2 * f where V proportional to f)
    explicit PowerModel(double static_power_watts = 1.0, 
                        double dynamic_base_watts = 15.0);
                        
    SystemPowerReport calculate_power(const std::vector<Core>& cores, 
                                     uint32_t total_processes, 
                                     uint32_t total_time,
                                     double throughput) const;
                                     
    void print_power_report(const SystemPowerReport& report) const;

private:
    double static_power_watts_;
    double dynamic_base_watts_;
    
    double compute_busy_power(double speed) const;
};

} // namespace scheduler
