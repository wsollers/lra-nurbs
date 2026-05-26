#pragma once
// app/ParticleInspectorPanel.hpp
// Shared ImGui inspector for scene-owned particles.

#include "app/AnimatedCurve.hpp"
#include "app/ParticleBehaviors.hpp"
#include "memory/Containers.hpp"
#include "sim/BrownianMotion.hpp"
#include "sim/LevelCurveWalker.hpp"

#include <imgui.h>
#include <algorithm>
#include <cmath>

namespace ndde {

struct ParticleInspectorOptions {
    const char* label = "Active particles";
    bool show_level_curve_controls = true;
    bool show_brownian_controls = true;
    bool show_trail_controls = true;
};

class ParticleInspectorPanel {
public:
    static void draw(memory::SimVector<AnimatedCurve>& particles,
                     const ParticleInspectorOptions& options = {}) {
        ImGui::SeparatorText(options.label);
        ImGui::TextDisabled("%zu particle(s)", particles.size());
        ImGui::Spacing();

        for (u32 i = 0; i < static_cast<u32>(particles.size()); ++i) {
            AnimatedCurve& particle = particles[i];
            ImGui::PushID(static_cast<int>(i));
            draw_particle(particle, options);
            ImGui::PopID();
            ImGui::Separator();
        }
    }

private:
    static void draw_particle(AnimatedCurve& particle, const ParticleInspectorOptions& options) {
        const Vec3 h = particle.head_world();
        const glm::vec2 uv = particle.head_uv();

        ImGui::TextColored(role_color(particle.particle_role()),
                           "%s", particle.metadata_label().c_str());
        ImGui::TextDisabled("id=%llu  role=%s",
            static_cast<unsigned long long>(particle.id()),
            role_name(particle.particle_role()).data());
        ImGui::TextDisabled("uv=(%.3f, %.3f)  p=(%.3f, %.3f, %.3f)",
                            uv.x, uv.y, h.x, h.y, h.z);
        ImGui::TextDisabled("trail=%u", particle.trail_size());

        if (options.show_trail_controls)
            draw_trail_controls(particle);

        if (options.show_level_curve_controls) {
            if (auto* level = particle.find_equation<ndde::sim::LevelCurveWalker>())
                draw_level_curve_controls(*level, h.z);
        }

        if (options.show_brownian_controls) {
            if (auto* brownian = particle.find_equation<ndde::sim::BrownianMotion>())
                draw_brownian_controls(*brownian);
        }
    }

    static void draw_trail_controls(AnimatedCurve& particle) {
        TrailConfig cfg = particle.trail_config();
        int mode = static_cast<int>(cfg.mode);
        constexpr const char* modes[] = {"None", "Finite", "Persistent", "Static curve"};
        if (ImGui::Combo("Trail##mode", &mode, modes, IM_ARRAYSIZE(modes))) {
            cfg.mode = static_cast<TrailMode>(std::clamp(mode, 0, 3));
            particle.set_trail_config(cfg);
        }

        int max_points = static_cast<int>(cfg.max_points);
        if (cfg.mode == TrailMode::Finite) {
            ImGui::SetNextItemWidth(120.f);
            if (ImGui::SliderInt("Max##trail", &max_points, 16, 12000)) {
                cfg.max_points = static_cast<u32>(std::max(max_points, 16));
                particle.set_trail_config(cfg);
            }
            ImGui::SameLine();
        }

        ImGui::SetNextItemWidth(120.f);
        if (ImGui::SliderFloat("Spacing##trail", &cfg.min_spacing, 0.f, 0.12f, "%.3f"))
            particle.set_trail_config(cfg);
    }

    static void draw_level_curve_controls(ndde::sim::LevelCurveWalker& level, f32 current_z) {
        auto& p = level.params();
        const f32 dz = current_z - p.z0;
        const ImVec4 col = std::abs(dz) < p.epsilon
            ? ImVec4(0.4f, 1.f, 0.4f, 1.f)
            : ImVec4(1.f, 0.5f, 0.2f, 1.f);
        ImGui::TextColored(col, "level z=%.3f  dz=%+.3f", current_z, dz);

        ImGui::SetNextItemWidth(120.f);
        ImGui::SliderFloat("z0##lw", &p.z0, -2.f, 2.f, "%.3f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.f);
        ImGui::SliderFloat("eps##lw", &p.epsilon, 0.02f, 1.f, "%.3f");
        ImGui::SetNextItemWidth(120.f);
        ImGui::SliderFloat("speed##lw", &p.walk_speed, 0.1f, 2.f, "%.2f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.f);
        ImGui::SliderFloat("floor##lw", &p.tangent_floor, 0.f, 1.f, "%.2f");
    }

    static void draw_brownian_controls(ndde::sim::BrownianMotion& brownian) {
        auto& p = brownian.params();
        ImGui::SetNextItemWidth(120.f);
        ImGui::SliderFloat("sigma##bm", &p.sigma, 0.01f, 2.f, "%.3f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.f);
        ImGui::SliderFloat("drift##bm", &p.drift_strength, -1.f, 1.f, "%.3f");
    }

    static ImVec4 role_color(ParticleRole role) noexcept {
        switch (role) {
            case ParticleRole::Leader: return {0.40f, 0.70f, 1.f, 1.f};
            case ParticleRole::Chaser: return {1.f, 0.55f, 0.35f, 1.f};
            case ParticleRole::Avoider: return {0.78f, 0.88f, 1.f, 1.f};
            case ParticleRole::Neutral:
            default: return {0.78f, 0.82f, 0.88f, 1.f};
        }
    }
};

} // namespace ndde
