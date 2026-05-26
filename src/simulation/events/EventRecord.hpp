#pragma once
// simulation/events/EventRecord.hpp
// Fixed-size, trivially copyable event record for the SPSC ring.
// No heap, no std::string, no std::any. Two cache lines (128 bytes).
//
// Layout (all offsets verified by static_assert below):
//   0      kind        EventKind  (u8)
//   1      severity    EventSeverity (u8)
//   2-3    _pad        (u8 x 2)
//   4-7    sim_time    (f32)
//   8-15   tick        (u64)
//  16-23   id_a        primary packed_id or 0 (u64)
//  24-31   id_b        secondary packed_id or 0 (u64)
//  32-35   val_a       f32 payload (kind-dependent)
//  36-39   val_b       f32 payload
//  40-43   val_c       f32 payload (uv.x of event point)
//  44-47   val_d       f32 payload (uv.y of event point)
//  48-111  label       char[64]  null-terminated short description
// 112-119  sequence    per-channel sequence assigned by EventBusService
// 120-127  _reserved   u8[8]     Phase 2 (metric factor, etc.)
// Total: 128 bytes = 2 cache lines

#include "math/Scalars.hpp"
#include <array>
#include <cstring>
#include <string_view>

namespace ndde::events {

// ── EventKind ─────────────────────────────────────────────────────────────────
enum class EventKind : u8 {
    // Engine-scoped
    AppStarted          = u8(0),
    AppStopping         = u8(1),
    SimSwitched         = u8(2),

    // Sim lifecycle
    SimStarted          = u8(10),
    SimReset            = u8(11),
    SimStopped          = u8(12),

    // Agent
    AgentSpawned        = u8(20),
    AgentDespawned      = u8(21),
    AgentCaptured       = u8(22),

    // Perturbation
    PerturbationFired   = u8(30),
    PerturbationDecayed = u8(31),

    // Field
    FieldAdded          = u8(40),
    FieldRemoved        = u8(41),

    // Geometry
    GeodesicBifurcation = u8(50),

    // Alerts
    AlertPreyProximity  = u8(60),
    AlertPreyEscape     = u8(61),
    AlertStealthGained  = u8(62),
    AlertStealthLost    = u8(63),
    AlertCapturePending = u8(64),
    AlertCustom         = u8(65),

    // Diagnostic
    EventsDropped       = u8(255),
};

// ── EventSeverity ─────────────────────────────────────────────────────────────
enum class EventSeverity : u8 {
    Info     = u8(0),   // grey   — lifecycle
    Notice   = u8(1),   // cyan   — agent / field events
    Warning  = u8(2),   // yellow — threshold crossings
    Alert    = u8(3),   // orange — capture pending, stealth lost
    Critical = u8(4),   // red    — capture achieved
};

// ── EventRecord ───────────────────────────────────────────────────────────────
struct EventRecord {
    EventKind     kind      = EventKind::AppStarted;
    EventSeverity severity  = EventSeverity::Info;
    u8            _pad[2]   = {};
    f32           sim_time  = f32(0);
    u64           tick      = u64(0);
    u64           id_a      = u64(0);
    u64           id_b      = u64(0);
    f32           val_a     = f32(0);
    f32           val_b     = f32(0);
    f32           val_c     = f32(0);   // uv.x
    f32           val_d     = f32(0);   // uv.y
    char          label[64] = {};
    u64           sequence  = u64(0);
    u8            _reserved[8] = {};

    // Write into label without heap allocation. Truncates silently.
    void set_label(std::string_view s) noexcept {
        const std::size_t n = s.size() < sizeof(label) - 1u
                            ? s.size() : sizeof(label) - 1u;
        std::memcpy(label, s.data(), n);
        label[n] = '\0';
    }

    [[nodiscard]] std::string_view label_view() const noexcept {
        return std::string_view{label};
    }
};

static_assert(sizeof(EventRecord)  == 128,
    "EventRecord must be exactly 128 bytes (2 cache lines). "
    "Check field types — every field must use ndde scalar aliases.");
static_assert(alignof(EventRecord) == 8,
    "EventRecord must be 8-byte aligned for SPSC ring correctness.");
static_assert(offsetof(EventRecord, kind)      ==   0);
static_assert(offsetof(EventRecord, severity)  ==   1);
static_assert(offsetof(EventRecord, sim_time)  ==   4);
static_assert(offsetof(EventRecord, tick)      ==   8);
static_assert(offsetof(EventRecord, id_a)      ==  16);
static_assert(offsetof(EventRecord, id_b)      ==  24);
static_assert(offsetof(EventRecord, val_a)     ==  32);
static_assert(offsetof(EventRecord, val_b)     ==  36);
static_assert(offsetof(EventRecord, val_c)     ==  40);
static_assert(offsetof(EventRecord, val_d)     ==  44);
static_assert(offsetof(EventRecord, label)     ==  48);
static_assert(offsetof(EventRecord, sequence)  == 112);
static_assert(offsetof(EventRecord, _reserved) == 120);

// ── Named builders ────────────────────────────────────────────────────────────

[[nodiscard]] inline EventRecord make_sim_started(
        u64 scenario_id, f32 sim_time, u64 tick) noexcept {
    EventRecord r;
    r.kind = EventKind::SimStarted; r.severity = EventSeverity::Info;
    r.sim_time = sim_time; r.tick = tick;
    r.id_a = scenario_id;
    r.set_label("ScenarioStarted");
    return r;
}

[[nodiscard]] inline EventRecord make_sim_reset(
        u64 scenario_id, f32 sim_time, u64 tick) noexcept {
    EventRecord r;
    r.kind = EventKind::SimReset; r.severity = EventSeverity::Info;
    r.sim_time = sim_time; r.tick = tick;
    r.id_a = scenario_id;
    r.set_label("ScenarioReset");
    return r;
}

[[nodiscard]] inline EventRecord make_agent_spawned(
        u64 packed_id, u64 recipe_id, f32 u, f32 v,
        f32 sim_time, u64 tick) noexcept {
    EventRecord r;
    r.kind = EventKind::AgentSpawned; r.severity = EventSeverity::Notice;
    r.sim_time = sim_time; r.tick = tick;
    r.id_a = packed_id; r.id_b = recipe_id; r.val_c = u; r.val_d = v;
    r.set_label("AgentSpawned");
    return r;
}

[[nodiscard]] inline EventRecord make_agent_captured(
        u64 pursuer_id, u64 prey_id, f32 distance,
        f32 sim_time, u64 tick) noexcept {
    EventRecord r;
    r.kind = EventKind::AgentCaptured; r.severity = EventSeverity::Critical;
    r.sim_time = sim_time; r.tick = tick;
    r.id_a = pursuer_id; r.id_b = prey_id; r.val_a = distance;
    r.set_label("AgentCaptured");
    return r;
}

[[nodiscard]] inline EventRecord make_perturbation_fired(
        f32 u, f32 v, f32 amplitude, f32 sim_time, u64 tick) noexcept {
    EventRecord r;
    r.kind = EventKind::PerturbationFired; r.severity = EventSeverity::Notice;
    r.sim_time = sim_time; r.tick = tick;
    r.val_a = amplitude; r.val_c = u; r.val_d = v;
    r.set_label("PerturbationFired");
    return r;
}

[[nodiscard]] inline EventRecord make_perturbation_decayed(
        f32 u, f32 v, f32 sim_time, u64 tick) noexcept {
    EventRecord r;
    r.kind = EventKind::PerturbationDecayed; r.severity = EventSeverity::Info;
    r.sim_time = sim_time; r.tick = tick;
    r.val_c = u; r.val_d = v;
    r.set_label("PerturbationDecayed");
    return r;
}

[[nodiscard]] inline EventRecord make_field_added(
        u64 field_id, f32 sim_time, u64 tick) noexcept {
    EventRecord r;
    r.kind = EventKind::FieldAdded; r.severity = EventSeverity::Notice;
    r.sim_time = sim_time; r.tick = tick; r.id_a = field_id;
    r.set_label("FieldAdded");
    return r;
}

[[nodiscard]] inline EventRecord make_field_removed(
        u64 field_id, f32 sim_time, u64 tick) noexcept {
    EventRecord r;
    r.kind = EventKind::FieldRemoved; r.severity = EventSeverity::Info;
    r.sim_time = sim_time; r.tick = tick; r.id_a = field_id;
    r.set_label("FieldRemoved");
    return r;
}

[[nodiscard]] inline EventRecord make_alert_proximity(
        u64 pursuer_id, u64 prey_id, f32 distance, f32 threshold,
        f32 sim_time, u64 tick) noexcept {
    EventRecord r;
    r.kind = EventKind::AlertPreyProximity; r.severity = EventSeverity::Warning;
    r.sim_time = sim_time; r.tick = tick;
    r.id_a = pursuer_id; r.id_b = prey_id;
    r.val_a = distance; r.val_b = threshold;
    r.set_label("AlertProximity");
    return r;
}

[[nodiscard]] inline EventRecord make_alert_escape(
        f32 distance, f32 threshold, f32 sim_time, u64 tick) noexcept {
    EventRecord r;
    r.kind = EventKind::AlertPreyEscape; r.severity = EventSeverity::Warning;
    r.sim_time = sim_time; r.tick = tick;
    r.val_a = distance; r.val_b = threshold;
    r.set_label("AlertEscape");
    return r;
}

[[nodiscard]] inline EventRecord make_alert_stealth(
        u64 agent_id, f32 kappa_g, f32 kappa_max, bool lost,
        f32 sim_time, u64 tick) noexcept {
    EventRecord r;
    r.kind     = lost ? EventKind::AlertStealthLost : EventKind::AlertStealthGained;
    r.severity = lost ? EventSeverity::Alert        : EventSeverity::Notice;
    r.sim_time = sim_time; r.tick = tick;
    r.id_a = agent_id; r.val_a = kappa_g; r.val_b = kappa_max;
    r.set_label(lost ? "StealthLost" : "StealthGained");
    return r;
}

[[nodiscard]] inline EventRecord make_alert_capture_pending(
        u64 pursuer_id, u64 prey_id, f32 seconds_to_capture,
        f32 sim_time, u64 tick) noexcept {
    EventRecord r;
    r.kind = EventKind::AlertCapturePending; r.severity = EventSeverity::Alert;
    r.sim_time = sim_time; r.tick = tick;
    r.id_a = pursuer_id; r.id_b = prey_id; r.val_a = seconds_to_capture;
    r.set_label("CapturePending");
    return r;
}

[[nodiscard]] inline EventRecord make_alert_custom(
        std::string_view name, f32 val_a, f32 val_b,
        EventSeverity severity, f32 sim_time, u64 tick) noexcept {
    EventRecord r;
    r.kind = EventKind::AlertCustom; r.severity = severity;
    r.sim_time = sim_time; r.tick = tick;
    r.val_a = val_a; r.val_b = val_b;
    r.set_label(name);
    return r;
}

[[nodiscard]] inline EventRecord make_events_dropped(
        u64 count, f32 sim_time, u64 tick) noexcept {
    EventRecord r;
    r.kind = EventKind::EventsDropped; r.severity = EventSeverity::Warning;
    r.sim_time = sim_time; r.tick = tick;
    r.val_a = static_cast<f32>(count);
    r.set_label("EventsDropped");
    return r;
}

} // namespace ndde::events
