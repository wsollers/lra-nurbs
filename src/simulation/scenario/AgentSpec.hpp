#pragma once
// simulation/scenario/AgentSpec.hpp
// Declarative description of one agent in a scenario.
// ScenarioBuilder consumes these to construct ParticleBuilder chains.

#include "app/ParticleTypes.hpp"
#include "app/ParticleBehaviors.hpp"
#include "simulation/spawn/SpawnMode.hpp"
#include "math/Scalars.hpp"
#include <glm/glm.hpp>
#include <string>

namespace ndde::simulation {

struct NoiseSpec {
    f32 sigma          = f32(0);
    f32 drift_strength = f32(0);
};

struct PursuitSpec {
    f32          speed         = f32(0.8);
    f32          delay_seconds = f32(0.5);
    ParticleRole target_role   = ParticleRole::Leader;
    bool         avoid         = false;    // true = AvoidBehavior instead of Seek
};

struct StealthSpec {
    bool enabled   = false;
    f32  kappa_max = f32(2.0);
};

// Declare what equation drives this agent.
// Behaviors are composable — an agent can have both pursuit and brownian.
struct AgentSpec {
    // Identity
    std::string  label;
    ParticleRole role         = ParticleRole::Neutral;
    u32          colour_slot  = u32(0);

    // Spawn placement
    SpawnConfig  spawn;          // where to place this agent

    // Brownian stochastic component (added on top of any deterministic behavior)
    NoiseSpec    noise;          // sigma > 0 enables Milstein integrator

    // Deterministic behavior (optional — agents can be pure Brownian)
    bool         has_pursuit    = false;
    PursuitSpec  pursuit;

    // Constraints
    StealthSpec  stealth;
    bool         domain_confinement = true;

    // History (required for prey that chasers pursue with delay)
    bool         use_history        = false;
    u64          history_capacity   = u64(4096);
    f32          history_dt_min     = f32(1.0f / 120.f);

    // Trail
    TrailMode    trail_mode         = TrailMode::Finite;
    u32          trail_max_points   = u32(1200);
    f32          trail_min_spacing  = f32(0.012);

    // Telemetry
    bool         record_telemetry   = true;
};

} // namespace ndde::simulation
