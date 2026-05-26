#pragma once
// engine/events/SimEvent.hpp
// Plain-aggregate simulation-lifecycle events fired by Engine.
// Zero heap, zero virtual dispatch, trivially copyable.

#include "math/Scalars.hpp"
#include <chrono>
#include <string_view>

namespace ndde::events {

struct SimStarted {
    std::string_view                      sim_name;
    u64                                   sim_index;
    std::chrono::system_clock::time_point wall_time;
};

struct SimStopped {
    std::string_view                      sim_name;
    u64                                   sim_index;
    f32                                   total_sim_time;  ///< simulated seconds
    u64                                   total_ticks;
    u64                                   total_records;
    u64                                   dropped_records;
    std::chrono::system_clock::time_point wall_time;
};

} // namespace ndde::events
