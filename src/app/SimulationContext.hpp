#pragma once
// app/SimulationContext.hpp
// Simulation state/context.
//
// Compatibility note: existing particle behaviors currently receive a
// lightweight view over surface/particle/RNG data.  The same type is being
// extended into the owned simulation state container used by new
// ISimulation-based simulations.

#include "app/ParticleTypes.hpp"
#include "engine/SimulationClock.hpp"
#include "math/Surfaces.hpp"
#include "memory/Containers.hpp"
#include "sim/HistoryBuffer.hpp"
#include <glm/glm.hpp>
#include <optional>
#include <random>

namespace ndde {

class AnimatedCurve;
namespace simulation { class FieldCompositor; }

struct SurfacePerturbation {
    Vec2 uv{0.f, 0.f};
    f32 amplitude = 0.f;
    f32 radius = 0.f;
    f32 falloff = 1.f;
    u32 seed = 0u;
};

struct SimulationDirtyState {
    bool surface = false;
    bool particles = false;
    bool history = false;
    bool main_view = false;
    bool alternate_view = false;
    bool hover_math = false;

    void mark_surface_changed() noexcept {
        surface = true;
        main_view = true;
        alternate_view = true;
        hover_math = true;
    }

    void mark_particles_changed() noexcept {
        particles = true;
        history = true;
        main_view = true;
        alternate_view = true;
        hover_math = true;
    }

    void clear() noexcept {
        surface = false;
        particles = false;
        history = false;
        main_view = false;
        alternate_view = false;
        hover_math = false;
    }

    [[nodiscard]] bool any() const noexcept {
        return surface || particles || history || main_view || alternate_view || hover_math;
    }
};

struct SimulationMathCache {
    u64 surface_revision = 0;
    u64 particle_revision = 0;
    u64 hover_revision = 0;

    void bump_surface() noexcept { ++surface_revision; ++hover_revision; }
    void bump_particles() noexcept { ++particle_revision; ++hover_revision; }
};

struct SimulationCommandState {
    memory::FrameVector<SurfacePerturbation> surface_perturbations;

    void push(SurfacePerturbation perturbation) {
        surface_perturbations.push_back(perturbation);
    }

    void clear() noexcept {
        surface_perturbations.clear();
    }
};

class SimulationContext {
public:
    SimulationContext() = default;

    SimulationContext(const ndde::math::ISurface* surface,
                      const memory::SimVector<AnimatedCurve>* particles,
                      std::mt19937* rng,
                      const simulation::FieldCompositor* fields = nullptr) noexcept
        : m_surface(surface), m_particles(particles), m_rng(rng), m_fields(fields)
    {}

    void set_time(f32 t) noexcept { m_time = t; }
    [[nodiscard]] f32 time() const noexcept { return m_time; }
    void set_tick(TickInfo tick) noexcept {
        m_tick = tick;
        m_time = tick.time;
    }
    [[nodiscard]] TickInfo tick() const noexcept { return m_tick; }

    [[nodiscard]] const ndde::math::ISurface& surface() const noexcept { return *m_surface; }
    void set_surface(const ndde::math::ISurface* surface) noexcept {
        m_surface = surface;
        m_dirty.mark_surface_changed();
        m_math_cache.bump_surface();
    }

    [[nodiscard]] std::mt19937& rng() const noexcept { return *m_rng; }
    void set_rng(std::mt19937* rng) noexcept { m_rng = rng; }
    void set_fields(const simulation::FieldCompositor* fields) noexcept { m_fields = fields; }
    [[nodiscard]] const simulation::FieldCompositor* fields() const noexcept { return m_fields; }
    [[nodiscard]] bool has_fields() const noexcept { return m_fields != nullptr; }
    void set_particles(const memory::SimVector<AnimatedCurve>* particles) noexcept {
        m_particles = particles;
        m_dirty.mark_particles_changed();
        m_math_cache.bump_particles();
    }

    [[nodiscard]] const memory::SimVector<AnimatedCurve>& particles() const noexcept {
        return *m_particles;
    }

    [[nodiscard]] bool has_surface() const noexcept { return m_surface != nullptr; }
    [[nodiscard]] bool has_particles() const noexcept { return m_particles != nullptr; }
    [[nodiscard]] bool has_rng() const noexcept { return m_rng != nullptr; }

    [[nodiscard]] SimulationDirtyState& dirty() noexcept { return m_dirty; }
    [[nodiscard]] const SimulationDirtyState& dirty() const noexcept { return m_dirty; }
    [[nodiscard]] SimulationMathCache& math_cache() noexcept { return m_math_cache; }
    [[nodiscard]] const SimulationMathCache& math_cache() const noexcept { return m_math_cache; }
    [[nodiscard]] SimulationCommandState& commands() noexcept { return m_commands; }
    [[nodiscard]] const SimulationCommandState& commands() const noexcept { return m_commands; }

    void queue_perturbation(SurfacePerturbation perturbation) {
        m_commands.push(perturbation);
        m_dirty.mark_surface_changed();
        m_math_cache.bump_surface();
    }

    [[nodiscard]] bool has_pending_perturbations() const noexcept {
        return !m_commands.surface_perturbations.empty();
    }

    void clear_frame_state() noexcept {
        m_dirty.clear();
        m_commands.clear();
    }

    [[nodiscard]] const AnimatedCurve* find(ParticleId id) const noexcept;
    [[nodiscard]] const AnimatedCurve* first(ParticleRole role, ParticleId exclude = 0) const noexcept;
    [[nodiscard]] const AnimatedCurve* nearest(ParticleRole role, glm::vec2 from, ParticleId exclude = 0) const noexcept;
    [[nodiscard]] glm::vec2 centroid(ParticleRole role, ParticleId exclude = 0) const noexcept;

private:
    const ndde::math::ISurface* m_surface = nullptr;
    const memory::SimVector<AnimatedCurve>* m_particles = nullptr;
    std::mt19937* m_rng = nullptr;
    const simulation::FieldCompositor* m_fields = nullptr;
    f32 m_time = 0.f;
    TickInfo m_tick{};
    SimulationDirtyState m_dirty{};
    SimulationMathCache m_math_cache{};
    SimulationCommandState m_commands{};
};

} // namespace ndde
