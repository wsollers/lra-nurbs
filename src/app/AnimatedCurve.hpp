#pragma once
// app/AnimatedCurve.hpp
// AnimatedCurve: one walker particle on a parametric surface.
//
// Moved from GaussianSurface.hpp (B1 refactor) so that particle helpers and
// SpawnStrategy can include this type without pulling in GaussianSurface.
//
// Ownership model
// ───────────────
// Standard construction: surface, equation, and integrator pointers are all
// non-owning.  All three must outlive the AnimatedCurve.
//
// with_equation() factory: the particle OWNS its equation via m_owned_equation
// (memory::Unique).  m_equation is an alias to m_owned_equation.get().  This
// enables per-particle SDE equations (BrownianMotion, DelayPursuitEquation).
//
// Move semantics: AnimatedCurve is moveable but not copyable.  The owned pointer
// members (m_owned_equation, m_history) move safely; m_equation (raw pointer
// alias) remains valid because unique_ptr move preserves the heap address.
//
// History recording (for delay-pursuit DDE)
// ─────────────────────────────────────────
// enable_history() allocates a ring buffer.  push_history() records the
// current parameter-space position after each advance() call.  query_history()
// interpolates to an arbitrary past time.  The buffer pointer is stable across
// vector reallocations (memory::Unique move does not change object address).
//
// Live equation access
// ─────────────────────
// equation() returns a non-owning pointer to the active IEquation (owned or
// shared).  Callers may dynamic_cast to a concrete type to mutate parameters
// at runtime without respawning the particle:
//   if (auto* bm = dynamic_cast<ndde::sim::BrownianMotion*>(c.equation()))
//       bm->params().sigma = 0.2f;
// This is the mechanism used by the live Brownian tuning sliders in the UI.

#include "math/Scalars.hpp"
#include "math/Surfaces.hpp"
#include "sim/IEquation.hpp"
#include "sim/IIntegrator.hpp"
#include "sim/IConstraint.hpp"
#include "sim/HistoryBuffer.hpp"
#include "app/ParticleBehaviors.hpp"
#include "app/FrenetFrame.hpp"
#include "app/ParticleTypes.hpp"
#include "math/GeometryTypes.hpp"
#include "memory/Containers.hpp"
#include "memory/MemoryService.hpp"
#include "memory/Unique.hpp"
#include <glm/glm.hpp>
#include <span>
#include <atomic>
#include <string>

namespace ndde {

class SimulationContext;

class AnimatedCurve {
public:
    enum class Role : u8 { Leader, Chaser };

    static constexpr u32 MAX_TRAIL   = 1200;
    static constexpr f32 WALK_SPEED  = 0.9f;
    static constexpr u32 MAX_SLOTS   = 4u;

    // Standard constructor: all three pointers are non-owning.
    // surface, equation, integrator must outlive this AnimatedCurve.
    explicit AnimatedCurve(f32 start_x, f32 start_y,
                           Role role,
                           u32 colour_slot,
                           const ndde::math::ISurface*   surface,
                           ndde::sim::IEquation*          equation,
                           const ndde::sim::IIntegrator*  integrator,
                           memory::MemoryService*         memory = nullptr);

    // Factory: construct a particle that OWNS its equation.
    // The owned pointer is moved into m_owned_equation; m_equation is set to
    // m_owned_equation.get().  All other pointers remain non-owning.
    static AnimatedCurve with_equation(
        f32 start_x, f32 start_y,
        Role role, u32 colour_slot,
        const ndde::math::ISurface*          surface,
        memory::Unique<ndde::sim::IEquation> owned_equation,
        const ndde::sim::IIntegrator*         integrator,
        memory::MemoryService*                memory = nullptr);

    // AnimatedCurve is moveable (unique_ptr members) but not copyable.
    ~AnimatedCurve() = default;
    AnimatedCurve(const AnimatedCurve&)            = delete;
    AnimatedCurve& operator=(const AnimatedCurve&) = delete;
    AnimatedCurve(AnimatedCurve&&)                 = default;
    AnimatedCurve& operator=(AnimatedCurve&&)      = default;

    void reset();
    void advance(f32 dt, f32 speed_scale = 1.f);
    void record_trail_sample(f32 t);

    [[nodiscard]] FrenetFrame frenet_at(u32 idx) const noexcept;

    [[nodiscard]] u32  trail_vertex_count() const noexcept;
    void tessellate_trail(std::span<Vertex> out) const;
    [[nodiscard]] Vec3 head_world() const noexcept;

    // head_uv: parameter-space position of the particle head.
    // Used by SpawnStrategy, DirectPursuitEquation, and export_session.
    // Note: this is the navigation coordinate -- arrival is detected by
    // neighbourhood radius, not exact equality.
    [[nodiscard]] glm::vec2 head_uv() const noexcept { return m_walk.uv; }
    [[nodiscard]] ParticleId id() const noexcept { return m_id; }
    [[nodiscard]] ParticleRole particle_role() const noexcept { return m_particle_role; }
    void set_particle_role(ParticleRole role) noexcept { m_particle_role = role; }
    void set_label(std::string label) { m_label = std::move(label); }
    void set_trail_config(TrailConfig cfg) noexcept { m_trail_config = cfg; }
    [[nodiscard]] const TrailConfig& trail_config() const noexcept { return m_trail_config; }

    void bind_behavior_stack() noexcept;
    void set_behavior_context(const SimulationContext* context) noexcept;
    [[nodiscard]] ParticleMetadata metadata() const;
    [[nodiscard]] std::string metadata_label() const;
    [[nodiscard]] f32 max_delay_seconds() const noexcept;
    [[nodiscard]] f32 max_nominal_speed() const noexcept;

    [[nodiscard]] u32  trail_size()    const noexcept { return static_cast<u32>(m_trail.size()); }
    [[nodiscard]] bool has_trail()     const noexcept { return m_trail.size() >= 4; }
    [[nodiscard]] const TrailSample& trail_sample(u32 i) const noexcept { return m_trail[i]; }
    [[nodiscard]] Vec3 trail_pt(u32 i) const noexcept { return m_trail[i].world; }
    [[nodiscard]] glm::vec2 trail_uv(u32 i) const noexcept { return m_trail[i].uv; }
    [[nodiscard]] f32 trail_time(u32 i) const noexcept { return m_trail[i].time; }
    [[nodiscard]] Role role()          const noexcept { return m_role; }
    [[nodiscard]] u32  colour_slot()   const noexcept { return m_colour_slot; }

    // ── History recording (delay-pursuit DDE) ─────────────────────────────────
    void enable_history(std::size_t capacity = 4096, f32 dt_min = 1.f/120.f);
    void push_history(f32 t);
    [[nodiscard]] glm::vec2 query_history(f32 t_past) const;

    // Non-owning access to the history buffer (null if not enabled).
    // The pointer is stable across vector reallocations.
    [[nodiscard]] const ndde::sim::HistoryBuffer* history() const noexcept {
        return m_history.get();
    }

    // Non-owning access to the equation (owned or shared).
    // dynamic_cast to a concrete type to mutate params live, e.g.:
    //   dynamic_cast<ndde::sim::BrownianMotion*>(c.equation())
    [[nodiscard]] ndde::sim::IEquation*       equation()       noexcept { return m_equation; }
    [[nodiscard]] const ndde::sim::IEquation* equation() const noexcept { return m_equation; }

    template <class Equation>
    [[nodiscard]] Equation* find_equation() noexcept {
        if (auto* eq = dynamic_cast<Equation*>(m_equation))
            return eq;
        if (auto* stack = dynamic_cast<BehaviorStack*>(m_equation))
            return stack->find_equation<Equation>();
        return nullptr;
    }

    template <class Equation>
    [[nodiscard]] const Equation* find_equation() const noexcept {
        if (const auto* eq = dynamic_cast<const Equation*>(m_equation))
            return eq;
        if (const auto* stack = dynamic_cast<const BehaviorStack*>(m_equation))
            return stack->find_equation<Equation>();
        return nullptr;
    }

    // Add a constraint applied after every integration sub-step.
    // AnimatedCurve owns the constraint via memory::Unique.
    // Constraints are applied in insertion order.
    void add_constraint(memory::Unique<ndde::sim::IConstraint> c) {
        m_constraints.push_back(std::move(c));
    }

    void bind_memory(memory::MemoryService* memory);

    // Mutable access to the particle state for pairwise constraint application.
    // Prefer ParticleSystem for new simulation code.
    // Do not use from equation or integrator code.
    [[nodiscard]] ndde::sim::ParticleState& walk_state() noexcept { return m_walk; }
    [[nodiscard]] const ndde::sim::ParticleState& walk_state() const noexcept { return m_walk; }

    [[nodiscard]] static Vec4 trail_colour(Role role, u32 slot, f32 age_t) noexcept;

    [[nodiscard]] Vec4 head_colour() const noexcept {
        return trail_colour(m_role, m_colour_slot, 1.f);
    }

private:
    ndde::sim::ParticleState                m_walk;
    memory::HistoryVector<TrailSample>       m_trail;
    const ndde::math::ISurface*              m_surface;        // non-owning, never null
    ndde::sim::IEquation*                    m_equation;       // non-owning OR alias to m_owned_equation
    memory::Unique<ndde::sim::IEquation>     m_owned_equation; // null when using shared equation
    const ndde::sim::IIntegrator*            m_integrator;     // non-owning, never null
    memory::Unique<ndde::sim::HistoryBuffer> m_history;        // null unless enable_history() called
    memory::SimVector<memory::Unique<ndde::sim::IConstraint>> m_constraints; // applied after each sub-step
    memory::MemoryService*                    m_memory = nullptr;
    Role                                     m_role;
    u32                                      m_colour_slot;
    f32                                      m_start_x, m_start_y;
    ParticleId                               m_id = next_id();
    ParticleRole                             m_particle_role = ParticleRole::Neutral;
    TrailConfig                              m_trail_config{};
    std::string                              m_label;

    void step(f32 dt, f32 speed_scale);

    [[nodiscard]] static ParticleId next_id() noexcept {
        static std::atomic<ParticleId> id{0};
        return ++id;
    }
};

} // namespace ndde
