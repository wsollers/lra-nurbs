#pragma once
// sim/LevelCurveWalker.hpp
// LevelCurveWalker: IEquation that traces a level curve of a height field
// within an ε-band around a target height z₀.
//
// Geometric Intuition
// ───────────────────
// On a height-field surface z = f(x,y), the gradient ∇f points in the
// direction of steepest ascent. The perpendicular direction ∇⊥f = (-∂f/∂y, ∂f/∂x)
// is exactly the tangent to the level curve f(x,y) = c.
//
// A particle following ∇⊥f / |∇f| would trace a perfect level curve —
// but numerical errors and discrete time steps let it drift. To maintain
// confinement, we add a corrective term:
//
//   If z > z₀ + ε : steer mostly downhill  (−∇f direction)
//   If z < z₀ − ε : steer mostly uphill    (+∇f direction)
//   Otherwise      : mostly level-curve tangent
//
// This is a proportional controller on the height error:
//
//   e = (z − z₀) / ε               ∈ [−1, 1] in band, outside otherwise
//   correction = -clamp(e, −1, 1) · ∇f / |∇f|
//
// The total velocity direction is:
//
//   d = max(tangent_floor, 1−|clamp(e)|) · ∇⊥f/|∇f|
//       − clamp(e) · ∇f/|∇f|
//
// normalised and scaled by walk_speed.
// Keeping a tangential floor prevents the walker from becoming a pure
// gradient-descent/ascent particle, which can otherwise trap it in a basin
// around local extrema.
//
// When |∇f| ≈ 0 (at a critical point), the particle wanders in a fixed
// direction (the last valid heading) rather than stopping — this prevents
// degenerate behaviour at local extrema.
//
// Parameters
// ──────────
//   z0         : target height (set at construction, or via set_z0())
//   epsilon    : half-width of the allowed ε-band around z0
//   walk_speed : forward speed (parameter-units / second)
//   turn_rate  : max heading change per integrator call (radians)
//               smooths direction reversals at gradient discontinuities

#include "sim/IEquation.hpp"
#include "numeric/ops.hpp"
#include <algorithm>
#include <string>

namespace ndde::sim {

class LevelCurveWalker final : public IEquation {
public:
    struct Params {
        float z0         = 0.f;    ///< target height
        float epsilon    = 0.15f;  ///< confinement half-band width
        float walk_speed = 0.7f;   ///< forward speed (param-units/s)
        float turn_rate  = 2.5f;   ///< max heading change per step (radians)
        float tangent_floor = 0.35f; ///< minimum level-curve component
    };

    LevelCurveWalker() = default;
    explicit LevelCurveWalker(Params p) : m_p(p) {}

    // ── IEquation::update ──────────────────────────────────────────────────────
    // Returns (du/dt, dv/dt) in parameter space.
    // Mutates state.angle (rate-limited heading) — same pattern as GradientWalker.

    [[nodiscard]] glm::vec2 update(
        ParticleState&              state,
        const ndde::math::ISurface& surface,
        float                       /*t*/) const override
    {
        const Vec3 du_v = surface.du(state.uv.x, state.uv.y);
        const Vec3 dv_v = surface.dv(state.uv.x, state.uv.y);

        // Gradient components (z-components of tangent vectors for height field)
        const float gx = du_v.z;   // ∂f/∂u
        const float gy = dv_v.z;   // ∂f/∂v
        const float gn = ops::sqrt(gx*gx + gy*gy);

        // Level-curve tangent direction: ∇⊥f = (−gy, gx) / |∇f|
        // Gradient direction: ∇f = (gx, gy) / |∇f|
        float tx, ty;   // desired tangent direction
        if (gn > 1e-5f) {
            // Current height and height error
            const float z   = surface.evaluate(state.uv.x, state.uv.y).z;
            const float err = (z - m_p.z0) / std::max(m_p.epsilon, 1e-6f);
            const float ce  = ops::clamp(err, -1.f, 1.f);
            const float te  = std::max(m_p.tangent_floor, 1.f - ops::abs(ce));

            // Blend tangent and corrective gradient components
            const float raw_x = te * (-gy / gn) - ce * (gx / gn);
            const float raw_y = te * ( gx / gn) - ce * (gy / gn);
            const float rn    = ops::sqrt(raw_x*raw_x + raw_y*raw_y);
            tx = (rn > 1e-6f) ? raw_x / rn : -gy / gn;
            ty = (rn > 1e-6f) ? raw_y / rn :  gx / gn;
        } else {
            // Critical point — hold last heading
            tx = ops::cos(state.angle);
            ty = ops::sin(state.angle);
        }

        // Rate-limited heading update (same approach as GradientWalker)
        const float desired = ops::atan2(ty, tx);
        float da = desired - state.angle;
        while (da >  ops::pi_v<float>) da -= ops::two_pi_v<float>;
        while (da < -ops::pi_v<float>) da += ops::two_pi_v<float>;
        state.angle += ops::clamp(da, -m_p.turn_rate, m_p.turn_rate);

        return { m_p.walk_speed * ops::cos(state.angle),
                 m_p.walk_speed * ops::sin(state.angle) };
    }

    [[nodiscard]] float phase_rate()  const override { return 0.f; }
    [[nodiscard]] std::string name()  const override { return "LevelCurveWalker"; }

    Params&       params()       noexcept { return m_p; }
    const Params& params() const noexcept { return m_p; }

    void set_z0(float z0) noexcept { m_p.z0 = z0; }

private:
    Params m_p;
};

} // namespace ndde::sim
