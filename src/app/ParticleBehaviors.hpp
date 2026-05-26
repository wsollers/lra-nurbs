#pragma once
// app/ParticleBehaviors.hpp
// Composable behavior stack for particle dynamics.

#include "app/SimulationContext.hpp"
#include "memory/Containers.hpp"
#include "memory/MemoryService.hpp"
#include "memory/Unique.hpp"
#include "simulation/fields/IField.hpp"
#include "sim/IEquation.hpp"
#include "numeric/ops.hpp"
#include <algorithm>
#include <limits>
#include <string>

namespace ndde {

struct TargetSelector {
    enum class Kind : u8 { ById, FirstRole, NearestRole, CentroidRole };

    Kind kind = Kind::FirstRole;
    ParticleId id = 0;
    ParticleRole role = ParticleRole::Leader;

    [[nodiscard]] static TargetSelector by_id(ParticleId id) noexcept {
        TargetSelector s; s.kind = Kind::ById; s.id = id; return s;
    }
    [[nodiscard]] static TargetSelector first(ParticleRole role) noexcept {
        TargetSelector s; s.kind = Kind::FirstRole; s.role = role; return s;
    }
    [[nodiscard]] static TargetSelector nearest(ParticleRole role) noexcept {
        TargetSelector s; s.kind = Kind::NearestRole; s.role = role; return s;
    }
    [[nodiscard]] static TargetSelector centroid(ParticleRole role) noexcept {
        TargetSelector s; s.kind = Kind::CentroidRole; s.role = role; return s;
    }
};

class IParticleBehavior {
public:
    virtual ~IParticleBehavior() = default;

    [[nodiscard]] virtual glm::vec2 velocity(
        ndde::sim::ParticleState&    state,
        const ndde::math::ISurface&  surface,
        f32                        t,
        const SimulationContext&     context,
        ParticleId                   owner) const = 0;

    [[nodiscard]] virtual glm::vec2 noise_coefficient(
        const ndde::sim::ParticleState& /*state*/,
        const ndde::math::ISurface&     /*surface*/,
        f32                           /*t*/,
        const SimulationContext&        /*context*/,
        ParticleId                      /*owner*/) const
    {
        return {0.f, 0.f};
    }

    [[nodiscard]] virtual f32 phase_rate() const { return 0.f; }
    [[nodiscard]] virtual std::string metadata_label() const = 0;
    [[nodiscard]] virtual f32 delay_seconds() const noexcept { return 0.f; }
    [[nodiscard]] virtual f32 nominal_speed() const noexcept { return 0.f; }

protected:
    IParticleBehavior() = default;
    IParticleBehavior(const IParticleBehavior&) = default;
    IParticleBehavior& operator=(const IParticleBehavior&) = default;
    IParticleBehavior(IParticleBehavior&&) = default;
    IParticleBehavior& operator=(IParticleBehavior&&) = default;
};

class EquationBehavior final : public IParticleBehavior {
public:
    explicit EquationBehavior(memory::Unique<ndde::sim::IEquation> equation)
        : m_equation(std::move(equation))
    {}

    [[nodiscard]] glm::vec2 velocity(ndde::sim::ParticleState& state,
                                     const ndde::math::ISurface& surface,
                                     f32 t,
                                     const SimulationContext&,
                                     ParticleId) const override {
        return m_equation ? m_equation->update(state, surface, t) : glm::vec2{0.f, 0.f};
    }

    [[nodiscard]] glm::vec2 noise_coefficient(const ndde::sim::ParticleState& state,
                                              const ndde::math::ISurface& surface,
                                              f32 t,
                                              const SimulationContext&,
                                              ParticleId) const override {
        return m_equation ? m_equation->noise_coefficient(state, surface, t) : glm::vec2{0.f, 0.f};
    }

    [[nodiscard]] f32 phase_rate() const override {
        return m_equation ? m_equation->phase_rate() : 0.f;
    }

    [[nodiscard]] std::string metadata_label() const override {
        return m_equation ? m_equation->name() : "Equation";
    }

    [[nodiscard]] ndde::sim::IEquation* equation() noexcept { return m_equation.get(); }
    [[nodiscard]] const ndde::sim::IEquation* equation() const noexcept { return m_equation.get(); }

private:
    memory::Unique<ndde::sim::IEquation> m_equation;
};

class BrownianBehavior final : public IParticleBehavior {
public:
    struct Params {
        f32 sigma = 0.4f;
        f32 drift_strength = 0.f;
    };

    BrownianBehavior() = default;
    explicit BrownianBehavior(Params p) : m_p(p) {}

    [[nodiscard]] glm::vec2 velocity(ndde::sim::ParticleState& state,
                                     const ndde::math::ISurface& surface,
                                     f32,
                                     const SimulationContext&,
                                     ParticleId) const override {
        if (ops::abs(m_p.drift_strength) < 1e-7f) return {0.f, 0.f};
        const glm::vec3 du = surface.du(state.uv.x, state.uv.y);
        const glm::vec3 dv = surface.dv(state.uv.x, state.uv.y);
        const f32 gn = ops::sqrt(du.z * du.z + dv.z * dv.z) + 1e-7f;
        return {m_p.drift_strength * du.z / gn, m_p.drift_strength * dv.z / gn};
    }

    [[nodiscard]] glm::vec2 noise_coefficient(const ndde::sim::ParticleState&,
                                              const ndde::math::ISurface&,
                                              f32,
                                              const SimulationContext&,
                                              ParticleId) const override {
        return {m_p.sigma, m_p.sigma};
    }

    [[nodiscard]] std::string metadata_label() const override {
        return ops::abs(m_p.drift_strength) > 1e-7f ? "Brownian + Bias Drift" : "Brownian";
    }

private:
    Params m_p;
};

class ConstantDriftBehavior final : public IParticleBehavior {
public:
    explicit ConstantDriftBehavior(glm::vec2 velocity) : m_velocity(velocity) {}

    [[nodiscard]] glm::vec2 velocity(ndde::sim::ParticleState&,
                                     const ndde::math::ISurface&,
                                     f32,
                                     const SimulationContext&,
                                     ParticleId) const override {
        return m_velocity;
    }

    [[nodiscard]] std::string metadata_label() const override {
        return "Constant Drift";
    }

private:
    glm::vec2 m_velocity{0.f, 0.f};
};

class SeekParticleBehavior final : public IParticleBehavior {
public:
    struct Params {
        TargetSelector target = TargetSelector::first(ParticleRole::Leader);
        f32 speed = 0.8f;
        f32 delay_seconds = 0.f;
    };

    SeekParticleBehavior() = default;
    explicit SeekParticleBehavior(Params p) : m_p(p) {}

    [[nodiscard]] glm::vec2 velocity(ndde::sim::ParticleState& state,
                                     const ndde::math::ISurface& surface,
                                     f32 t,
                                     const SimulationContext& context,
                                     ParticleId owner) const override {
        return direction_to_target(state.uv, surface, t, context, owner) * m_p.speed;
    }

    [[nodiscard]] std::string metadata_label() const override {
        return m_p.delay_seconds > 0.f ? "Delayed Seek" : "Seek";
    }
    [[nodiscard]] f32 delay_seconds() const noexcept override { return m_p.delay_seconds; }
    [[nodiscard]] f32 nominal_speed() const noexcept override { return m_p.speed; }

private:
    Params m_p;

    [[nodiscard]] glm::vec2 direction_to_target(glm::vec2 from,
                                                const ndde::math::ISurface& surface,
                                                f32 t,
                                                const SimulationContext& context,
                                                ParticleId owner) const;
};

class AvoidParticleBehavior final : public IParticleBehavior {
public:
    struct Params {
        TargetSelector target = TargetSelector::nearest(ParticleRole::Chaser);
        f32 speed = 0.8f;
        f32 delay_seconds = 0.f;
    };

    AvoidParticleBehavior() = default;
    explicit AvoidParticleBehavior(Params p) : m_p(p) {}

    [[nodiscard]] glm::vec2 velocity(ndde::sim::ParticleState& state,
                                     const ndde::math::ISurface& surface,
                                     f32 t,
                                     const SimulationContext& context,
                                     ParticleId owner) const override;

    [[nodiscard]] std::string metadata_label() const override {
        return m_p.delay_seconds > 0.f ? "Delayed Avoid" : "Avoid";
    }
    [[nodiscard]] f32 delay_seconds() const noexcept override { return m_p.delay_seconds; }
    [[nodiscard]] f32 nominal_speed() const noexcept override { return m_p.speed; }

private:
    SeekParticleBehavior::Params seek_params() const {
        return {.target = m_p.target, .speed = 1.f, .delay_seconds = m_p.delay_seconds};
    }
    Params m_p;
};

class CentroidSeekBehavior final : public IParticleBehavior {
public:
    struct Params {
        ParticleRole role = ParticleRole::Chaser;
        f32 speed = 0.8f;
    };

    CentroidSeekBehavior() = default;
    explicit CentroidSeekBehavior(Params p) : m_p(p) {}

    [[nodiscard]] glm::vec2 velocity(ndde::sim::ParticleState& state,
                                     const ndde::math::ISurface& surface,
                                     f32,
                                     const SimulationContext& context,
                                     ParticleId owner) const override;

    [[nodiscard]] std::string metadata_label() const override {
        return "Centroid Seek";
    }

private:
    Params m_p;
};

class GradientDriftBehavior final : public IParticleBehavior {
public:
    enum class Mode : u8 {
        Uphill,
        Downhill,
        LevelTangent
    };

    struct Params {
        Mode mode = Mode::Uphill;
        f32 speed = 0.6f;
    };

    GradientDriftBehavior() = default;
    explicit GradientDriftBehavior(Params p) : m_p(p) {}

    [[nodiscard]] glm::vec2 velocity(ndde::sim::ParticleState& state,
                                     const ndde::math::ISurface& surface,
                                     f32,
                                     const SimulationContext&,
                                     ParticleId) const override;

    [[nodiscard]] std::string metadata_label() const override;

private:
    Params m_p;
};

class OrbitBehavior final : public IParticleBehavior {
public:
    struct Params {
        glm::vec2 center{0.f, 0.f};
        f32 radius = 1.f;
        f32 angular_speed = 0.7f;
        f32 radial_strength = 0.45f;
        bool clockwise = false;
    };

    OrbitBehavior() = default;
    explicit OrbitBehavior(Params p) : m_p(p) {}

    [[nodiscard]] glm::vec2 velocity(ndde::sim::ParticleState& state,
                                     const ndde::math::ISurface& surface,
                                     f32,
                                     const SimulationContext&,
                                     ParticleId) const override;

    [[nodiscard]] std::string metadata_label() const override { return "Orbit"; }

private:
    Params m_p;
};

class FlockingBehavior final : public IParticleBehavior {
public:
    struct Params {
        ParticleRole role = ParticleRole::Neutral;
        f32 neighbor_radius = 1.2f;
        f32 separation_radius = 0.35f;
        f32 separation_weight = 1.0f;
        f32 cohesion_weight = 0.45f;
        f32 alignment_weight = 0.35f;
        f32 speed = 0.65f;
    };

    FlockingBehavior() = default;
    explicit FlockingBehavior(Params p) : m_p(p) {}

    [[nodiscard]] glm::vec2 velocity(ndde::sim::ParticleState& state,
                                     const ndde::math::ISurface& surface,
                                     f32,
                                     const SimulationContext& context,
                                     ParticleId owner) const override;

    [[nodiscard]] std::string metadata_label() const override { return "Flocking"; }

private:
    Params m_p;
};

struct SlopeVelocityTransform {
    bool enabled = false;
    f32 intercept = 1.f;
    f32 slope_gain = 0.f;
    f32 min_scale = 0.f;
    f32 max_scale = std::numeric_limits<f32>::infinity();

    [[nodiscard]] static SlopeVelocityTransform identity() noexcept { return {}; }
};

class BehaviorStack final : public ndde::sim::IEquation {
public:
    BehaviorStack() = default;
    ~BehaviorStack() override = default;
    BehaviorStack(const BehaviorStack&) = delete;
    BehaviorStack& operator=(const BehaviorStack&) = delete;
    BehaviorStack(BehaviorStack&&) noexcept = default;
    BehaviorStack& operator=(BehaviorStack&&) noexcept = default;

    void bind_memory(memory::MemoryService* memory) {
        if (memory) {
            memory->simulation().rebind_vector(m_behaviors);
            return;
        }

        std::pmr::memory_resource* resource = std::pmr::get_default_resource();
        if (resource == m_behaviors.get_allocator().resource())
            return;

        memory::SimVector<Entry> rebound{resource};
        rebound.reserve(m_behaviors.size());
        for (auto& entry : m_behaviors)
            rebound.push_back(std::move(entry));
        std::destroy_at(&m_behaviors);
        std::construct_at(&m_behaviors, std::move(rebound));
    }

    void set_context(const SimulationContext* context) noexcept { m_context = context; }
    void set_owner(ParticleId owner) noexcept { m_owner = owner; }

    void add(memory::Unique<IParticleBehavior> behavior, f32 weight = 1.f) {
        m_behaviors.push_back({std::move(behavior), weight});
    }

    void set_velocity_transform(SlopeVelocityTransform transform) noexcept {
        m_velocity_transform = transform;
    }

    [[nodiscard]] glm::vec2 update(ndde::sim::ParticleState& state,
                                   const ndde::math::ISurface& surface,
                                   f32 t) const override {
        if (!m_context) return {0.f, 0.f};
        glm::vec2 sum{0.f, 0.f};
        for (const auto& entry : m_behaviors)
            sum += entry.weight * entry.behavior->velocity(state, surface, t, *m_context, m_owner);
        if (const auto* fields = m_context->fields())
            sum += fields->total_drift(state, surface, t);
        return apply_velocity_transform(sum, state, surface);
    }

    [[nodiscard]] glm::vec2 noise_coefficient(const ndde::sim::ParticleState& state,
                                              const ndde::math::ISurface& surface,
                                              f32 t) const override {
        if (!m_context) return {0.f, 0.f};
        glm::vec2 sum{0.f, 0.f};
        for (const auto& entry : m_behaviors)
            sum += entry.weight * entry.behavior->noise_coefficient(state, surface, t, *m_context, m_owner);
        if (const auto* fields = m_context->fields())
            sum *= fields->diffusion_factor(state, surface, t);
        return sum;
    }

    [[nodiscard]] f32 phase_rate() const override {
        f32 rate = 0.f;
        for (const auto& entry : m_behaviors)
            rate += entry.weight * entry.behavior->phase_rate();
        return rate;
    }

    [[nodiscard]] f32 max_delay_seconds() const noexcept {
        f32 delay = 0.f;
        for (const auto& entry : m_behaviors)
            delay = std::max(delay, entry.behavior->delay_seconds());
        return delay;
    }

    [[nodiscard]] f32 max_nominal_speed() const noexcept {
        f32 speed = 0.f;
        for (const auto& entry : m_behaviors)
            speed = std::max(speed, entry.behavior->nominal_speed());
        return speed;
    }

    [[nodiscard]] std::string name() const override;
    [[nodiscard]] memory::FrameVector<std::string> behavior_labels() const;

    template <class Equation>
    [[nodiscard]] Equation* find_equation() noexcept {
        for (const auto& entry : m_behaviors) {
            auto* wrapper = dynamic_cast<EquationBehavior*>(entry.behavior.get());
            if (!wrapper) continue;
            if (auto* eq = dynamic_cast<Equation*>(wrapper->equation()))
                return eq;
        }
        return nullptr;
    }

    template <class Equation>
    [[nodiscard]] const Equation* find_equation() const noexcept {
        for (const auto& entry : m_behaviors) {
            const auto* wrapper = dynamic_cast<const EquationBehavior*>(entry.behavior.get());
            if (!wrapper) continue;
            if (const auto* eq = dynamic_cast<const Equation*>(wrapper->equation()))
                return eq;
        }
        return nullptr;
    }

private:
    struct Entry {
        memory::Unique<IParticleBehavior> behavior;
        f32 weight = 1.f;
    };

    memory::SimVector<Entry> m_behaviors;
    const SimulationContext* m_context = nullptr;
    ParticleId m_owner = 0;
    SlopeVelocityTransform m_velocity_transform{};

    [[nodiscard]] glm::vec2 apply_velocity_transform(glm::vec2 velocity,
                                                     const ndde::sim::ParticleState& state,
                                                     const ndde::math::ISurface& surface) const noexcept {
        if (!m_velocity_transform.enabled) return velocity;
        const f32 speed = ops::length(velocity);
        if (speed < 1e-7f) return velocity;

        const glm::vec2 dir = velocity / speed;
        const glm::vec3 du = surface.du(state.uv.x, state.uv.y);
        const glm::vec3 dv = surface.dv(state.uv.x, state.uv.y);
        const f32 directional_derivative = du.z * dir.x + dv.z * dir.y;
        const f32 scale = ops::clamp(
            m_velocity_transform.intercept + m_velocity_transform.slope_gain * directional_derivative,
            m_velocity_transform.min_scale,
            m_velocity_transform.max_scale);
        return velocity * scale;
    }
};

} // namespace ndde
