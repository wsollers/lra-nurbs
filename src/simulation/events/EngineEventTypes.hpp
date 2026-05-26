#pragma once
// simulation/events/EngineEventTypes.hpp
// Canonical app-scoped event PODs fired on the EngineEventBus.
// engine/events/AppEvent.hpp is now a shim that includes this file.
// wall_time preserved for TelemetryService compatibility.

#include "math/Scalars.hpp"
#include <chrono>

namespace ndde::events {

struct AppStarted {
    std::chrono::system_clock::time_point wall_time
        = std::chrono::system_clock::now();
};

struct AppStopping {};

struct SimSwitched {
    u64              sim_index = u64(0);
};

} // namespace ndde::events
