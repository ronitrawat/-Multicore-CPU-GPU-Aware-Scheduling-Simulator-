#include "Process.h"
#include <limits>

namespace scheduler {

Process::Process(uint32_t pid, const std::string& name, uint32_t arrival,
                 uint32_t burst, int priority)
    : pid(pid),
      name(name),
      arrival_time(arrival),
      burst_time(burst),
      remaining_time(static_cast<double>(burst)),
      priority(priority),
      state(ProcessState::NEW),
      first_response_time(std::numeric_limits<uint32_t>::max()),
      completion_time(0),
      waiting_time(0),
      ready_enter_time(arrival),
      last_core_id(-1),
      age_counter(0),
      migration_count(0),
      migration_penalty_remaining(0) {}

uint32_t Process::turnaround_time() const {
    return completion_time - arrival_time;
}

uint32_t Process::response_time() const {
    if (first_response_time == std::numeric_limits<uint32_t>::max()) {
        return 0;  // Not yet scheduled
    }
    return first_response_time - arrival_time;
}

} // namespace scheduler
