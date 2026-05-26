#include "app/ParticleFactory.hpp"
#include "app/SimulationContext.hpp"
#include "app/ParticleBehaviors.hpp"
#include "app/AnimatedCurve.hpp"

#include <limits>

namespace ndde {

namespace {

[[nodiscard]] glm::vec2 shortest_delta(glm::vec2 target,
                                       glm::vec2 from,
                                       const ndde::math::ISurface& surface) noexcept {
    glm::vec2 delta = target - from;
    if (surface.is_periodic_u()) {
        const f32 span = surface.u_max() - surface.u_min();
        if (delta.x >  span * 0.5f) delta.x -= span;
        if (delta.x < -span * 0.5f) delta.x += span;
    }
    if (surface.is_periodic_v()) {
        const f32 span = surface.v_max() - surface.v_min();
        if (delta.y >  span * 0.5f) delta.y -= span;
        if (delta.y < -span * 0.5f) delta.y += span;
    }
    return delta;
}

[[nodiscard]] glm::vec2 normalize_or_zero(glm::vec2 v) noexcept {
    const f32 d = ops::length(v);
    return d > 1e-7f ? v / d : glm::vec2{0.f, 0.f};
}

[[nodiscard]] glm::vec2 target_position(const TargetSelector& selector,
                                        glm::vec2 from,
                                        const ndde::math::ISurface& surface,
                                        f32 t,
                                        const SimulationContext& context,
                                        ParticleId owner,
                                        f32 delay) noexcept {
    const AnimatedCurve* target = nullptr;
    switch (selector.kind) {
        case TargetSelector::Kind::ById:
            target = context.find(selector.id);
            break;
        case TargetSelector::Kind::FirstRole:
            target = context.first(selector.role, owner);
            break;
        case TargetSelector::Kind::NearestRole:
            target = context.nearest(selector.role, from, owner);
            break;
        case TargetSelector::Kind::CentroidRole:
            return context.centroid(selector.role, owner);
    }

    if (!target) return from;
    if (delay > 0.f) {
        if (const auto* history = target->history())
            return history->query(t - delay);
    }
    (void)surface;
    return target->head_uv();
}

} // namespace

const AnimatedCurve* SimulationContext::find(ParticleId id) const noexcept {
    if (!m_particles) return nullptr;
    for (const auto& particle : *m_particles) {
        if (particle.id() == id) return &particle;
    }
    return nullptr;
}

const AnimatedCurve* SimulationContext::first(ParticleRole role, ParticleId exclude) const noexcept {
    if (!m_particles) return nullptr;
    for (const auto& particle : *m_particles) {
        if (particle.id() != exclude && particle.particle_role() == role)
            return &particle;
    }
    return nullptr;
}

const AnimatedCurve* SimulationContext::nearest(ParticleRole role, glm::vec2 from, ParticleId exclude) const noexcept {
    if (!m_particles || !m_surface) return nullptr;
    const AnimatedCurve* best = nullptr;
    f32 best_d2 = std::numeric_limits<f32>::max();
    for (const auto& particle : *m_particles) {
        if (particle.id() == exclude || particle.particle_role() != role) continue;
        const glm::vec2 d = shortest_delta(particle.head_uv(), from, *m_surface);
        const f32 d2 = d.x * d.x + d.y * d.y;
        if (d2 < best_d2) {
            best_d2 = d2;
            best = &particle;
        }
    }
    return best;
}

glm::vec2 SimulationContext::centroid(ParticleRole role, ParticleId exclude) const noexcept {
    if (!m_particles) return {0.f, 0.f};
    glm::vec2 sum{0.f, 0.f};
    u32 count = 0;
    for (const auto& particle : *m_particles) {
        if (particle.id() == exclude || particle.particle_role() != role) continue;
        sum += particle.head_uv();
        ++count;
    }
    return count > 0 ? sum / static_cast<f32>(count) : glm::vec2{0.f, 0.f};
}

glm::vec2 SeekParticleBehavior::direction_to_target(glm::vec2 from,
                                                    const ndde::math::ISurface& surface,
                                                    f32 t,
                                                    const SimulationContext& context,
                                                    ParticleId owner) const {
    const glm::vec2 target = target_position(m_p.target, from, surface, t, context, owner, m_p.delay_seconds);
    return normalize_or_zero(shortest_delta(target, from, surface));
}

glm::vec2 AvoidParticleBehavior::velocity(ndde::sim::ParticleState& state,
                                          const ndde::math::ISurface& surface,
                                          f32 t,
                                          const SimulationContext& context,
                                          ParticleId owner) const {
    const glm::vec2 target = target_position(m_p.target, state.uv, surface, t, context, owner, m_p.delay_seconds);
    return -normalize_or_zero(shortest_delta(target, state.uv, surface)) * m_p.speed;
}

glm::vec2 CentroidSeekBehavior::velocity(ndde::sim::ParticleState& state,
                                         const ndde::math::ISurface& surface,
                                         f32,
                                         const SimulationContext& context,
                                         ParticleId owner) const {
    const glm::vec2 target = context.centroid(m_p.role, owner);
    return normalize_or_zero(shortest_delta(target, state.uv, surface)) * m_p.speed;
}

glm::vec2 GradientDriftBehavior::velocity(ndde::sim::ParticleState& state,
                                          const ndde::math::ISurface& surface,
                                          f32,
                                          const SimulationContext&,
                                          ParticleId) const {
    const glm::vec3 du = surface.du(state.uv.x, state.uv.y);
    const glm::vec3 dv = surface.dv(state.uv.x, state.uv.y);
    glm::vec2 grad{du.z, dv.z};
    if (ops::length(grad) < 1e-7f)
        return glm::vec2{ops::cos(state.angle), ops::sin(state.angle)} * m_p.speed;

    switch (m_p.mode) {
        case Mode::Downhill:
            grad = -grad;
            break;
        case Mode::LevelTangent:
            grad = {-grad.y, grad.x};
            break;
        case Mode::Uphill:
        default:
            break;
    }
    return normalize_or_zero(grad) * m_p.speed;
}

std::string GradientDriftBehavior::metadata_label() const {
    switch (m_p.mode) {
        case Mode::Downhill: return "Gradient Downhill";
        case Mode::LevelTangent: return "Gradient Level Tangent";
        case Mode::Uphill:
        default: return "Gradient Uphill";
    }
}

glm::vec2 OrbitBehavior::velocity(ndde::sim::ParticleState& state,
                                  const ndde::math::ISurface& surface,
                                  f32,
                                  const SimulationContext&,
                                  ParticleId) const {
    const glm::vec2 radial = shortest_delta(state.uv, m_p.center, surface);
    const f32 r = ops::length(radial);
    const glm::vec2 outward = r > 1e-7f ? radial / r : glm::vec2{1.f, 0.f};
    glm::vec2 tangent = m_p.clockwise
        ? glm::vec2{outward.y, -outward.x}
        : glm::vec2{-outward.y, outward.x};
    const glm::vec2 radial_correction = -outward * ((r - m_p.radius) * m_p.radial_strength);
    return tangent * (m_p.angular_speed * std::max(m_p.radius, 0.01f)) + radial_correction;
}

glm::vec2 FlockingBehavior::velocity(ndde::sim::ParticleState& state,
                                     const ndde::math::ISurface& surface,
                                     f32,
                                     const SimulationContext& context,
                                     ParticleId owner) const {
    glm::vec2 separation{0.f, 0.f};
    glm::vec2 cohesion_sum{0.f, 0.f};
    glm::vec2 alignment_sum{0.f, 0.f};
    u32 neighbor_count = 0;
    u32 alignment_count = 0;

    for (const Particle& other : context.particles()) {
        if (other.id() == owner || other.particle_role() != m_p.role) continue;
        const glm::vec2 delta = shortest_delta(other.head_uv(), state.uv, surface);
        const f32 d = ops::length(delta);
        if (d > m_p.neighbor_radius || d < 1e-7f) continue;

        cohesion_sum += delta;
        ++neighbor_count;

        if (d < m_p.separation_radius)
            separation -= delta / (d * d + 1e-4f);

        if (other.trail_size() >= 2) {
            const Vec3 a = other.trail_pt(other.trail_size() - 2u);
            const Vec3 b = other.trail_pt(other.trail_size() - 1u);
            const glm::vec2 heading{b.x - a.x, b.y - a.y};
            if (ops::length(heading) > 1e-7f) {
                alignment_sum += normalize_or_zero(heading);
                ++alignment_count;
            }
        }
    }

    glm::vec2 velocity{0.f, 0.f};
    if (neighbor_count > 0)
        velocity += m_p.cohesion_weight * normalize_or_zero(cohesion_sum / static_cast<f32>(neighbor_count));
    velocity += m_p.separation_weight * normalize_or_zero(separation);
    if (alignment_count > 0)
        velocity += m_p.alignment_weight * normalize_or_zero(alignment_sum / static_cast<f32>(alignment_count));

    return normalize_or_zero(velocity) * m_p.speed;
}

std::string BehaviorStack::name() const {
    if (m_behaviors.empty()) return "BehaviorStack";
    std::string out;
    for (const auto& entry : m_behaviors) {
        if (!out.empty()) out += " + ";
        out += entry.behavior->metadata_label();
    }
    return out;
}

memory::FrameVector<std::string> BehaviorStack::behavior_labels() const {
    memory::FrameVector<std::string> labels;
    labels.reserve(m_behaviors.size());
    for (const auto& entry : m_behaviors)
        labels.push_back(entry.behavior->metadata_label());
    if (m_velocity_transform.enabled)
        labels.push_back("Slope Velocity Transform");
    return labels;
}

AnimatedCurve ParticleBuilder::build(const ndde::sim::IIntegrator* deterministic,
                                     const ndde::sim::IIntegrator* stochastic) {
    const ndde::sim::IIntegrator* integrator = (m_stochastic && stochastic) ? stochastic : deterministic;
    auto stack = make_sim_unique_as<ndde::sim::IEquation, BehaviorStack>(std::move(m_stack));

    AnimatedCurve particle = AnimatedCurve::with_equation(
        m_uv.x, m_uv.y,
        m_role == ParticleRole::Chaser ? AnimatedCurve::Role::Chaser : AnimatedCurve::Role::Leader,
        0u,
        m_surface,
        std::move(stack),
        integrator,
        m_memory);

    particle.bind_memory(m_memory);
    particle.set_particle_role(m_role);
    particle.set_label(m_label);
    particle.set_trail_config(m_trail);
    if (m_enable_history)
        particle.enable_history(m_history_capacity, m_history_dt_min);
    for (auto& constraint : m_constraints)
        particle.add_constraint(std::move(constraint));
    particle.bind_behavior_stack();
    return particle;
}

} // namespace ndde
