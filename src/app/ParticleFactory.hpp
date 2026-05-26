#pragma once
// app/ParticleFactory.hpp
// Fluent builder for composable particles.

#include "app/AnimatedCurve.hpp"
#include "app/ParticleBehaviors.hpp"
#include "memory/Containers.hpp"
#include "memory/MemoryService.hpp"
#include "sim/DomainConfinement.hpp"
#include "sim/EulerIntegrator.hpp"
#include "sim/IConstraint.hpp"
#include "sim/MilsteinIntegrator.hpp"
#include <limits>
#include <string>
#include <utility>

namespace ndde {

class ParticleBuilder {
public:
    explicit ParticleBuilder(const ndde::math::ISurface* surface,
                             memory::MemoryService* memory = nullptr)
        : m_surface(surface)
        , m_memory(memory)
        , m_constraints(make_constraint_vector(memory))
    {
        m_stack.bind_memory(memory);
    }
    ~ParticleBuilder() = default;
    ParticleBuilder(const ParticleBuilder&) = delete;
    ParticleBuilder& operator=(const ParticleBuilder&) = delete;
    ParticleBuilder(ParticleBuilder&&) noexcept = default;
    ParticleBuilder& operator=(ParticleBuilder&&) noexcept = default;

    ParticleBuilder& named(std::string label) & {
        m_label = std::move(label);
        return *this;
    }
    ParticleBuilder&& named(std::string label) && {
        named(std::move(label));
        return std::move(*this);
    }

    ParticleBuilder& role(ParticleRole role) & noexcept {
        m_role = role;
        return *this;
    }
    ParticleBuilder&& role(ParticleRole role) && noexcept {
        this->role(role);
        return std::move(*this);
    }

    ParticleBuilder& at(glm::vec2 uv) & noexcept {
        m_uv = uv;
        return *this;
    }
    ParticleBuilder&& at(glm::vec2 uv) && noexcept {
        this->at(uv);
        return std::move(*this);
    }

    ParticleBuilder& trail(TrailConfig cfg) & noexcept {
        m_trail = cfg;
        return *this;
    }
    ParticleBuilder&& trail(TrailConfig cfg) && noexcept {
        this->trail(cfg);
        return std::move(*this);
    }

    ParticleBuilder& history(std::size_t capacity = 4096, f32 dt_min = 1.f / 120.f) & noexcept {
        m_history_capacity = capacity;
        m_history_dt_min = dt_min;
        m_enable_history = true;
        return *this;
    }
    ParticleBuilder&& history(std::size_t capacity = 4096, f32 dt_min = 1.f / 120.f) && noexcept {
        this->history(capacity, dt_min);
        return std::move(*this);
    }

    ParticleBuilder& stochastic(bool enabled = true) & noexcept {
        m_stochastic = enabled;
        return *this;
    }
    ParticleBuilder&& stochastic(bool enabled = true) && noexcept {
        this->stochastic(enabled);
        return std::move(*this);
    }

    ParticleBuilder& slope_velocity_transform(SlopeVelocityTransform transform) & noexcept {
        transform.enabled = true;
        m_stack.set_velocity_transform(transform);
        return *this;
    }
    ParticleBuilder&& slope_velocity_transform(SlopeVelocityTransform transform) && noexcept {
        this->slope_velocity_transform(transform);
        return std::move(*this);
    }

    ParticleBuilder& slope_velocity_transform(f32 intercept,
                                              f32 slope_gain,
                                              f32 min_scale = 0.f,
                                              f32 max_scale = std::numeric_limits<f32>::infinity()) & noexcept {
        return slope_velocity_transform(SlopeVelocityTransform{
            .enabled = true,
            .intercept = intercept,
            .slope_gain = slope_gain,
            .min_scale = min_scale,
            .max_scale = max_scale
        });
    }
    ParticleBuilder&& slope_velocity_transform(f32 intercept,
                                               f32 slope_gain,
                                               f32 min_scale = 0.f,
                                               f32 max_scale = std::numeric_limits<f32>::infinity()) && noexcept {
        this->slope_velocity_transform(intercept, slope_gain, min_scale, max_scale);
        return std::move(*this);
    }

    template <class Behavior, class... Args>
    ParticleBuilder& with_behavior(f32 weight, Args&&... args) & {
        m_stack.add(make_sim_unique_as<IParticleBehavior, Behavior>(std::forward<Args>(args)...), weight);
        return *this;
    }
    template <class Behavior, class... Args>
    ParticleBuilder&& with_behavior(f32 weight, Args&&... args) && {
        this->with_behavior<Behavior>(weight, std::forward<Args>(args)...);
        return std::move(*this);
    }

    template <class Behavior, class... Args>
    ParticleBuilder& with_behavior(Args&&... args) & {
        return with_behavior<Behavior>(1.f, std::forward<Args>(args)...);
    }
    template <class Behavior, class... Args>
    ParticleBuilder&& with_behavior(Args&&... args) && {
        this->with_behavior<Behavior>(1.f, std::forward<Args>(args)...);
        return std::move(*this);
    }

    ParticleBuilder& with_equation(memory::Unique<ndde::sim::IEquation> equation, f32 weight = 1.f) & {
        m_stack.add(make_sim_unique_as<IParticleBehavior, EquationBehavior>(std::move(equation)), weight);
        return *this;
    }
    ParticleBuilder&& with_equation(memory::Unique<ndde::sim::IEquation> equation, f32 weight = 1.f) && {
        this->with_equation(std::move(equation), weight);
        return std::move(*this);
    }

    template <class Equation, class... Args>
    ParticleBuilder& with_equation(f32 weight, Args&&... args) & {
        return with_equation(make_sim_unique_as<ndde::sim::IEquation, Equation>(
            std::forward<Args>(args)...), weight);
    }

    template <class Equation, class... Args>
    ParticleBuilder&& with_equation(f32 weight, Args&&... args) && {
        this->with_equation<Equation>(weight, std::forward<Args>(args)...);
        return std::move(*this);
    }

    template <class Equation, class... Args>
    ParticleBuilder& with_equation_type(Args&&... args) & {
        return with_equation<Equation>(1.f, std::forward<Args>(args)...);
    }

    template <class Equation, class... Args>
    ParticleBuilder&& with_equation_type(Args&&... args) && {
        this->with_equation_type<Equation>(std::forward<Args>(args)...);
        return std::move(*this);
    }

    template <class Constraint, class... Args>
    ParticleBuilder& with_constraint(Args&&... args) & {
        m_constraints.push_back(make_sim_unique_as<ndde::sim::IConstraint, Constraint>(
            std::forward<Args>(args)...));
        return *this;
    }
    template <class Constraint, class... Args>
    ParticleBuilder&& with_constraint(Args&&... args) && {
        this->with_constraint<Constraint>(std::forward<Args>(args)...);
        return std::move(*this);
    }

    [[nodiscard]] AnimatedCurve build(const ndde::sim::IIntegrator* deterministic,
                                      const ndde::sim::IIntegrator* stochastic = nullptr);

private:
    const ndde::math::ISurface* m_surface = nullptr;
    memory::MemoryService* m_memory = nullptr;
    glm::vec2 m_uv{0.f, 0.f};
    ParticleRole m_role = ParticleRole::Neutral;
    std::string m_label;
    TrailConfig m_trail{};
    bool m_enable_history = false;
    std::size_t m_history_capacity = 4096;
    f32 m_history_dt_min = 1.f / 120.f;
    bool m_stochastic = false;
    BehaviorStack m_stack;
    memory::SimVector<memory::Unique<ndde::sim::IConstraint>> m_constraints;

    [[nodiscard]] static memory::SimVector<memory::Unique<ndde::sim::IConstraint>>
    make_constraint_vector(memory::MemoryService* memory) {
        return memory ? memory->simulation().make_vector<memory::Unique<ndde::sim::IConstraint>>()
                      : memory::SimVector<memory::Unique<ndde::sim::IConstraint>>{
                            std::pmr::get_default_resource()
                        };
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
};

class ParticleFactory {
public:
    explicit ParticleFactory(const ndde::math::ISurface* surface,
                             memory::MemoryService* memory = nullptr)
        : m_surface(surface)
        , m_memory(memory)
    {}

    [[nodiscard]] ParticleBuilder particle() const { return ParticleBuilder(m_surface, m_memory); }

private:
    const ndde::math::ISurface* m_surface = nullptr;
    memory::MemoryService* m_memory = nullptr;
};

using Particle = AnimatedCurve;

} // namespace ndde
