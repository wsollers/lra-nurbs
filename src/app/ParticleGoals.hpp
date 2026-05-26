#pragma once
// app/ParticleGoals.hpp
// Scene-level goals / win conditions over composable particles.

#include "app/SimulationContext.hpp"
#include "app/AnimatedCurve.hpp"
#include "numeric/ops.hpp"
#include <string>

namespace ndde {

enum class GoalStatus : u8 {
    Running,
    Succeeded,
    Failed
};

class IParticleGoal {
public:
    virtual ~IParticleGoal() = default;

    [[nodiscard]] virtual GoalStatus evaluate(const SimulationContext& context) = 0;
    [[nodiscard]] virtual std::string metadata_label() const = 0;

protected:
    IParticleGoal() = default;
    IParticleGoal(const IParticleGoal&) = default;
    IParticleGoal& operator=(const IParticleGoal&) = default;
    IParticleGoal(IParticleGoal&&) = default;
    IParticleGoal& operator=(IParticleGoal&&) = default;
};

class CaptureGoal final : public IParticleGoal {
public:
    struct Params {
        ParticleRole seeker_role = ParticleRole::Chaser;
        ParticleRole target_role = ParticleRole::Leader;
        f32 radius = 0.25f;
    };

    CaptureGoal() = default;
    explicit CaptureGoal(Params p) : m_p(p) {}

    [[nodiscard]] GoalStatus evaluate(const SimulationContext& context) override {
        for (const auto& seeker : context.particles()) {
            if (seeker.particle_role() != m_p.seeker_role) continue;
            const AnimatedCurve* target = context.nearest(m_p.target_role, seeker.head_uv(), seeker.id());
            if (!target) continue;
            const Vec3 a = context.surface().evaluate(seeker.head_uv().x, seeker.head_uv().y);
            const Vec3 b = context.surface().evaluate(target->head_uv().x, target->head_uv().y);
            if (ops::length(a - b) <= m_p.radius)
                return GoalStatus::Succeeded;
        }
        return GoalStatus::Running;
    }

    [[nodiscard]] std::string metadata_label() const override { return "Capture"; }

private:
    Params m_p;
};

class SurvivalGoal final : public IParticleGoal {
public:
    explicit SurvivalGoal(f32 duration_seconds) : m_duration(duration_seconds) {}

    [[nodiscard]] GoalStatus evaluate(const SimulationContext& context) override {
        return context.time() >= m_duration ? GoalStatus::Succeeded : GoalStatus::Running;
    }

    [[nodiscard]] std::string metadata_label() const override { return "Survival"; }

private:
    f32 m_duration = 0.f;
};

} // namespace ndde
