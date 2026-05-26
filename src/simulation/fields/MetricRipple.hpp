#pragma once
// simulation/fields/MetricRipple.hpp
// Conformal metric perturbation triggered by a double-click poke.
//
// g(x,t) = (1 + ε·h(x,t; p, t0)) · g0
//
// h(x,t) = A · sin(ω(t−t0) − k·d0(x,p))
//             · exp(−α·d0(x,p))
//             · exp(−β(t−t0))
//             · 1{t ≥ t0}
//
// where d0(x,p) is the flat-metric distance from x to the poke point p.
//
// Well-posedness requires ε·A < 1 (metric stays positive-definite).
// active() returns false when the temporal factor falls below DECAY_THRESHOLD,
// at which point FieldCompositor removes the ripple and fires FieldRemoved.

#include "simulation/fields/IField.hpp"
#include "simulation/events/SimEventTypes.hpp"
#include "numeric/ops.hpp"
#include <string>

namespace ndde::simulation {

class MetricRipple final : public IField {
public:
    struct Params {
        f32 u         = f32(0);    // poke point u
        f32 v         = f32(0);    // poke point v
        f32 t0        = f32(0);    // fire time
        f32 amplitude = f32(0.15); // A
        f32 omega     = f32(6.28); // ω — wavefront angular frequency
        f32 k_wave    = f32(8.0);  // k — wave number
        f32 alpha     = f32(2.5);  // spatial decay
        f32 beta      = f32(0.8);  // temporal decay
        f32 epsilon   = f32(0.15); // conformal scale (ε·A < 1 required)
        f32 geometry_scale = f32(2); // visible height scale for rendered mesh
        u32 seed      = u32(0);
    };

    explicit MetricRipple(const Params& p) : m_p(p) {
        // Build a stable name for the compositor remove() call
        m_name = "MetricRipple@(" + std::to_string(p.u).substr(0, 5)
               + "," + std::to_string(p.v).substr(0, 5) + ")";
    }

    // Construct from a PerturbationFired event
    static MetricRipple from_event(
            const ndde::simulation::events::PerturbationFired& e,
            f32 epsilon = f32(0.15)) {
        Params p;
        p.u = e.u; p.v = e.v; p.t0 = e.sim_time;
        p.amplitude = e.amplitude; p.omega = e.omega;
        p.k_wave = e.k_wave; p.alpha = e.alpha;
        p.beta = e.beta; p.epsilon = epsilon; p.seed = e.seed;
        p.geometry_scale = f32(2);
        return MetricRipple{p};
    }

    [[nodiscard]] f32 metric_factor(f32 u, f32 v, f32 t) const override {
        return f32(1) + m_p.epsilon * wave_height(u, v, t);
    }

    [[nodiscard]] f32 surface_displacement(f32 u, f32 v, f32 t) const override {
        return m_p.geometry_scale * wave_height(u, v, t);
    }

    [[nodiscard]] f32 wave_height(f32 u, f32 v, f32 t) const {
        if (t < m_p.t0) return f32(0);
        const f32 dt  = t - m_p.t0;
        const f32 du  = u - m_p.u;
        const f32 dv  = v - m_p.v;
        const f32 d0  = ops::sqrt(du*du + dv*dv);
        return m_p.amplitude
             * ops::sin(m_p.omega * dt - m_p.k_wave * d0)
             * ops::exp(-m_p.alpha * d0)
             * ops::exp(-m_p.beta  * dt);
    }

    [[nodiscard]] bool active(f32 t) const override {
        if (t < m_p.t0) return true;
        return ops::exp(-m_p.beta * (t - m_p.t0)) > DECAY_THRESHOLD;
    }

    [[nodiscard]] std::string_view name() const noexcept override { return m_name; }

    [[nodiscard]] const Params& params() const noexcept { return m_p; }

private:
    static constexpr f32 DECAY_THRESHOLD = f32(0.001);
    Params      m_p;
    std::string m_name;
};

} // namespace ndde::simulation
