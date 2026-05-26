#pragma once
// sim/MilsteinIntegrator.hpp
// Milstein scheme for Itô SDEs with scalar diffusion.
//
// Mathematical derivation
// ───────────────────────
// The Euler-Maruyama scheme for dX = mu*dt + sigma*dW is:
//
//   X_{n+1} = X_n + mu(X_n)*dt + sigma(X_n)*dW_n
//
// where dW_n ~ N(0, dt) is a Gaussian increment.
//
// The Milstein correction adds a term that captures the curvature of sigma:
//
//   X_{n+1} = X_n + mu(X_n)*dt + sigma(X_n)*dW_n
//            + (1/2) * sigma(X_n) * (d sigma/dX)(X_n) * (dW_n^2 - dt)
//
// The correction term (1/2)*sigma*sigma'*(dW^2 - dt) is O(dt) in expectation
// (since E[dW^2] = dt) and O(dt^(3/2)) in strong error -- one order better
// than Euler-Maruyama (which has strong order 1/2).
//
// For CONSTANT sigma (like our BrownianMotion with fixed sigma):
//   d sigma/dX = 0  =>  the correction term vanishes.
//   Milstein = Euler-Maruyama exactly.
//
// This matters when sigma depends on state (e.g. sigma = sigma_0 / sqrt(K(u,v))
// for curvature-adaptive diffusion).  The Milstein correction then accounts
// for how the diffusion strength changes as the particle moves.
//
// Implementation
// ──────────────
// We use a SCALAR Milstein scheme (diagonal noise, independent per axis):
//   dX_u = mu_u*dt + sigma_u*dW_u + (1/2)*sigma_u*(d sigma_u/du)*(dW_u^2 - dt)
//   dX_v = mu_v*dt + sigma_v*dW_v + (1/2)*sigma_v*(d sigma_v/dv)*(dW_v^2 - dt)
//
// The partial derivative d sigma/du is estimated by central finite difference:
//   d sigma_u/du ~= (sigma_u(X + h*e_u) - sigma_u(X - h*e_u)) / (2h)
//
// Random number generation
// ────────────────────────
// Each step generates two independent N(0,1) samples via Box-Muller transform
// and scales by sqrt(dt).  The RNG state is held inside the integrator
// (thread_local, so parallel particles don't share state).
//
// Box-Muller: given U1, U2 ~ Uniform(0,1):
//   Z1 = sqrt(-2 ln U1) * cos(2*pi*U2)   ~ N(0,1)
//   Z2 = sqrt(-2 ln U1) * sin(2*pi*U2)   ~ N(0,1)

#include "sim/IIntegrator.hpp"
#include "numeric/ops.hpp"
#include <glm/glm.hpp>
#include <random>

namespace ndde::sim {

class MilsteinIntegrator final : public IIntegrator {
public:
    // fd_h: finite-difference step for estimating d sigma/d(u,v).
    // Smaller h = more accurate but more noise-sensitive.
    explicit MilsteinIntegrator(float fd_h = 1e-3f) : m_fd_h(fd_h) {}

    // Set the seed used by all MilsteinIntegrator instances in this process.
    // Call before spawning any Brownian particles for a reproducible run.
    // Default: 0 = use std::random_device (non-reproducible).
    static void set_global_seed(uint64_t seed) noexcept {
        s_global_seed = seed;
        s_seed_set    = (seed != 0);
        ++s_seed_generation;
    }
    [[nodiscard]] static uint64_t global_seed()    noexcept { return s_global_seed; }
    [[nodiscard]] static bool     seed_is_fixed()  noexcept { return s_seed_set; }

    void step(ParticleState&              state,
              IEquation&                  equation,
              const ndde::math::ISurface& surface,
              float                       t,
              float                       dt) const override
    {
        // ── Drift ─────────────────────────────────────────────────────────────
        const glm::vec2 mu    = equation.update(state, surface, t);

        // ── Diffusion at current position ─────────────────────────────────────
        const glm::vec2 sigma = equation.noise_coefficient(state, surface, t);

        // ── Wiener increment: dW ~ N(0, dt) ──────────────────────────────────
        const float sqrt_dt = ops::sqrt(dt);
        const glm::vec2 dW  = sqrt_dt * normal2();

        // ── Milstein correction: (1/2)*sigma*(d sigma/dX)*(dW^2 - dt) ────────
        // For constant sigma this is zero; it matters for state-dependent sigma.
        const glm::vec2 dsigma = equation.has_constant_noise()
            ? glm::vec2{0.f, 0.f}
            : sigma_gradient(state, equation, surface, t);
        const glm::vec2 milstein = 0.5f * sigma * dsigma * (dW*dW - glm::vec2(dt));

        // ── Euler-Maruyama + Milstein correction ──────────────────────────────
        state.uv += mu * dt + sigma * dW + milstein;

        // Phase: purely deterministic (no stochastic phase accumulation).
        state.phase += equation.phase_rate() * ops::length(mu) * dt;
    }

    [[nodiscard]] std::string name() const override { return "MilsteinIntegrator"; }

private:
    float m_fd_h;

    // Box-Muller transform: generate two independent N(0,1) samples.
    // thread_local RNG: uses s_global_seed when fixed, else hardware entropy.
    [[nodiscard]] static glm::vec2 normal2() {
        const auto make_rng = []() {
            if (s_seed_set) {
                std::seed_seq seq{
                    static_cast<uint32_t>(s_global_seed >> 32u),
                    static_cast<uint32_t>(s_global_seed & 0xFFFF'FFFFull)
                };
                return std::mt19937{seq};
            }
            std::random_device rd;
            std::seed_seq seq{rd(), rd(), rd(), rd()};
            return std::mt19937{seq};
        };
        thread_local uint64_t observed_generation = 0;
        thread_local std::mt19937 rng{make_rng()};
        if (observed_generation != s_seed_generation) {
            rng = make_rng();
            observed_generation = s_seed_generation;
        }
        thread_local std::uniform_real_distribution<float> uniform(1e-7f, 1.f);

        const float u1 = uniform(rng);
        const float u2 = uniform(rng);
        const float r  = ops::sqrt(-2.f * ops::log(u1));
        const float th = ops::two_pi_v<float> * u2;
        return { r * ops::cos(th), r * ops::sin(th) };
    }

    inline static uint64_t s_global_seed = 0;
    inline static bool     s_seed_set    = false;
    inline static uint64_t s_seed_generation = 1;

    // Central finite-difference estimate of d sigma/d(u,v) at current position.
    // Uses the diagonal components: d(sigma_u)/du and d(sigma_v)/dv.
    [[nodiscard]] glm::vec2 sigma_gradient(
        const ParticleState&        state,
        IEquation&                  equation,
        const ndde::math::ISurface& surface,
        float                       t) const
    {
        const float h = m_fd_h;

        // Perturb u
        ParticleState pu_fwd = state;  pu_fwd.uv.x += h;
        ParticleState pu_bwd = state;  pu_bwd.uv.x -= h;
        const float sig_u_fwd = equation.noise_coefficient(pu_fwd, surface, t).x;
        const float sig_u_bwd = equation.noise_coefficient(pu_bwd, surface, t).x;
        const float dsig_du   = (sig_u_fwd - sig_u_bwd) / (2.f * h);

        // Perturb v
        ParticleState pv_fwd = state;  pv_fwd.uv.y += h;
        ParticleState pv_bwd = state;  pv_bwd.uv.y -= h;
        const float sig_v_fwd = equation.noise_coefficient(pv_fwd, surface, t).y;
        const float sig_v_bwd = equation.noise_coefficient(pv_bwd, surface, t).y;
        const float dsig_dv   = (sig_v_fwd - sig_v_bwd) / (2.f * h);

        return { dsig_du, dsig_dv };
    }
};

} // namespace ndde::sim
