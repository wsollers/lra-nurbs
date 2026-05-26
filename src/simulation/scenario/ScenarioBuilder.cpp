// simulation/scenario/ScenarioBuilder.cpp
#include "simulation/scenario/ScenarioBuilder.hpp"
#include "app/ParticleBehaviors.hpp"
#include "telemetry/TelemetryRecord.hpp"
#include "numeric/ops.hpp"
#include <algorithm>
#include <cstring>
#include <format>
#include <iostream>
#include <memory_resource>
#include <type_traits>

namespace ndde::simulation {

std::vector<glm::vec2> ScenarioBuilder::compute_spawn_positions(
        const AgentSpec& spec, u32 count, std::mt19937& rng) const {
    std::vector<glm::vec2> positions;
    positions.reserve(count);
    const auto& cfg = spec.spawn;
    if (!m_surface || count == u32(0)) return positions;

    const f32 u_min = m_surface->u_min();
    const f32 u_max = m_surface->u_max();
    const f32 v_min = m_surface->v_min();
    const f32 v_max = m_surface->v_max();
    const f32 two_pi = ops::two_pi_v<f32>;

    auto clamp_uv = [&](glm::vec2 p) -> glm::vec2 {
        return { std::clamp(p.x, u_min, u_max), std::clamp(p.y, v_min, v_max) };
    };

    switch (cfg.mode) {
    case SpawnMode::RingPack: {
        for (u32 i = u32(0); i < count; ++i) {
            const f32 a = cfg.phase_offset
                + two_pi * static_cast<f32>(i) / static_cast<f32>(count);
            positions.push_back(clamp_uv(cfg.center +
                glm::vec2{cfg.radius * ops::cos(a), cfg.radius * ops::sin(a)}));
        }
        break;
    }
    case SpawnMode::PoissonDisc: {
        const f32 min_sep = cfg.min_separation > f32(0)
            ? cfg.min_separation
            : cfg.radius / (f32(2) * ops::sqrt(static_cast<f32>(count)));
        std::uniform_real_distribution<f32> ad(f32(0), two_pi);
        std::uniform_real_distribution<f32> rd(f32(0), f32(1));
        const u32 max_att = count * u32(30);
        u32 att = u32(0);
        while (positions.size() < count && att < max_att) {
            ++att;
            const f32 a = ad(rng);
            const f32 r = cfg.radius * ops::sqrt(rd(rng));
            const glm::vec2 cand = clamp_uv(cfg.center +
                glm::vec2{r * ops::cos(a), r * ops::sin(a)});
            bool ok = true;
            for (const auto& q : positions) {
                const glm::vec2 d = cand - q;
                if (ops::sqrt(d.x*d.x + d.y*d.y) < min_sep) { ok = false; break; }
            }
            if (ok) positions.push_back(cand);
        }
        while (positions.size() < count) {
            const f32 a = ad(rng);
            const f32 r = cfg.radius * ops::sqrt(rd(rng));
            positions.push_back(clamp_uv(cfg.center +
                glm::vec2{r * ops::cos(a), r * ops::sin(a)}));
        }
        break;
    }
    case SpawnMode::Grid: {
        const u32 cols = cfg.grid_cols > u32(0)
            ? cfg.grid_cols
            : static_cast<u32>(ops::sqrt(static_cast<f32>(count)) + f32(0.5));
        const u32 rows = (count + cols - u32(1)) / cols;
        const f32 su = cfg.radius * f32(2) / static_cast<f32>(std::max(cols-u32(1), u32(1)));
        const f32 sv = cfg.radius * f32(2) / static_cast<f32>(std::max(rows-u32(1), u32(1)));
        for (u32 i = u32(0); i < count; ++i) {
            positions.push_back(clamp_uv({
                cfg.center.x - cfg.radius + static_cast<f32>(i % cols) * su,
                cfg.center.y - cfg.radius + static_cast<f32>(i / cols) * sv
            }));
        }
        break;
    }
    case SpawnMode::RandomDisc:
    default: {
        std::uniform_real_distribution<f32> jitter(f32(0.82), f32(1.16));
        for (u32 i = u32(0); i < count; ++i) {
            const f32 a = two_pi * static_cast<f32>(i) / static_cast<f32>(count)
                        + f32(0.37) * static_cast<f32>(i % u32(5));
            const f32 r = cfg.radius * jitter(rng);
            positions.push_back(clamp_uv(cfg.center +
                glm::vec2{r * ops::cos(a), r * ops::sin(a)}));
        }
        break;
    }
    }
    return positions;
}

// ── Build ─────────────────────────────────────────────────────────────────────
// IMPORTANT: parameter types use ndde::events:: fully qualified because
// inside ndde::simulation, "events" resolves to ndde::simulation::events.

void ScenarioBuilder::build(
        ParticleSystem&                                        particles,
        FieldCompositor&                                       compositor,
        std::vector<ndde::events::AlertRulePtr>&               alerts_out,
        EventBusService&                                       events,
        EventChannelId                                         channel,
        memory::MemoryService*                                 memory,
        f32                                                    sim_time,
        u64                                                    tick)
{
    events.publish(channel, ndde::simulation::events::ScenarioStarted{
        .scenario = runtime_node_id_from_text(m_name),
        .sim_time = sim_time,
        .tick = tick
    });

    for (auto& field : m_fields) {
        compositor.add(field);
        events.publish(channel, ndde::simulation::events::FieldAdded{
            .field = runtime_node_id_from_text(field->name()),
            .sim_time   = sim_time,
            .tick       = tick
        });
    }

    std::mt19937 rng{particles.rng()};

    for (const AgentSpec& spec : m_agents) {
        const auto positions = compute_spawn_positions(spec, u32(1), rng);
        if (positions.empty()) continue;
        const glm::vec2 uv = positions[0];

        auto builder = particles.factory().particle()
            .named(spec.label)
            .role(spec.role)
            .at(uv)
            .trail(TrailConfig{spec.trail_mode, spec.trail_max_points,
                               spec.trail_min_spacing});

        if (spec.use_history)
            builder.history(static_cast<std::size_t>(spec.history_capacity),
                             spec.history_dt_min);
        if (spec.noise.sigma > f32(0))
            builder.stochastic();

        if (spec.has_pursuit) {
            if (spec.pursuit.avoid) {
                AvoidParticleBehavior::Params ap;
                ap.target        = TargetSelector::nearest(spec.pursuit.target_role);
                ap.speed         = spec.pursuit.speed;
                ap.delay_seconds = spec.pursuit.delay_seconds;
                builder.with_behavior<AvoidParticleBehavior>(ap);
            } else {
                SeekParticleBehavior::Params sp;
                sp.target        = TargetSelector::nearest(spec.pursuit.target_role);
                sp.speed         = spec.pursuit.speed;
                sp.delay_seconds = spec.pursuit.delay_seconds;
                builder.with_behavior<SeekParticleBehavior>(sp);
            }
        }
        if (spec.noise.sigma > f32(0)) {
            builder.with_behavior<BrownianBehavior>(BrownianBehavior::Params{
                .sigma = spec.noise.sigma, .drift_strength = spec.noise.drift_strength
            });
        }

        Particle& particle = particles.spawn(std::move(builder));
        const u64 packed   = telemetry::pack_particle_id(
            particle.id(), static_cast<ParticleRole>(particle.particle_role()));

        events.publish(channel, ndde::simulation::events::AgentSpawned{
            .packed_id = packed,
            .recipe = runtime_node_id_from_text(spec.label),
            .u = uv.x,
            .v = uv.y,
            .sim_time  = sim_time,
            .tick = tick
        });
    }

    for (auto& alert : m_alerts) {
        std::visit([&alerts_out, memory](auto& params) {
            using Params = std::decay_t<decltype(params)>;
            if constexpr (std::is_same_v<Params, ndde::events::ProximityAlert::Params>) {
                alerts_out.push_back(memory
                    ? memory->simulation().template make_unique_as<
                        ndde::events::AlertRule, ndde::events::ProximityAlert>(params)
                    : memory::unique_cast<ndde::events::AlertRule>(
                        memory::make_unique<ndde::events::ProximityAlert>(
                            std::pmr::get_default_resource(), params)));
            } else if constexpr (std::is_same_v<Params, ndde::events::EscapeAlert::Params>) {
                alerts_out.push_back(memory
                    ? memory->simulation().template make_unique_as<
                        ndde::events::AlertRule, ndde::events::EscapeAlert>(params)
                    : memory::unique_cast<ndde::events::AlertRule>(
                        memory::make_unique<ndde::events::EscapeAlert>(
                            std::pmr::get_default_resource(), params)));
            } else if constexpr (std::is_same_v<Params, ndde::events::StealthAlert::Params>) {
                alerts_out.push_back(memory
                    ? memory->simulation().template make_unique_as<
                        ndde::events::AlertRule, ndde::events::StealthAlert>(params)
                    : memory::unique_cast<ndde::events::AlertRule>(
                        memory::make_unique<ndde::events::StealthAlert>(
                            std::pmr::get_default_resource(), params)));
            } else if constexpr (std::is_same_v<Params, ndde::events::CapturePendingAlert::Params>) {
                alerts_out.push_back(memory
                    ? memory->simulation().template make_unique_as<
                        ndde::events::AlertRule, ndde::events::CapturePendingAlert>(params)
                    : memory::unique_cast<ndde::events::AlertRule>(
                        memory::make_unique<ndde::events::CapturePendingAlert>(
                            std::pmr::get_default_resource(), params)));
            } else if constexpr (std::is_same_v<Params, ndde::events::CustomAlert::Params>) {
                alerts_out.push_back(memory
                    ? memory->simulation().template make_unique_as<
                        ndde::events::AlertRule, ndde::events::CustomAlert>(std::move(params))
                    : memory::unique_cast<ndde::events::AlertRule>(
                        memory::make_unique<ndde::events::CustomAlert>(
                            std::pmr::get_default_resource(), std::move(params))));
            }
        }, alert);
    }
    m_alerts.clear();

    std::cout << std::format("[ScenarioBuilder] Built '{}': {} agents, "
        "{} fields, {} alerts\n",
        m_name, particles.size(), compositor.size(), alerts_out.size());
}

} // namespace ndde::simulation
