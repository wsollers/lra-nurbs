// simulation/events/EventLog.cpp
#include "simulation/events/EventLog.hpp"
#include <format>
#include <iostream>

namespace ndde::events {

void EventLog::init(u64 capacity_records, u64 max_display) {
    m_max_display = max_display;
    const u64 slab_bytes = capacity_records * static_cast<u64>(sizeof(EventRecord));
    m_slab.resize(static_cast<std::size_t>(slab_bytes));
    m_ring.attach(m_slab.data(), slab_bytes);
    m_drain_scratch.resize(static_cast<std::size_t>(capacity_records));
    m_display.reserve(static_cast<std::size_t>(max_display));
    std::cout << std::format("[EventLog] Init — {} records ({} KB slab)\n",
        capacity_records, slab_bytes / u64(1024));
}

void EventLog::destroy() noexcept {
    m_ring.detach();
    m_slab.clear();
    m_slab.shrink_to_fit();
    m_drain_scratch.clear();
    m_drain_scratch.shrink_to_fit();
    m_display.clear();
}

void EventLog::reset() noexcept {
    m_ring.reset();
    m_last_dropped = u64(0);
    // Keep display entries — the user may still want to see previous run output.
    // Call clear_display() explicitly to wipe the panel.
}

void EventLog::drain(f32 sim_time, u64 tick) {
    if (m_ring.empty()) return;

    const u64 n = m_ring.consume_all(m_drain_scratch);
    for (u64 i = u64(0); i < n; ++i) {
        evict_oldest_if_full();
        m_display.push_back(LogEntry{
            .kind     = m_drain_scratch[static_cast<std::size_t>(i)].kind,
            .severity = m_drain_scratch[static_cast<std::size_t>(i)].severity,
            .sim_time = m_drain_scratch[static_cast<std::size_t>(i)].sim_time,
            .text     = format_record(m_drain_scratch[static_cast<std::size_t>(i)])
        });
    }

    // Emit synthetic dropped-events entry if any were lost since last drain.
    const u64 dropped_now = m_ring.dropped();
    if (dropped_now > m_last_dropped) {
        const u64 new_drops = dropped_now - m_last_dropped;
        evict_oldest_if_full();
        m_display.push_back(LogEntry{
            .kind     = EventKind::EventsDropped,
            .severity = EventSeverity::Warning,
            .sim_time = sim_time,
            .text     = std::format("[{:>7.2f}] WARN   {} events dropped (ring full)",
                                     sim_time, new_drops)
        });
        m_last_dropped = dropped_now;
    }
}

void EventLog::push_engine_string(std::string msg, EventSeverity sev) {
    evict_oldest_if_full();
    m_display.push_back(LogEntry{
        .kind     = EventKind::AppStarted,   // generic engine kind
        .severity = sev,
        .sim_time = f32(0),
        .text     = std::move(msg)
    });
}

void EventLog::evict_oldest_if_full() {
    if (static_cast<u64>(m_display.size()) >= m_max_display && !m_display.empty())
        m_display.erase(m_display.begin());
}

std::string EventLog::format_record(const EventRecord& r) {
    const auto& lv = r.label_view();
    switch (r.kind) {
    case EventKind::AppStarted:
        return std::format("[Engine  ] AppStarted");
    case EventKind::AppStopping:
        return std::format("[Engine  ] AppStopping");
    case EventKind::SimSwitched:
        return std::format("[Engine  ] SimSwitched → {}", lv);
    case EventKind::SimStarted:
        return std::format("[{:>7.2f}] INFO   SimStarted: {}", r.sim_time, lv);
    case EventKind::SimReset:
        return std::format("[{:>7.2f}] INFO   SimReset: {}", r.sim_time, lv);
    case EventKind::SimStopped:
        return std::format("[{:>7.2f}] INFO   SimStopped: {} ticks={}", r.sim_time, lv, r.tick);
    case EventKind::AgentSpawned:
        return std::format("[{:>7.2f}] SPAWN  {} ({:.3f}, {:.3f})",
            r.sim_time, lv, r.val_c, r.val_d);
    case EventKind::AgentDespawned:
        return std::format("[{:>7.2f}] DESPAWN id={}", r.sim_time, r.id_a);
    case EventKind::AgentCaptured:
        return std::format("[{:>7.2f}] CAPTURE pursuer={} prey={} d={:.3f}",
            r.sim_time, r.id_a, r.id_b, r.val_a);
    case EventKind::PerturbationFired:
        return std::format("[{:>7.2f}] POKE   ({:.3f}, {:.3f}) A={:.3f}",
            r.sim_time, r.val_c, r.val_d, r.val_a);
    case EventKind::PerturbationDecayed:
        return std::format("[{:>7.2f}] DECAY  ({:.3f}, {:.3f})",
            r.sim_time, r.val_c, r.val_d);
    case EventKind::FieldAdded:
        return std::format("[{:>7.2f}] FIELD+ {}", r.sim_time, lv);
    case EventKind::FieldRemoved:
        return std::format("[{:>7.2f}] FIELD- {}", r.sim_time, lv);
    case EventKind::GeodesicBifurcation:
        return std::format("[{:>7.2f}] BIFURC pursuer={} d_old={:.3f} d_new={:.3f}",
            r.sim_time, r.id_a, r.val_a, r.val_b);
    case EventKind::AlertPreyProximity:
        return std::format("[{:>7.2f}] WARN   Proximity d={:.3f} < thr={:.3f}",
            r.sim_time, r.val_a, r.val_b);
    case EventKind::AlertPreyEscape:
        return std::format("[{:>7.2f}] WARN   EscapeAlert d={:.3f} > thr={:.3f}",
            r.sim_time, r.val_a, r.val_b);
    case EventKind::AlertStealthLost:
        return std::format("[{:>7.2f}] ALERT  StealthLost id={} κ_g={:.3f} > {:.3f}",
            r.sim_time, r.id_a, r.val_a, r.val_b);
    case EventKind::AlertStealthGained:
        return std::format("[{:>7.2f}] NOTICE StealthGained id={} κ_g={:.3f}",
            r.sim_time, r.id_a, r.val_a);
    case EventKind::AlertCapturePending:
        return std::format("[{:>7.2f}] ALERT  CapturePending ttc={:.2f}s",
            r.sim_time, r.val_a);
    case EventKind::AlertCustom:
        return std::format("[{:>7.2f}] ALERT  {} a={:.3f} b={:.3f}",
            r.sim_time, lv, r.val_a, r.val_b);
    case EventKind::EventsDropped:
        return std::format("[{:>7.2f}] WARN   {:.0f} events dropped (ring full)",
            r.sim_time, r.val_a);
    default:
        return std::format("[{:>7.2f}] EVENT  {} k={}",
            r.sim_time, lv, static_cast<u8>(r.kind));
    }
}

} // namespace ndde::events
