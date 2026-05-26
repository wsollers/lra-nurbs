#pragma once
// simulation/events/AlertRule.hpp
// Declarative alert conditions evaluated per-tick in SimContext::end_tick().
// Each rule fires ONCE on the rising edge of a condition (edge detection).
// Hysteresis prevents boundary chatter.
// All evaluate() implementations are wait-free and allocation-free.
//
#include "simulation/events/EventRecord.hpp"
#include "simulation/events/EventRing.hpp"
#include "math/GeometryTypes.hpp"
#include "math/Scalars.hpp"
#include "app/ParticleTypes.hpp"
#include "memory/Unique.hpp"
#include <array>
#include <functional>
#include <string_view>

// Forward-declare ISurface so alert declarations do not pull in surface math bodies.
namespace ndde::math { class ISurface; }

namespace ndde::events {

class AlertRule;
using AlertRulePtr = ndde::memory::Unique<AlertRule>;

struct AlertParticleView {
    ParticleId    id = ParticleId(0);
    ParticleRole  role = ParticleRole::Neutral;
    Vec2          uv{f32(0), f32(0)};
    u32           trail_size = u32(0);
    f32           geodesic_curvature = f32(0);
};

// ── AlertContext ──────────────────────────────────────────────────────────────
// Read-only view passed to every AlertRule::evaluate(). No allocation.
// Particle access via raw pointer + count avoids including LifetimeVector.
struct AlertContext {
    f32                    sim_time;
    u64                    tick;
    const math::ISurface*  surface;        // never null
    const AlertParticleView* particles;     // pointer to first element
    u64                    particle_count;
};

// ── AlertRule — base ──────────────────────────────────────────────────────────
class AlertRule {
public:
    virtual ~AlertRule() = default;

    // Evaluate condition. Push EventRecord to ring on rising edge.
    // Must be wait-free and non-allocating.
    virtual void evaluate(const AlertContext& ctx, EventRing& ring) = 0;

    [[nodiscard]] virtual std::string_view name()     const noexcept = 0;
    [[nodiscard]] virtual EventSeverity    severity() const noexcept = 0;

    // Reset edge state. Called on ScenarioReset so alerts re-arm cleanly.
    virtual void reset() noexcept {}

protected:
    AlertRule() = default;
    AlertRule(const AlertRule&) = default;
    AlertRule& operator=(const AlertRule&) = default;
    AlertRule(AlertRule&&) = default;
    AlertRule& operator=(AlertRule&&) = default;
};

// ── ProximityAlert ────────────────────────────────────────────────────────────
class ProximityAlert final : public AlertRule {
public:
    struct Params {
        ParticleRole pursuer_role = ParticleRole::Chaser;
        ParticleRole prey_role    = ParticleRole::Leader;
        f32          threshold    = f32(0.5);
        f32          hysteresis   = f32(0.15);
    };
    explicit ProximityAlert(Params p) : m_p(p) {}

    void evaluate(const AlertContext& ctx, EventRing& ring) override;
    [[nodiscard]] std::string_view name()     const noexcept override { return "ProximityAlert"; }
    [[nodiscard]] EventSeverity    severity() const noexcept override { return EventSeverity::Warning; }
    void reset() noexcept override { m_was_inside = false; }

private:
    Params m_p;
    bool   m_was_inside = false;
};

// ── EscapeAlert ───────────────────────────────────────────────────────────────
class EscapeAlert final : public AlertRule {
public:
    struct Params {
        ParticleRole pursuer_role    = ParticleRole::Chaser;
        ParticleRole prey_role       = ParticleRole::Leader;
        f32          escape_distance = f32(3.0);
        f32          hysteresis      = f32(0.3);
    };
    explicit EscapeAlert(Params p) : m_p(p) {}

    void evaluate(const AlertContext& ctx, EventRing& ring) override;
    [[nodiscard]] std::string_view name()     const noexcept override { return "EscapeAlert"; }
    [[nodiscard]] EventSeverity    severity() const noexcept override { return EventSeverity::Warning; }
    void reset() noexcept override { m_was_outside = false; }

private:
    Params m_p;
    bool   m_was_outside = false;
};

// ── StealthAlert ──────────────────────────────────────────────────────────────
class StealthAlert final : public AlertRule {
public:
    struct Params {
        ParticleRole role       = ParticleRole::Chaser;
        f32          kappa_max  = f32(2.0);
        f32          hysteresis = f32(0.1);
    };
    explicit StealthAlert(Params p) : m_p(p) {}

    void evaluate(const AlertContext& ctx, EventRing& ring) override;
    [[nodiscard]] std::string_view name()     const noexcept override { return "StealthAlert"; }
    [[nodiscard]] EventSeverity    severity() const noexcept override { return EventSeverity::Alert; }
    void reset() noexcept override {
        m_agent_count = u32(0);
        m_state_map.fill({});
    }

private:
    Params m_p;
    static constexpr u32 MAX_AGENTS = 64u;
    struct AgentEdge { u64 id = u64(0); bool stealth_ok = true; };
    std::array<AgentEdge, MAX_AGENTS> m_state_map{};
    u32 m_agent_count = u32(0);

    [[nodiscard]] AgentEdge* find_or_add(u64 id) noexcept;
};

// ── CapturePendingAlert ───────────────────────────────────────────────────────
class CapturePendingAlert final : public AlertRule {
public:
    struct Params {
        ParticleRole pursuer_role  = ParticleRole::Chaser;
        ParticleRole prey_role     = ParticleRole::Leader;
        f32          seconds_ahead = f32(2.0);
        f32          hysteresis    = f32(0.5);
    };
    explicit CapturePendingAlert(Params p) : m_p(p) {}

    void evaluate(const AlertContext& ctx, EventRing& ring) override;
    [[nodiscard]] std::string_view name()     const noexcept override { return "CapturePendingAlert"; }
    [[nodiscard]] EventSeverity    severity() const noexcept override { return EventSeverity::Alert; }
    void reset() noexcept override { m_was_pending = false; m_prev_dist = f32(-1); }

private:
    Params m_p;
    bool   m_was_pending = false;
    f32    m_prev_dist   = f32(-1);
};

// ── CustomAlert ───────────────────────────────────────────────────────────────
class CustomAlert final : public AlertRule {
public:
    using EvalFn = std::function<bool(const AlertContext&, f32& val_a, f32& val_b)>;

    struct Params {
        char          rule_name[32] = "CustomAlert";
        EventSeverity severity      = EventSeverity::Warning;
        f32           hysteresis    = f32(0);
        EvalFn        fn;
    };
    explicit CustomAlert(Params p) : m_p(std::move(p)) {}

    void evaluate(const AlertContext& ctx, EventRing& ring) override;
    [[nodiscard]] std::string_view name()     const noexcept override { return m_p.rule_name; }
    [[nodiscard]] EventSeverity    severity() const noexcept override { return m_p.severity; }
    void reset() noexcept override { m_was_active = false; }

private:
    Params m_p;
    bool   m_was_active = false;
};

} // namespace ndde::events
