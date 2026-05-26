// app/GaussianSurface.cpp
#include "app/GaussianSurface.hpp"
#include "app/AnimatedCurve.hpp"
#include "app/FrenetFrame.hpp"
#include "app/ParticleBehaviors.hpp"
#include "sim/IEquation.hpp"
#include "sim/IIntegrator.hpp"
#include "sim/HistoryBuffer.hpp"
#include "sim/DomainConfinement.hpp"
#include "numeric/ops.hpp"
#include <cmath>
#include <stdexcept>
#include <algorithm>
#include <numbers>
#include <glm/glm.hpp>

namespace ndde {

// == GaussianSurface::eval_static ============================================
// Renamed from eval() -- identical computation.
// The static alias eval() in the header forwards here.

f32 GaussianSurface::eval_static(f32 x, f32 y) noexcept {
    const f32 g0 =  1.6f * ops::exp(-((x-1.5f)*(x-1.5f)/0.6f  + (y-0.4f)*(y-0.4f)/0.9f));
    const f32 g1 = -1.3f * ops::exp(-((x+1.3f)*(x+1.3f)/0.8f  + (y+1.1f)*(y+1.1f)/0.5f));
    const f32 g2 =  1.1f * ops::exp(-((x+0.2f)*(x+0.2f)/1.2f  + (y-2.0f)*(y-2.0f)/0.4f));
    const f32 g3 = -0.8f * ops::exp(-((x-2.5f)*(x-2.5f)/0.5f  + (y+0.7f)*(y+0.7f)/1.0f));
    const f32 g4 =  0.7f * ops::exp(-((x-0.8f)*(x-0.8f)/0.7f  + (y+2.3f)*(y+2.3f)/0.6f));
    const f32 g5 = -0.5f * ops::exp(-((x+2.8f)*(x+2.8f)/0.3f  + (y-1.4f)*(y-1.4f)/0.8f));
    const f32 s0 =  0.15f * ops::sin(2.0f*x) * ops::sin(3.0f*y);
    const f32 s1 =  0.12f * ops::cos(1.5f*x - 1.0f) * ops::sin(2.5f*y + 0.7f);
    return g0 + g1 + g2 + g3 + g4 + g5 + s0 + s1;
}

// == GaussianSurface::grad ====================================================

GaussianSurface::Grad GaussianSurface::grad(f32 x, f32 y) noexcept {
    constexpr f32 h = 1e-3f;
    return {
        (eval(x+h,y) - eval(x-h,y)) / (2.f*h),
        (eval(x,y+h) - eval(x,y-h)) / (2.f*h)
    };
}

// == GaussianSurface::du / dv ================================================
// ISurface overrides -- central finite difference on the height field.
// p(u,v) = (u, v, f(u,v)), so:
//   du = (1, 0, df/du)   and   dv = (0, 1, df/dv)
// We compute df/du and df/dv via the existing grad() central-difference.

Vec3 GaussianSurface::du(float u, float v, float /*t*/) const {
    const auto [fx, fy] = grad(u, v);
    return Vec3{1.f, 0.f, fx};
}

Vec3 GaussianSurface::dv(float u, float v, float /*t*/) const {
    const auto [fx, fy] = grad(u, v);
    return Vec3{0.f, 1.f, fy};
}

// == GaussianSurface::unit_normal =============================================

Vec3 GaussianSurface::unit_normal(f32 x, f32 y) noexcept {
    const auto [fx, fy] = grad(x, y);
    const f32 len = ops::sqrt(fx*fx + fy*fy + 1.f);
    return { -fx/len, -fy/len, 1.f/len };
}

// == GaussianSurface::gaussian_curvature ======================================

f32 GaussianSurface::gaussian_curvature(f32 x, f32 y) noexcept {
    constexpr f32 h = 1e-3f;
    const auto [fx, fy] = grad(x, y);
    const f32 fxx = (eval(x+h,y) - 2.f*eval(x,y) + eval(x-h,y)) / (h*h);
    const f32 fyy = (eval(x,y+h) - 2.f*eval(x,y) + eval(x,y-h)) / (h*h);
    const f32 fxy = (eval(x+h,y+h) - eval(x+h,y-h) - eval(x-h,y+h) + eval(x-h,y-h)) / (4.f*h*h);
    const f32 E = 1.f+fx*fx, F = fx*fy, G = 1.f+fy*fy;
    const f32 n = ops::sqrt(1.f + fx*fx + fy*fy);
    const f32 L = fxx/n, M = fxy/n, N2 = fyy/n;
    const f32 denom = E*G - F*F;
    return ops::abs(denom) < 1e-10f ? 0.f : (L*N2 - M*M) / denom;
}

// == GaussianSurface::mean_curvature ==========================================

f32 GaussianSurface::mean_curvature(f32 x, f32 y) noexcept {
    constexpr f32 h = 1e-3f;
    const auto [fx, fy] = grad(x, y);
    const f32 fxx = (eval(x+h,y) - 2.f*eval(x,y) + eval(x-h,y)) / (h*h);
    const f32 fyy = (eval(x,y+h) - 2.f*eval(x,y) + eval(x,y-h)) / (h*h);
    const f32 fxy = (eval(x+h,y+h) - eval(x+h,y-h) - eval(x-h,y+h) + eval(x-h,y-h)) / (4.f*h*h);
    const f32 E = 1.f+fx*fx, F = fx*fy, G = 1.f+fy*fy;
    const f32 n = ops::sqrt(1.f + fx*fx + fy*fy);
    const f32 L = fxx/n, M = fxy/n, N2 = fyy/n;
    const f32 denom = 2.f*(E*G - F*F);
    return ops::abs(denom) < 1e-10f ? 0.f : (E*N2 + G*L - 2.f*F*M) / denom;
}

// == GaussianSurface::height_color ============================================

Vec4 GaussianSurface::height_color(f32 z) noexcept {
    const f32 t = std::clamp((z - Z_MIN) / (Z_MAX - Z_MIN), 0.f, 1.f);
    struct Stop { f32 pos; Vec3 rgb; };
    static constexpr Stop stops[] = {
        {0.00f, {0.165f, 0.000f, 0.431f}},
        {0.15f, {0.000f, 0.157f, 0.706f}},
        {0.35f, {0.000f, 0.627f, 0.784f}},
        {0.50f, {0.000f, 0.784f, 0.471f}},
        {0.65f, {0.784f, 0.863f, 0.000f}},
        {0.80f, {1.000f, 0.627f, 0.000f}},
        {1.00f, {0.784f, 0.000f, 0.118f}}
    };
    constexpr int N = 7;
    int lo = 0;
    for (int i = 0; i < N-1; ++i) {
        if (t >= stops[i].pos && t <= stops[i+1].pos) { lo = i; break; }
        lo = N-2;
    }
    const f32 span = stops[lo+1].pos - stops[lo].pos;
    const f32 dt   = span < 1e-6f ? 0.f : (t - stops[lo].pos) / span;
    const Vec3 c = stops[lo].rgb + dt*(stops[lo+1].rgb - stops[lo].rgb);
    return {c.x, c.y, c.z, 1.f};
}

// == GaussianSurface::wireframe_vertex_count ==================================

u32 GaussianSurface::wireframe_vertex_count(u32 u_lines, u32 v_lines) noexcept {
    return (u_lines + 1u) * v_lines * 2u +
           (v_lines + 1u) * u_lines * 2u;
}

// == GaussianSurface::tessellate_wireframe ====================================

void GaussianSurface::tessellate_wireframe(std::span<Vertex> out,
                                            u32 u_lines, u32 v_lines) {
    const u32 needed = wireframe_vertex_count(u_lines, v_lines);
    if (out.size() < needed)
        throw std::invalid_argument("[GaussianSurface] span too small");

    const f32 xs = (XMAX - XMIN) / static_cast<f32>(u_lines);
    const f32 ys = (YMAX - YMIN) / static_cast<f32>(v_lines);
    u32 idx = 0;

    for (u32 i = 0; i <= u_lines; ++i) {
        const f32 x = XMIN + static_cast<f32>(i) * xs;
        for (u32 j = 0; j < v_lines; ++j) {
            const f32 ya = YMIN + static_cast<f32>(j)   * ys;
            const f32 yb = YMIN + static_cast<f32>(j+1) * ys;
            const f32 za = eval(x, ya), zb = eval(x, yb);
            out[idx++] = { Vec3{x, ya, za}, height_color(za) };
            out[idx++] = { Vec3{x, yb, zb}, height_color(zb) };
        }
    }
    for (u32 j = 0; j <= v_lines; ++j) {
        const f32 y = YMIN + static_cast<f32>(j) * ys;
        for (u32 i = 0; i < u_lines; ++i) {
            const f32 xa = XMIN + static_cast<f32>(i)   * xs;
            const f32 xb = XMIN + static_cast<f32>(i+1) * xs;
            const f32 za = eval(xa, y), zb = eval(xb, y);
            out[idx++] = { Vec3{xa, y, za}, height_color(za) };
            out[idx++] = { Vec3{xb, y, zb}, height_color(zb) };
        }
    }
}

// == GaussianSurface::contour_max_vertices ====================================

u32 GaussianSurface::contour_max_vertices(u32 grid_n, u32 n_levels) noexcept {
    return grid_n * grid_n * 4u * n_levels;
}

// == GaussianSurface::tessellate_contours =====================================

u32 GaussianSurface::tessellate_contours(std::span<Vertex> out,
                                          u32 grid_n,
                                          const float* levels, u32 n_levels,
                                          Vec4 color) {
    u32 idx = 0;
    const f32 dx = (XMAX-XMIN) / static_cast<f32>(grid_n);
    const f32 dy = (YMAX-YMIN) / static_cast<f32>(grid_n);

    for (u32 li = 0; li < n_levels; ++li) {
        const f32 lv = levels[li];
        for (u32 i = 0; i < grid_n; ++i) {
            for (u32 j = 0; j < grid_n; ++j) {
                const f32 x0 = XMIN + static_cast<f32>(i)   * dx;
                const f32 x1 = XMIN + static_cast<f32>(i+1) * dx;
                const f32 y0 = YMIN + static_cast<f32>(j)   * dy;
                const f32 y1 = YMIN + static_cast<f32>(j+1) * dy;
                const f32 v00 = eval(x0,y0), v10 = eval(x1,y0);
                const f32 v01 = eval(x0,y1), v11 = eval(x1,y1);

                f32 pts[8]; u32 np = 0;
                auto seg = [&](f32 va, f32 vb, f32 xa, f32 ya, f32 xb, f32 yb) {
                    if ((va-lv)*(vb-lv) < 0.f) {
                        const f32 t = (lv-va)/(vb-va);
                        pts[np++] = xa + t*(xb-xa);
                        pts[np++] = ya + t*(yb-ya);
                    }
                };
                seg(v00,v10, x0,y0, x1,y0);
                seg(v10,v11, x1,y0, x1,y1);
                seg(v11,v01, x1,y1, x0,y1);
                seg(v01,v00, x0,y1, x0,y0);

                if (np >= 4 && idx+2 <= out.size()) {
                    out[idx++] = { Vec3{pts[0], pts[1], 0.f}, color };
                    out[idx++] = { Vec3{pts[2], pts[3], 0.f}, color };
                }
            }
        }
    }
    return idx;
}

// =============================================================================
// AnimatedCurve
// =============================================================================

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
    , m_owned_equation(nullptr)   // shared equation -- ownership is external
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
        if (m_constraints.get_allocator().resource() == desired_constraints)
            return;
        memory::SimVector<memory::Unique<ndde::sim::IConstraint>> rebound{desired_constraints};
        rebound.reserve(m_constraints.size());
        for (auto& constraint : m_constraints)
            rebound.push_back(std::move(constraint));
        std::destroy_at(&m_constraints);
        std::construct_at(&m_constraints, std::move(rebound));
    }

    if (m_history && memory) {
        // Existing history buffers are scope-owned; callers should bind memory
        // before enabling history so stale buffers do not cross a scope reset.
        m_history.get_deleter().assert_alive();
    }
}

// static factory: particle owns its equation
// static
AnimatedCurve AnimatedCurve::with_equation(
    f32 start_x, f32 start_y,
    Role role, u32 colour_slot,
    const ndde::math::ISurface*           surface,
    memory::Unique<ndde::sim::IEquation> owned_equation,
    const ndde::sim::IIntegrator*         integrator,
    memory::MemoryService*                mem)
{
    AnimatedCurve c(start_x, start_y, role, colour_slot,
                    surface, owned_equation.get(), integrator, mem);
    c.m_owned_equation = std::move(owned_equation);
    // m_equation already points to m_owned_equation.get() via the constructor
    return c;
}

void AnimatedCurve::reset() {
    m_trail.clear();
    m_walk.uv    = { m_start_x, m_start_y };
    m_walk.phase = 0.f;
    m_walk.angle = 0.f;
}

void AnimatedCurve::advance(f32 dt, f32 speed_scale) {
    const int sub_steps = 4;
    const f32 sub_dt    = dt / static_cast<f32>(sub_steps);
    for (int s = 0; s < sub_steps; ++s)
        step(sub_dt, speed_scale);
}

void AnimatedCurve::step(f32 dt, f32 speed_scale) {
    // Step 5 / A1: pass m_walk directly to the integrator (no copy-in/copy-out).
    // The integrator calls m_equation->velocity(state, ...) with mutable state,
    // so GradientWalker updates state.angle in place -- no const_cast needed.
    m_integrator->step(m_walk, *m_equation, *m_surface, 0.f, dt * speed_scale);

    // Apply constraints (boundary handling, pairwise, etc.)
    for (const auto& c : m_constraints)
        c->apply(m_walk, *m_surface);

}

void AnimatedCurve::record_trail_sample(float t) {
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
    for (const auto& constraint : m_constraints) {
        if (constraint) md.constraints.push_back(constraint->name());
    }
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

float AnimatedCurve::max_delay_seconds() const noexcept {
    if (const auto* stack = dynamic_cast<const BehaviorStack*>(m_equation))
        return stack->max_delay_seconds();
    return 0.f;
}

float AnimatedCurve::max_nominal_speed() const noexcept {
    if (const auto* stack = dynamic_cast<const BehaviorStack*>(m_equation))
        return stack->max_nominal_speed();
    return 0.f;
}

// == AnimatedCurve::history methods ==========================================

void AnimatedCurve::enable_history(std::size_t capacity, float dt_min) {
    if (m_memory) {
        m_history = m_memory->history().make_unique<ndde::sim::HistoryBuffer>(
            capacity, dt_min, m_memory->history().make_vector<ndde::sim::HistoryBuffer::Record>());
        return;
    }

    std::pmr::memory_resource* owner_resource = m_trail.get_allocator().resource();
    m_history = memory::make_unique<ndde::sim::HistoryBuffer>(owner_resource, capacity, dt_min, owner_resource);
}

void AnimatedCurve::push_history(float t) {
    if (!m_history) return;
    // Push the current parameter-space position (the walk state, not the trail).
    // The trail stores world-space Vec3; we need the (u,v) pair for the DDE.
    m_history->push(t, m_walk.uv);
}

glm::vec2 AnimatedCurve::query_history(float t_past) const {
    if (!m_history) return m_walk.uv;  // fallback: current pos
    return m_history->query(t_past);
}

// == AnimatedCurve::trail_colour ==============================================
//
// Leaders -> blue family.   Chasers -> red family.
// slot 0 = brightest/most-saturated, slot 3 = softer/more-muted.
// age_t in [0,1]: 0 = oldest trail point (dim), 1 = head (full brightness).
//
// Colour design:
//   Leaders (blue):
//     slot 0 -- bright sky-blue      (0.35, 0.75, 1.00)
//     slot 1 -- royal blue           (0.15, 0.45, 1.00)
//     slot 2 -- steel blue           (0.30, 0.60, 0.90)
//     slot 3 -- periwinkle           (0.55, 0.65, 1.00)
//   Chasers (red):
//     slot 0 -- bright coral-red     (1.00, 0.25, 0.20)
//     slot 1 -- crimson              (1.00, 0.05, 0.15)
//     slot 2 -- tomato               (1.00, 0.40, 0.30)
//     slot 3 -- rose                 (1.00, 0.35, 0.55)

Vec4 AnimatedCurve::trail_colour(Role role, u32 slot, f32 age_t) noexcept {
    // Base RGB for each slot, at full brightness.
    static constexpr Vec3 leader_base[MAX_SLOTS] = {
        {0.35f, 0.75f, 1.00f},   // slot 0: sky-blue
        {0.15f, 0.45f, 1.00f},   // slot 1: royal blue
        {0.30f, 0.60f, 0.90f},   // slot 2: steel blue
        {0.55f, 0.65f, 1.00f},   // slot 3: periwinkle
    };
    static constexpr Vec3 chaser_base[MAX_SLOTS] = {
        {1.00f, 0.25f, 0.20f},   // slot 0: coral-red
        {1.00f, 0.05f, 0.15f},   // slot 1: crimson
        {1.00f, 0.40f, 0.30f},   // slot 2: tomato
        {1.00f, 0.35f, 0.55f},   // slot 3: rose
    };

    const u32  s    = slot % MAX_SLOTS;
    const Vec3 base = (role == Role::Leader) ? leader_base[s] : chaser_base[s];

    // Dim the tail: scale RGB down to ~20% at age=0, full at age=1.
    const f32 bright = 0.20f + 0.80f * age_t;
    const f32 alpha  = 0.30f + 0.70f * age_t;
    return { base.x * bright, base.y * bright, base.z * bright, alpha };
}

// == AnimatedCurve::frenet_at =================================================
//
// Computes T, N, B, kappa, tau at trail[idx] using finite differences.
//
// CURVATURE: 3-point stencil  [idx-1, idx, idx+1]
//   T1 = normalize(p0 - pm),  T2 = normalize(pp - p0)
//   N  = normalize(T2 - T1),  kappa = |T2 - T1| / l1
//
// TORSION: 4-point stencil  [idx-2, idx-1, idx, idx+1]
//   Three edge-tangents T0, T1, T2.
//   B01 = normalize(T0 x T1),  B12 = normalize(T1 x T2)
//   tau = signed_angle(B01, B12, T1) / ds
//   sign = sgn( (B01 x B12).T1 )   [right-hand rule]
//   ds   = 0.5*(|seg_0| + |seg_1|)
//
// Index bounds at the head (idx = n-2):
//   idx-2 = n-4 >= 0  requires n >= 4  (enforced by early-exit guard).
//   idx+1 = n-1 < n   always true.

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
        // Degenerate: curve is locally straight (zero discrete curvature).
        // Use the surface normal as a fallback N so B = T x N is well-defined.
        // We pass t=0.f here because trail points are world-space snapshots
        // without timestamps -- the exact surface time at push is not stored.
        // For static surfaces this is exact; for deforming surfaces it is an
        // approximation (the fallback path is rare in practice).
        fr.N     = m_surface->unit_normal(p0.x, p0.y, 0.f);
        fr.kappa = 0.f;
    }
    fr.B = glm::normalize(glm::cross(fr.T, fr.N));

    // Torsion -- signed rotation of the binormal per unit arc-length.
    if (idx >= 2) {
        const Vec3& pmm = m_trail[idx - 2].world;
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

// == AnimatedCurve::trail_vertex_count / tessellate_trail / head_world ========

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
