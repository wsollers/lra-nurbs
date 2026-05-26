#pragma once
// simulation/scenario/ScenarioBuilder.hpp
// Fluent builder for simulation scenarios.
//
// CRITICAL NAMESPACE NOTE:
// This class lives in ndde::simulation. The sub-namespace ndde::simulation::events
// exists (from SimEventTypes.hpp). Inside ndde::simulation, the unqualified name
// "events" resolves to ndde::simulation::events, NOT ndde::events.
// Therefore all alert types (AlertRule, ProximityAlert, etc.) and EventBus,
// which live in ndde::events, MUST use the full qualification ndde::events::.

#include "simulation/scenario/AgentSpec.hpp"
#include "engine/events/EventBusService.hpp"
#include "simulation/fields/IField.hpp"
#include "simulation/events/AlertRule.hpp"
#include "simulation/events/SimEventTypes.hpp"
#include "simulation/events/EventRecord.hpp"
#include "app/ParticleSystem.hpp"
#include "app/ParticleTypes.hpp"
#include "math/Surfaces.hpp"
#include "math/Scalars.hpp"
#include "memory/MemoryService.hpp"
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <random>
#include <variant>

namespace ndde::simulation {

class ScenarioBuilder {
public:
    ScenarioBuilder& named(std::string n) { m_name = std::move(n); return *this; }
    ScenarioBuilder& on_surface(const math::ISurface* s) { m_surface = s; return *this; }

    ScenarioBuilder& add_agent(AgentSpec spec) {
        m_agents.push_back(std::move(spec)); return *this;
    }
    ScenarioBuilder& add_agents(u32 count, AgentSpec proto) {
        proto.spawn.mode = SpawnMode::RingPack;
        for (u32 i = u32(0); i < count; ++i) {
            proto.colour_slot = i % u32(4);
            m_agents.push_back(proto);
        }
        return *this;
    }

    ScenarioBuilder& with_field(std::shared_ptr<IField> field) {
        m_fields.push_back(std::move(field)); return *this;
    }

    // ── Alert registration ────────────────────────────────────────────────────
    // IMPORTANT: use ndde::events:: fully qualified — "events::" alone resolves
    // to ndde::simulation::events inside this namespace.

    ScenarioBuilder& alert_proximity(
            f32 threshold, f32 hysteresis = f32(0.15),
            ParticleRole pursuer = ParticleRole::Chaser,
            ParticleRole prey    = ParticleRole::Leader) {
        m_alerts.push_back(ndde::events::ProximityAlert::Params{pursuer, prey, threshold, hysteresis});
        return *this;
    }

    ScenarioBuilder& alert_escape(
            f32 distance, f32 hysteresis = f32(0.3),
            ParticleRole pursuer = ParticleRole::Chaser,
            ParticleRole prey    = ParticleRole::Leader) {
        m_alerts.push_back(ndde::events::EscapeAlert::Params{pursuer, prey, distance, hysteresis});
        return *this;
    }

    ScenarioBuilder& alert_stealth(
            f32 kappa_max, f32 hysteresis = f32(0.1),
            ParticleRole role = ParticleRole::Chaser) {
        m_alerts.push_back(ndde::events::StealthAlert::Params{role, kappa_max, hysteresis});
        return *this;
    }

    ScenarioBuilder& alert_capture_pending(
            f32 seconds_ahead = f32(2.0), f32 hysteresis = f32(0.5)) {
        m_alerts.push_back(ndde::events::CapturePendingAlert::Params{
            ParticleRole::Chaser, ParticleRole::Leader,
            seconds_ahead, hysteresis});
        return *this;
    }

    ScenarioBuilder& alert_custom(
            std::string_view alert_name,
            ndde::events::CustomAlert::EvalFn fn,
            ndde::events::EventSeverity sev = ndde::events::EventSeverity::Warning,
            f32 hysteresis = f32(0)) {
        ndde::events::CustomAlert::Params cp;
        const auto len = alert_name.size() < sizeof(cp.rule_name) - 1u
                       ? alert_name.size() : sizeof(cp.rule_name) - 1u;
        std::memcpy(cp.rule_name, alert_name.data(), len);
        cp.rule_name[len] = '\0';
        cp.severity   = sev;
        cp.hysteresis = hysteresis;
        cp.fn         = std::move(fn);
        m_alerts.push_back(std::move(cp));
        return *this;
    }

    // ── Build ─────────────────────────────────────────────────────────────────
    // ndde::events:: fully qualified in signature for the same namespace reason.
    void build(ParticleSystem&                                        particles,
               FieldCompositor&                                       compositor,
               std::vector<ndde::events::AlertRulePtr>&               alerts_out,
               EventBusService&                                       events,
               EventChannelId                                         channel,
               memory::MemoryService*                                 memory,
               f32                                                    sim_time,
               u64                                                    tick);

    [[nodiscard]] const std::string& scenario_name() const noexcept { return m_name; }

private:
    std::string                                           m_name    = "Unnamed";
    const math::ISurface*                                 m_surface = nullptr;
    std::vector<AgentSpec>                                m_agents;
    std::vector<std::shared_ptr<IField>>                  m_fields;
    using AlertSpec = std::variant<
        ndde::events::ProximityAlert::Params,
        ndde::events::EscapeAlert::Params,
        ndde::events::StealthAlert::Params,
        ndde::events::CapturePendingAlert::Params,
        ndde::events::CustomAlert::Params>;
    std::vector<AlertSpec>                                m_alerts;

    std::vector<glm::vec2> compute_spawn_positions(
            const AgentSpec& spec, u32 count, std::mt19937& rng) const;
};

} // namespace ndde::simulation
