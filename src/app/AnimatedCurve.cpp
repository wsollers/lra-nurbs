// app/AnimatedCurve.cpp
// AnimatedCurve implementation.
// Previously lived in app/GaussianSurface.cpp (now archived to src/old/).
// Extracted here so it compiles independently of GaussianSurface.

#include "app/AnimatedCurve.hpp"
#include "app/FrenetFrame.hpp"
#include "app/ParticleBehaviors.hpp"
#include "sim/IEquation.hpp"
#include "sim/IIntegrator.hpp"
#include "sim/HistoryBuffer.hpp"
#include "sim/DomainConfinement.hpp"
#include "numeric/ops.hpp"
#include <cmath>
#include <algorithm>
#include <glm/glm.hpp>

namespace ndde {

namespace {

memory::HistoryVector<TrailSample> make_particle_trail_vector(memory::MemoryService* memory) {
    return memory ? memory->history().make_vector<TrailSample>()
                  : memory::HistoryVector<TrailSample>{std::pmr::get_default_resource()};
}

memory::SimVector<memory::Unique<ndde::sim::IConstraint>>
make_particle_constraint_vector(memory::MemoryService* memory) {
    return memory ? memory->simulation().make_vector<memory::Unique<ndde::sim::IConstraint>>()
                  : memory::SimVector<memory::Unique<ndde::sim::IConstraint>>{
                        std::pmr::get_default_resource()
                    };
}

memory::Unique<ndde::sim::IConstraint>
make_domain_confinement_constraint(memory::MemoryService* memory) {
    if (memory)
        return memory->simulation().make_unique_as<ndde::sim::IConstraint, ndde::sim::DomainConfinement>();
    return memory::unique_cast<ndde::sim::IConstraint>(
        memory::make_unique<ndde::sim::DomainConfinement>(std::pmr::get_default_resource()));
}

} // namespace

AnimatedCurve::AnimatedCurve(f32 start_x, f32 start_y,
                             Role role, u32 colour_slot,
                             const ndde::math::ISurface*  surface,
                             ndde::sim::IEquation*         equation,
                             const ndde::sim::IIntegrator* integrator,
                             memory::MemoryService*        mem)
    : m_trail(make_particle_trail_vector(mem))
    , m_surface(surface)
    , m_equation(equation)
    , m_owned_equation(nullptr)
    , m_integrator(integrator)
    , m_constraints(make_particle_constraint_vector(mem))
    , m_memory(mem)
    , m_role(role)
    , m_colour_slot(colour_slot % MAX_SLOTS)
    , m_start_x(start_x)
    , m_start_y(start_y)
{
    m_walk = ndde::sim::ParticleState{ glm::vec2{start_x, start_y}, 0.f, 0.f };
    m_particle_role = role == Role::Leader ? ParticleRole::Leader : ParticleRole::Chaser;
    m_constraints.push_back(make_domain_confinement_constraint(mem));
}

void AnimatedCurve::bind_memory(memory::MemoryService* memory) {
    m_memory = memory;
    if (memory) {
        memory->history().rebind_vector(m_trail);
    } else {
        std::pmr::memory_resource* desired_trail = std::pmr::get_default_resource();
        if (m_trail.get_allocator().resource() != desired_trail) {
            memory::HistoryVector<TrailSample> rebound{desired_trail};
            rebound.reserve(m_trail.size());
            for (TrailSample& sample : m_trail)
                rebound.push_back(sample);
            std::destroy_at(&m_trail);
            std::construct_at(&m_trail, std::move(rebound));
        }
    }
    if (memory) {
        memory->simulation().rebind_vector(m_constraints);
    } else {
        std::pmr::memory_resource* desired_constraints = std::pmr::get_default_resource();
        if (m_constraints.get_allocator().resource() == desired_constraints) return;
        memory::SimVector<memory::Unique<ndde::sim::IConstraint>> rebound{desired_constraints};
        rebound.reserve(m_constraints.size());
        for (auto& constraint : m_constraints)
            rebound.push_back(std::move(constraint));
        std::destroy_at(&m_constraints);
        std::construct_at(&m_constraints, std::move(rebound));
    }
    if (m_history && memory)
        m_history.get_deleter().assert_alive();
}

// static
AnimatedCurve AnimatedCurve::with_equation(
    f32 start_x, f32 start_y,
    Role role, u32 colour_slot,
    const ndde::math::ISurface*          surface,
    memory::Unique<ndde::sim::IEquation> owned_equation,
    const ndde::sim::IIntegrator*        integrator,
    memory::MemoryService*               mem)
{
    AnimatedCurve c(start_x, start_y, role, colour_slot,
                    surface, owned_equation.get(), integrator, mem);
    c.m_owned_equation = std::move(owned_equation);
    return c;
}

void AnimatedCurve::reset() {
    m_trail.clear();
    m_walk.uv    = { m_start_x, m_start_y };
    m_walk.phase = 0.f;
    m_walk.angle = 0.f;
}

void AnimatedCurve::advance(f32 dt, f32 speed_scale) {
    const i32 sub_steps = 4;
    const f32 sub_dt    = dt / static_cast<f32>(sub_steps);
    for (i32 s = 0; s < sub_steps; ++s)
        step(sub_dt, speed_scale);
}

void AnimatedCurve::step(f32 dt, f32 speed_scale) {
    m_integrator->step(m_walk, *m_equation, *m_surface, 0.f, dt * speed_scale);
    for (const auto& c : m_constraints)
        c->apply(m_walk, *m_surface);
}

void AnimatedCurve::record_trail_sample(f32 t) {
    const Vec3 pt = m_surface->evaluate(m_walk.uv.x, m_walk.uv.y, t);
    if (m_trail_config.mode != TrailMode::None &&
        (m_trail.empty() || glm::length(pt - m_trail.back().world) > m_trail_config.min_spacing)) {
        m_trail.push_back(TrailSample{.uv = m_walk.uv, .world = pt, .time = t});
        const std::size_t max_points = m_trail_config.max_points > 0
            ? static_cast<std::size_t>(m_trail_config.max_points)
            : static_cast<std::size_t>(MAX_TRAIL);
        if (m_trail_config.mode == TrailMode::Finite && m_trail.size() > max_points)
            m_trail.erase(m_trail.begin());
    }
}

void AnimatedCurve::bind_behavior_stack() noexcept {
    if (auto* stack = dynamic_cast<BehaviorStack*>(m_equation))
        stack->set_owner(m_id);
}

void AnimatedCurve::set_behavior_context(const SimulationContext* context) noexcept {
    if (auto* stack = dynamic_cast<BehaviorStack*>(m_equation))
        stack->set_context(context);
}

ParticleMetadata AnimatedCurve::metadata() const {
    ParticleMetadata md;
    md.role = std::string(role_name(m_particle_role));
    if (auto* stack = dynamic_cast<const BehaviorStack*>(m_equation)) {
        md.behaviors = stack->behavior_labels();
    } else if (m_equation) {
        md.behaviors.push_back(m_equation->name());
    }
    md.constraints.reserve(m_constraints.size());
    for (const auto& constraint : m_constraints)
        if (constraint) md.constraints.push_back(constraint->name());
    md.label = metadata_label();
    return md;
}

std::string AnimatedCurve::metadata_label() const {
    std::string out = m_label.empty() ? std::string(role_name(m_particle_role)) : m_label;
    if (auto* stack = dynamic_cast<const BehaviorStack*>(m_equation)) {
        for (const std::string& label : stack->behavior_labels())
            out += " - " + label;
    } else if (m_equation) {
        out += " - " + m_equation->name();
    }
    return out;
}

f32 AnimatedCurve::max_delay_seconds() const noexcept {
    if (const auto* stack = dynamic_cast<const BehaviorStack*>(m_equation))
        return stack->max_delay_seconds();
    return 0.f;
}

f32 AnimatedCurve::max_nominal_speed() const noexcept {
    if (const auto* stack = dynamic_cast<const BehaviorStack*>(m_equation))
        return stack->max_nominal_speed();
    return 0.f;
}

void AnimatedCurve::enable_history(std::size_t capacity, f32 dt_min) {
    if (m_memory) {
        m_history = m_memory->history().make_unique<ndde::sim::HistoryBuffer>(
            capacity, dt_min,
            m_memory->history().make_vector<ndde::sim::HistoryBuffer::Record>());
        return;
    }
    std::pmr::memory_resource* owner_resource = m_trail.get_allocator().resource();
    m_history = memory::make_unique<ndde::sim::HistoryBuffer>(
        owner_resource, capacity, dt_min, owner_resource);
}

void AnimatedCurve::push_history(f32 t) {
    if (!m_history) return;
    m_history->push(t, m_walk.uv);
}

glm::vec2 AnimatedCurve::query_history(f32 t_past) const {
    if (!m_history) return m_walk.uv;
    return m_history->query(t_past);
}

Vec4 AnimatedCurve::trail_colour(Role role, u32 slot, f32 age_t) noexcept {
    static constexpr Vec3 leader_base[MAX_SLOTS] = {
        {0.35f, 0.75f, 1.00f}, {0.15f, 0.45f, 1.00f},
        {0.30f, 0.60f, 0.90f}, {0.55f, 0.65f, 1.00f},
    };
    static constexpr Vec3 chaser_base[MAX_SLOTS] = {
        {1.00f, 0.25f, 0.20f}, {1.00f, 0.05f, 0.15f},
        {1.00f, 0.40f, 0.30f}, {1.00f, 0.35f, 0.55f},
    };
    const u32  s    = slot % MAX_SLOTS;
    const Vec3 base = (role == Role::Leader) ? leader_base[s] : chaser_base[s];
    const f32  bright = 0.20f + 0.80f * age_t;
    const f32  alpha  = 0.30f + 0.70f * age_t;
    return { base.x * bright, base.y * bright, base.z * bright, alpha };
}

FrenetFrame AnimatedCurve::frenet_at(u32 idx) const noexcept {
    FrenetFrame fr;
    const u32 n = static_cast<u32>(m_trail.size());
    if (n < 4 || idx < 1 || idx >= n-1) return fr;

    const Vec3& pm = m_trail[idx-1].world;
    const Vec3& p0 = m_trail[idx].world;
    const Vec3& pp = m_trail[idx+1].world;
    const Vec3 v1 = p0 - pm;
    const Vec3 v2 = pp - p0;
    const f32  l1 = glm::length(v1);
    const f32  l2 = glm::length(v2);
    if (l1 < 1e-9f || l2 < 1e-9f) return fr;

    const Vec3 T1 = v1 / l1;
    const Vec3 T2 = v2 / l2;
    fr.T     = glm::normalize(T1 + T2);
    fr.speed = (l1 + l2) * 0.5f;

    const Vec3 dT  = T2 - T1;
    const f32  dTl = glm::length(dT);
    if (dTl > 1e-9f) {
        fr.N     = dT / dTl;
        fr.kappa = dTl / l1;
    } else {
        fr.N     = m_surface->unit_normal(p0.x, p0.y, 0.f);
        fr.kappa = 0.f;
    }
    fr.B = glm::normalize(glm::cross(fr.T, fr.N));

    if (idx >= 2) {
        const Vec3& pmm = m_trail[idx-2].world;
        const Vec3  va  = pm - pmm;
        const f32   la  = glm::length(va);
        if (la > 1e-9f) {
            const Vec3 T0      = va / la;
            const Vec3 B01_raw = glm::cross(T0, T1);
            const Vec3 B12_raw = glm::cross(T1, T2);
            const f32  b01l    = glm::length(B01_raw);
            const f32  b12l    = glm::length(B12_raw);
            if (b01l > 1e-9f && b12l > 1e-9f) {
                const Vec3 B01  = B01_raw / b01l;
                const Vec3 B12  = B12_raw / b12l;
                const f32 cos_a = std::clamp(glm::dot(B01, B12), -1.f, 1.f);
                const f32 alpha = ops::acos(cos_a);
                const f32 s     = glm::dot(glm::cross(B01, B12), T1);
                const f32 sign  = (s >= 0.f) ? 1.f : -1.f;
                const f32 ds    = 0.5f * (la + l1);
                fr.tau = sign * alpha / (ds + 1e-9f);
            }
        }
    }
    return fr;
}

u32 AnimatedCurve::trail_vertex_count() const noexcept {
    return static_cast<u32>(m_trail.size());
}

void AnimatedCurve::tessellate_trail(std::span<Vertex> out) const {
    const u32 n = trail_vertex_count();
    if (out.size() < n) return;
    for (u32 i = 0; i < n; ++i) {
        const f32 t = static_cast<f32>(i) / static_cast<f32>(n > 1 ? n-1 : 1);
        out[i] = { m_trail[i].world, trail_colour(m_role, m_colour_slot, t) };
    }
}

Vec3 AnimatedCurve::head_world() const noexcept {
    if (m_trail.empty()) return m_surface->evaluate(m_walk.uv.x, m_walk.uv.y);
    return m_trail.back().world;
}

} // namespace ndde
