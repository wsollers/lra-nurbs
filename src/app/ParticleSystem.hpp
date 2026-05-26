#pragma once
// app/ParticleSystem.hpp
// Scene-local owner for particles, particle constraints, and particle goals.

#include "app/AnimatedCurve.hpp"
#include "app/ParticleFactory.hpp"
#include "app/ParticleGoals.hpp"
#include "app/SimulationContext.hpp"
#include "engine/IScene.hpp"
#include "memory/Containers.hpp"
#include "memory/MemoryService.hpp"
#include "sim/EulerIntegrator.hpp"
#include "sim/IConstraint.hpp"
#include "sim/MilsteinIntegrator.hpp"
#include <random>
#include <string>
#include <utility>

namespace ndde {

namespace simulation { class FieldCompositor; }

class ParticleSystem {
public:
    explicit ParticleSystem(const ndde::math::ISurface* surface,
                            u32 seed = std::random_device{}())
        : m_surface(surface), m_rng(seed)
    {}

    void bind_memory(memory::MemoryService* memory) {
        m_memory = memory;
        if (memory) {
            memory->simulation().rebind_vector(m_particles);
            memory->simulation().rebind_vector(m_pair_constraints);
            memory->simulation().rebind_vector(m_goals);
        } else {
            rebind_vector_to_resource(m_particles, std::pmr::get_default_resource());
            rebind_vector_to_resource(m_pair_constraints, std::pmr::get_default_resource());
            rebind_vector_to_resource(m_goals, std::pmr::get_default_resource());
        }
        for (Particle& particle : m_particles)
            particle.bind_memory(memory);
    }

    void set_surface(const ndde::math::ISurface* surface) noexcept { m_surface = surface; }
    [[nodiscard]] const ndde::math::ISurface* surface() const noexcept { return m_surface; }

    [[nodiscard]] ParticleFactory factory() const noexcept { return ParticleFactory(m_surface, m_memory); }
    [[nodiscard]] std::mt19937& rng() noexcept { return m_rng; }

    [[nodiscard]] memory::SimVector<Particle>& particles() noexcept { return m_particles; }
    [[nodiscard]] const memory::SimVector<Particle>& particles() const noexcept { return m_particles; }
    [[nodiscard]] bool empty() const noexcept { return m_particles.empty(); }
    [[nodiscard]] std::size_t size() const noexcept { return m_particles.size(); }

    [[nodiscard]] memory::PersistentVector<ParticleSnapshot> snapshot_particles() const {
        memory::PersistentVector<ParticleSnapshot> out =
            m_memory ? m_memory->persistent().make_vector<ParticleSnapshot>()
                     : memory::PersistentVector<ParticleSnapshot>{};
        out.reserve(m_particles.size());
        for (const auto& particle : m_particles) {
            const glm::vec2 uv = particle.head_uv();
            const Vec3 head = particle.head_world();
            out.push_back(ParticleSnapshot{
                .id = particle.id(),
                .role = std::string(role_name(particle.particle_role())),
                .label = particle.metadata_label(),
                .u = uv.x,
                .v = uv.y,
                .x = head.x,
                .y = head.y,
                .z = head.z
            });
        }
        return out;
    }

    Particle& spawn(ParticleBuilder builder) {
        m_particles.push_back(builder.build(&m_euler, &m_milstein));
        return m_particles.back();
    }

    void set_behavior_context(const SimulationContext* context) noexcept {
        for (Particle& particle : m_particles)
            particle.set_behavior_context(context);
    }

    void clear() { m_particles.clear(); }
    void clear_goals() { m_goals.clear(); }
    void clear_pair_constraints() { m_pair_constraints.clear(); }
    void clear_all() {
        m_particles.clear();
        m_pair_constraints.clear();
        m_goals.clear();
    }

    void update(f32 dt, f32 speed_scale, f32 sim_time,
                const simulation::FieldCompositor* fields = nullptr) {
        SimulationContext context(m_surface, &m_particles, &m_rng, fields);
        context.set_time(sim_time);
        for (auto& particle : m_particles) {
            particle.set_behavior_context(&context);
            particle.advance(dt, speed_scale);
            particle.record_trail_sample(sim_time);
            particle.push_history(sim_time);
        }
        set_behavior_context(nullptr);
        apply_pair_constraints();
    }

    [[nodiscard]] SimulationContext context(f32 sim_time,
                                            const simulation::FieldCompositor* fields = nullptr) {
        SimulationContext c(m_surface, &m_particles, &m_rng, fields);
        c.set_time(sim_time);
        return c;
    }

    template <class Constraint, class... Args>
    Constraint& add_pair_constraint(Args&&... args) {
        auto owned = make_sim_unique<Constraint>(std::forward<Args>(args)...);
        Constraint& ref = *owned;
        auto c = memory::unique_cast<ndde::sim::IPairConstraint>(std::move(owned));
        m_pair_constraints.push_back(std::move(c));
        return ref;
    }

    template <class Goal, class... Args>
    Goal& add_goal(Args&&... args) {
        auto owned = make_sim_unique<Goal>(std::forward<Args>(args)...);
        Goal& ref = *owned;
        auto g = memory::unique_cast<IParticleGoal>(std::move(owned));
        m_goals.push_back(std::move(g));
        return ref;
    }

    [[nodiscard]] GoalStatus evaluate_goals(f32 sim_time) {
        SimulationContext c = context(sim_time);
        GoalStatus aggregate = GoalStatus::Running;
        for (const auto& goal : m_goals) {
            if (!goal) continue;
            const GoalStatus status = goal->evaluate(c);
            if (status != GoalStatus::Running)
                aggregate = status;
        }
        return aggregate;
    }

private:
    const ndde::math::ISurface* m_surface = nullptr;
    memory::MemoryService* m_memory = nullptr;
    ndde::sim::EulerIntegrator m_euler;
    ndde::sim::MilsteinIntegrator m_milstein;
    memory::SimVector<Particle> m_particles;
    memory::SimVector<memory::Unique<ndde::sim::IPairConstraint>> m_pair_constraints;
    memory::SimVector<memory::Unique<IParticleGoal>> m_goals;
    std::mt19937 m_rng;

    template <class T>
    static void rebind_vector_to_resource(memory::SimVector<T>& vector, std::pmr::memory_resource* resource) {
        if (vector.get_allocator().resource() == resource)
            return;
        memory::SimVector<T> rebound{resource};
        rebound.reserve(vector.size());
        for (auto& item : vector)
            rebound.push_back(std::move(item));
        std::destroy_at(&vector);
        std::construct_at(&vector, std::move(rebound));
    }

    template <class T, class... Args>
    [[nodiscard]] memory::Unique<T> make_sim_unique(Args&&... args) const {
        return m_memory ? m_memory->simulation().make_unique<T>(std::forward<Args>(args)...)
                        : memory::make_unique<T>(std::pmr::get_default_resource(), std::forward<Args>(args)...);
    }

    template <class Base, class Derived, class... Args>
    [[nodiscard]] memory::Unique<Base> make_sim_unique_as(Args&&... args) const {
        return m_memory ? m_memory->simulation().make_unique_as<Base, Derived>(std::forward<Args>(args)...)
                        : memory::unique_cast<Base>(
                              memory::make_unique<Derived>(std::pmr::get_default_resource(),
                                                           std::forward<Args>(args)...));
    }

    void apply_pair_constraints() {
        if (!m_surface || m_pair_constraints.empty()) return;
        for (auto& constraint : m_pair_constraints) {
            if (!constraint) continue;
            for (std::size_t i = 0; i < m_particles.size(); ++i) {
                for (std::size_t j = i + 1; j < m_particles.size(); ++j) {
                    constraint->apply(m_particles[i].walk_state(),
                                      m_particles[j].walk_state(),
                                      *m_surface);
                }
            }
        }
    }
};

} // namespace ndde
