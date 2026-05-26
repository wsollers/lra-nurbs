// simulation/events/AlertRule.cpp
#include "simulation/events/AlertRule.hpp"
#include "math/Surfaces.hpp"
#include "numeric/ops.hpp"
#include <glm/glm.hpp>
#include <limits>

namespace ndde::events {

// ── helpers ───────────────────────────────────────────────────────────────────

static glm::vec3 world_pos(const AlertParticleView& p, const math::ISurface& surface) {
    const auto w  = surface.evaluate(p.uv.x, p.uv.y);
    return {w.x, w.y, w.z};
}

static f32 world_dist(const AlertParticleView& a, const AlertParticleView& b,
                       const math::ISurface& surface) {
    const glm::vec3 pa = world_pos(a, surface);
    const glm::vec3 pb = world_pos(b, surface);
    const glm::vec3 d  = pa - pb;
    return ops::sqrt(d.x*d.x + d.y*d.y + d.z*d.z);
}

// ── ProximityAlert ────────────────────────────────────────────────────────────

void ProximityAlert::evaluate(const AlertContext& ctx, EventRing& ring) {
    if (!ctx.surface || !ctx.particles || ctx.particle_count == u64(0)) return;

    f32 min_dist     = std::numeric_limits<f32>::max();
    u64 best_pursuer = u64(0);
    u64 best_prey    = u64(0);

    for (u64 i = u64(0); i < ctx.particle_count; ++i) {
        const AlertParticleView& pursuer = ctx.particles[i];
        if (pursuer.role != m_p.pursuer_role) continue;

        for (u64 j = u64(0); j < ctx.particle_count; ++j) {
            const AlertParticleView& prey = ctx.particles[j];
            if (prey.role != m_p.prey_role) continue;
            const f32 d = world_dist(pursuer, prey, *ctx.surface);
            if (d < min_dist) {
                min_dist     = d;
                best_pursuer = pursuer.id;
                best_prey    = prey.id;
            }
        }
    }

    if (min_dist == std::numeric_limits<f32>::max()) return;

    const bool inside     = min_dist < m_p.threshold;
    const bool reset_edge = !inside && min_dist > m_p.threshold + m_p.hysteresis;

    if (inside && !m_was_inside) {
        (void)ring.push(make_alert_proximity(
            best_pursuer, best_prey, min_dist, m_p.threshold,
            ctx.sim_time, ctx.tick));
    }
    if (reset_edge) m_was_inside = false;
    if (inside)     m_was_inside = true;
}

// ── EscapeAlert ───────────────────────────────────────────────────────────────

void EscapeAlert::evaluate(const AlertContext& ctx, EventRing& ring) {
    if (!ctx.surface || !ctx.particles || ctx.particle_count == u64(0)) return;

    glm::vec3 centroid{f32(0)};
    u32 pursuer_count = u32(0);
    for (u64 i = u64(0); i < ctx.particle_count; ++i) {
        const AlertParticleView& p = ctx.particles[i];
        if (p.role != m_p.pursuer_role) continue;
        centroid += world_pos(p, *ctx.surface);
        ++pursuer_count;
    }
    if (pursuer_count == u32(0)) return;
    centroid /= static_cast<f32>(pursuer_count);

    f32 max_dist = f32(0);
    for (u64 i = u64(0); i < ctx.particle_count; ++i) {
        const AlertParticleView& p = ctx.particles[i];
        if (p.role != m_p.prey_role) continue;
        const glm::vec3 pw = world_pos(p, *ctx.surface);
        const glm::vec3 dv = pw - centroid;
        const f32 d = ops::sqrt(dv.x*dv.x + dv.y*dv.y + dv.z*dv.z);
        if (d > max_dist) max_dist = d;
    }

    const bool outside    = max_dist > m_p.escape_distance;
    const bool reset_edge = !outside
        && max_dist < m_p.escape_distance - m_p.hysteresis;

    if (outside && !m_was_outside) {
        (void)ring.push(make_alert_escape(
            max_dist, m_p.escape_distance, ctx.sim_time, ctx.tick));
    }
    if (reset_edge) m_was_outside = false;
    if (outside)    m_was_outside = true;
}

// ── StealthAlert ──────────────────────────────────────────────────────────────

StealthAlert::AgentEdge* StealthAlert::find_or_add(u64 id) noexcept {
    for (u32 i = u32(0); i < m_agent_count; ++i)
        if (m_state_map[i].id == id) return &m_state_map[i];
    if (m_agent_count >= MAX_AGENTS) return nullptr;
    m_state_map[m_agent_count] = AgentEdge{id, true};
    return &m_state_map[m_agent_count++];
}

void StealthAlert::evaluate(const AlertContext& ctx, EventRing& ring) {
    if (!ctx.surface || !ctx.particles || ctx.particle_count == u64(0)) return;

    for (u64 i = u64(0); i < ctx.particle_count; ++i) {
        const AlertParticleView& p = ctx.particles[i];
        if (p.role != m_p.role) continue;

        const u32 trail_n = p.trail_size;
        if (trail_n < 4u) continue;

        const f32 kappa_g = p.geodesic_curvature;

        AgentEdge* edge = find_or_add(p.id);
        if (!edge) continue;

        const bool violation = kappa_g > m_p.kappa_max;
        const bool settled   = !violation
            && kappa_g < m_p.kappa_max - m_p.hysteresis;

        if (violation && edge->stealth_ok) {
            (void)ring.push(make_alert_stealth(
                p.id, kappa_g, m_p.kappa_max, /*lost=*/true,
                ctx.sim_time, ctx.tick));
            edge->stealth_ok = false;
        } else if (settled && !edge->stealth_ok) {
            (void)ring.push(make_alert_stealth(
                p.id, kappa_g, m_p.kappa_max, /*lost=*/false,
                ctx.sim_time, ctx.tick));
            edge->stealth_ok = true;
        }
    }
}

// ── CapturePendingAlert ───────────────────────────────────────────────────────

void CapturePendingAlert::evaluate(const AlertContext& ctx, EventRing& ring) {
    if (!ctx.surface || !ctx.particles || ctx.particle_count == u64(0)) return;

    f32 min_dist     = std::numeric_limits<f32>::max();
    u64 best_pursuer = u64(0);
    u64 best_prey    = u64(0);

    for (u64 i = u64(0); i < ctx.particle_count; ++i) {
        const AlertParticleView& pursuer = ctx.particles[i];
        if (pursuer.role != m_p.pursuer_role) continue;
        for (u64 j = u64(0); j < ctx.particle_count; ++j) {
            const AlertParticleView& prey = ctx.particles[j];
            if (prey.role != m_p.prey_role) continue;
            const f32 d = world_dist(pursuer, prey, *ctx.surface);
            if (d < min_dist) {
                min_dist     = d;
                best_pursuer = pursuer.id;
                best_prey    = prey.id;
            }
        }
    }

    if (min_dist == std::numeric_limits<f32>::max()) { m_prev_dist = f32(-1); return; }

    constexpr f32 assumed_dt = f32(1.0 / 60.0);
    f32 ttc = std::numeric_limits<f32>::max();
    if (m_prev_dist >= f32(0)) {
        const f32 closing = m_prev_dist - min_dist;
        if (closing > f32(1e-6)) {
            const f32 rate = closing / assumed_dt;
            ttc = min_dist / rate;
        }
    }
    m_prev_dist = min_dist;

    const bool pending    = ttc < m_p.seconds_ahead;
    const bool reset_edge = !pending && ttc > m_p.seconds_ahead + m_p.hysteresis;

    if (pending && !m_was_pending) {
        (void)ring.push(make_alert_capture_pending(
            best_pursuer, best_prey, ttc, ctx.sim_time, ctx.tick));
    }
    if (reset_edge) m_was_pending = false;
    if (pending)    m_was_pending = true;
}

// ── CustomAlert ───────────────────────────────────────────────────────────────

void CustomAlert::evaluate(const AlertContext& ctx, EventRing& ring) {
    if (!m_p.fn) return;
    f32 val_a = f32(0);
    f32 val_b = f32(0);
    const bool active = m_p.fn(ctx, val_a, val_b);

    if (active && !m_was_active) {
        (void)ring.push(make_alert_custom(
            m_p.rule_name, val_a, val_b,
            m_p.severity, ctx.sim_time, ctx.tick));
        m_was_active = true;
    } else if (!active) {
        m_was_active = false;
    }
}

} // namespace ndde::events
