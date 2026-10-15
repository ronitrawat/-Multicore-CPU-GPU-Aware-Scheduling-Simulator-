#include "PowerModel.h"
#include "Core.h"
#include <iostream>
#include <iomanip>
#include <cmath>

namespace scheduler {

PowerModel::PowerModel(double static_power_watts, double dynamic_base_watts)
    : static_power_watts_(static_power_watts),
      dynamic_base_watts_(dynamic_base_watts) {}

double PowerModel::compute_busy_power(double speed) const {
    // P_busy = P_static + P_dynamic_base * (speed^3)
    return static_power_watts_ + dynamic_base_watts_ * std::pow(speed, 3.0);
}

SystemPowerReport PowerModel::calculate_power(const std::vector<Core>& cores, 
                                            uint32_t total_processes, 
                                            uint32_t total_time,
                                            double throughput) const {
    SystemPowerReport report;
    if (cores.empty() || total_time == 0) return report;
    
    double total_system_energy = 0.0;
    double total_peak_power = 0.0;
    
    for (const auto& core : cores) {
        CorePowerMetrics cpm;
        cpm.core_id = core.id();
        cpm.speed = core.speed();
        cpm.idle_power_watts = static_power_watts_;
        cpm.busy_power_watts = compute_busy_power(core.speed());
        
        // Busy + context switch overhead consume active dynamic power
        uint32_t active_ticks = core.busy_time() + core.context_switch_overhead_time();
        uint32_t idle_ticks = core.idle_time();
        
        // Energy in Joules (assuming 1 tick = 1 millisecond or standardized unit)
        // E = P_busy * t_active + P_idle * t_idle
        double active_energy = cpm.busy_power_watts * active_ticks;
        double idle_energy = cpm.idle_power_watts * idle_ticks;
        cpm.total_energy_joules = active_energy + idle_energy;
        
        uint32_t total_core_ticks = active_ticks + idle_ticks;
        cpm.average_power_watts = total_core_ticks > 0 
            ? cpm.total_energy_joules / total_core_ticks 
            : 0.0;
            
        total_system_energy += cpm.total_energy_joules;
        total_peak_power += cpm.busy_power_watts;
        
        report.core_power_metrics.push_back(cpm);
    }
    
    report.total_energy_joules = total_system_energy;
    report.average_power_watts = total_time > 0 ? (total_system_energy / total_time) : 0.0;
    report.peak_power_watts = total_peak_power;
    report.energy_per_process_joules = total_processes > 0 
        ? total_system_energy / total_processes 
        : 0.0;
        
    // Performance per Watt = Throughput / Average Power
    report.performance_per_watt = report.average_power_watts > 0.0 
        ? throughput / report.average_power_watts 
        : 0.0;
        
    return report;
}

void PowerModel::print_power_report(const SystemPowerReport& report) const {
    std::cout << "\n================= Energy & Power Consumption Report =================\n\n";
    std::cout << "Total System Energy    : " << std::fixed << std::setprecision(2) 
              << report.total_energy_joules << " J (Watt-ticks)\n";
    std::cout << "Average System Power   : " << std::fixed << std::setprecision(2) 
              << report.average_power_watts << " W\n";
    std::cout << "Peak Active Power      : " << std::fixed << std::setprecision(2) 
              << report.peak_power_watts << " W\n";
    std::cout << "Energy per Process     : " << std::fixed << std::setprecision(2) 
              << report.energy_per_process_joules << " J/process\n";
    std::cout << "Performance per Watt   : " << std::fixed << std::setprecision(4) 
              << (report.performance_per_watt * 1000.0) << " (proc/sec)/kW\n\n";
              
    std::cout << std::setw(8) << "Core ID"
              << std::setw(10) << "Speed"
              << std::setw(14) << "Busy Power"
              << std::setw(14) << "Idle Power"
              << std::setw(16) << "Total Energy"
              << std::setw(14) << "Avg Power" << "\n";
    std::cout << std::string(76, '-') << "\n";
    
    for (const auto& c : report.core_power_metrics) {
        std::cout << std::setw(8) << c.core_id
                  << std::setw(10) << std::fixed << std::setprecision(2) << c.speed
                  << std::setw(12) << std::fixed << std::setprecision(2) << c.busy_power_watts << " W"
                  << std::setw(12) << std::fixed << std::setprecision(2) << c.idle_power_watts << " W"
                  << std::setw(14) << std::fixed << std::setprecision(2) << c.total_energy_joules << " J"
                  << std::setw(12) << std::fixed << std::setprecision(2) << c.average_power_watts << " W\n";
    }
    std::cout << std::string(76, '-') << "\n\n";
}

} // namespace scheduler
