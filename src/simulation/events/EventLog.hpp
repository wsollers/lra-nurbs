#pragma once
// simulation/events/EventLog.hpp
// Owns the EventRing slab and the display buffer for the engine log panel.
// drain() is the ONLY place std::format / string allocation happens.
// Called once per frame after the simulation tick completes.

#include "simulation/events/EventRecord.hpp"
#include "simulation/events/EventRing.hpp"
#include "math/Scalars.hpp"
#include <cstddef>
#include <string>
#include <vector>

namespace ndde::events {

struct LogEntry {
    EventKind     kind;
    EventSeverity severity;
    f32           sim_time;
    std::string   text;
};

class EventLog {
public:
    EventLog() = default;
    ~EventLog() { destroy(); }

    EventLog(const EventLog&)            = delete;
    EventLog& operator=(const EventLog&) = delete;
    EventLog(EventLog&&)                 = delete;
    EventLog& operator=(EventLog&&)      = delete;

    // Allocate ring slab and pre-size scratch buffer.
    // capacity_records: ring depth (default 4096 = 512 KB slab)
    // max_display:      oldest entries evicted when panel buffer is full
    void init(u64 capacity_records = u64(4096),
              u64 max_display      = u64(512));

    void destroy() noexcept;
    void reset()   noexcept;   // called on SimReset — clears ring + display

    [[nodiscard]] EventRing& ring() noexcept { return m_ring; }

    // Drain ring → format → append to display entries.
    // Also emits a synthetic EventsDropped entry if ring.dropped() > 0.
    // Call once per frame AFTER the sim tick.
    void drain(f32 sim_time, u64 tick);

    // Push a plain engine-scoped string (from EngineEventBus or Engine itself).
    // Main thread only — not via the ring.
    void push_engine_string(std::string msg, EventSeverity sev = EventSeverity::Info);

    [[nodiscard]] const std::vector<LogEntry>& entries() const noexcept {
        return m_display;
    }
    [[nodiscard]] u64  total_dropped()   const noexcept { return m_ring.dropped(); }
    [[nodiscard]] u64  capacity()        const noexcept { return m_ring.capacity(); }
    [[nodiscard]] u64  approx_queued()   const noexcept { return m_ring.approx_size(); }

    void clear_display() noexcept { m_display.clear(); }

    // Format a single record into a string (exposed for testing).
    [[nodiscard]] static std::string format_record(const EventRecord& r);

private:
    std::vector<std::byte>       m_slab;
    EventRing                    m_ring;
    std::vector<LogEntry>        m_display;
    std::vector<EventRecord>     m_drain_scratch;  // pre-allocated, reused
    u64                          m_max_display    = u64(512);
    u64                          m_last_dropped   = u64(0);

    void evict_oldest_if_full();
};

} // namespace ndde::events
