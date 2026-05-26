#pragma once
// simulation/events/SimEventTypes.hpp
// Sim-scoped typed event PODs dispatched by SimContext / ScenarioBuilder.
// These are the structs that subscribe<E>() handlers receive.
// Payloads intentionally carry compact IDs and scalar data only. Human names,
// labels, and paths belong to metadata/resource/logger services.
//
// NOTE: The names SimStarted/SimStopped are already defined in
// engine/events/SimEvent.hpp (used by TelemetryService, with wall_time).
// The structs here are sim-domain events with different fields.
// They live in the ndde::simulation::events sub-namespace to avoid collision.

#include "engine/RuntimeIds.hpp"
#include "math/Scalars.hpp"

namespace ndde::simulation::events {

// ── Sim lifecycle ─────────────────────────────────────────────────────────────

struct ScenarioStarted {
    RuntimeNodeId scenario = {};
    f32              sim_time = f32(0);
    u64              tick     = u64(0);
};

struct ScenarioReset {
    RuntimeNodeId scenario = {};
    f32              sim_time = f32(0);
    u64              tick     = u64(0);
};

struct ScenarioStopped {
    RuntimeNodeId scenario = {};
    f32              sim_time    = f32(0);
    u64              total_ticks = u64(0);
};

// ── Agent events ──────────────────────────────────────────────────────────────

struct AgentSpawned {
    u64           packed_id = u64(0);
    RuntimeNodeId recipe = {};
    f32           u         = f32(0);
    f32           v         = f32(0);
    f32           sim_time  = f32(0);
    u64           tick      = u64(0);
};

struct AgentDespawned {
    u64 packed_id = u64(0);
    f32 sim_time  = f32(0);
    u64 tick      = u64(0);
};

struct AgentCaptured {
    u64 pursuer_id = u64(0);
    u64 prey_id    = u64(0);
    f32 distance   = f32(0);
    f32 sim_time   = f32(0);
    u64 tick       = u64(0);
};

// ── Perturbation ──────────────────────────────────────────────────────────────

struct PerturbationFired {
    f32 u         = f32(0);
    f32 v         = f32(0);
    f32 amplitude = f32(0);
    f32 omega     = f32(0);
    f32 k_wave    = f32(0);
    f32 alpha     = f32(0);
    f32 beta      = f32(0);
    f32 sim_time  = f32(0);
    u64 tick      = u64(0);
    u32 seed      = u32(0);
};

struct PerturbationDecayed {
    f32 u        = f32(0);
    f32 v        = f32(0);
    f32 sim_time = f32(0);
    u64 tick     = u64(0);
    u32 seed     = u32(0);
};

// ── Field events ──────────────────────────────────────────────────────────────

struct FieldAdded {
    RuntimeNodeId field = {};
    f32           sim_time = f32(0);
    u64           tick     = u64(0);
};

struct FieldRemoved {
    RuntimeNodeId field = {};
    f32           sim_time = f32(0);
    u64           tick     = u64(0);
};

// ── Geometry ──────────────────────────────────────────────────────────────────

struct GeodesicBifurcation {
    u64 pursuer_id    = u64(0);
    u64 target_id     = u64(0);
    f32 path_length_a = f32(0);
    f32 path_length_b = f32(0);
    f32 sim_time      = f32(0);
    u64 tick          = u64(0);
};

} // namespace ndde::simulation::events
