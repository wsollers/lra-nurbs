#pragma once
// sim/DifferentialSystem.hpp
// General differential equation systems and fixed-step ODE solvers.

#include "math/Scalars.hpp"
#include "memory/MemoryService.hpp"

#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ndde::sim {

struct EquationSystemMetadata {
    std::string name;
    std::string formula;
    std::string variables;
};

class IDifferentialSystem {
public:
    virtual ~IDifferentialSystem() = default;

    [[nodiscard]] virtual EquationSystemMetadata metadata() const = 0;
    [[nodiscard]] virtual std::size_t dimension() const = 0;

    virtual void evaluate(f64 t,
                          std::span<const f64> state,
                          std::span<f64> derivative) const = 0;

protected:
    IDifferentialSystem() = default;
    IDifferentialSystem(const IDifferentialSystem&) = default;
    IDifferentialSystem& operator=(const IDifferentialSystem&) = default;
    IDifferentialSystem(IDifferentialSystem&&) = default;
    IDifferentialSystem& operator=(IDifferentialSystem&&) = default;
};

class IOdeSolver {
public:
    virtual ~IOdeSolver() = default;

    [[nodiscard]] virtual std::string_view name() const = 0;
    [[nodiscard]] virtual std::size_t required_scratch_values(std::size_t dimension) const noexcept = 0;

    virtual void step(const IDifferentialSystem& system,
                      f64 t,
                      f64 dt,
                      std::span<f64> state,
                      std::span<f64> scratch) const = 0;

protected:
    IOdeSolver() = default;
    IOdeSolver(const IOdeSolver&) = default;
    IOdeSolver& operator=(const IOdeSolver&) = default;
    IOdeSolver(IOdeSolver&&) = default;
    IOdeSolver& operator=(IOdeSolver&&) = default;
};

struct OdeHistorySample {
    f64 t = 0.0;
    std::size_t offset = 0;
};

class InitialValueProblem {
public:
    InitialValueProblem(memory::MemoryService& memory,
                        const IDifferentialSystem& system,
                        std::span<const f64> initial_state,
                        f64 t0 = 0.0)
        : m_memory(memory)
        , m_system(system)
        , m_initial(memory.simulation().make_vector<f64>())
        , m_state(memory.simulation().make_vector<f64>())
        , m_scratch(memory.simulation().make_vector<f64>())
        , m_history(memory.history().make_vector<OdeHistorySample>())
        , m_history_values(memory.history().make_vector<f64>())
        , m_t(t0)
        , m_t0(t0)
    {
        m_initial.assign(initial_state.begin(), initial_state.end());
        m_state.assign(initial_state.begin(), initial_state.end());
        record_history();
    }

    void reset() {
        m_state.assign(m_initial.begin(), m_initial.end());
        m_scratch.clear();
        m_history.clear();
        m_history_values.clear();
        m_t = m_t0;
        record_history();
    }

    void step(const IOdeSolver& solver, f64 dt) {
        const std::size_t required = solver.required_scratch_values(m_state.size());
        if (m_scratch.size() < required)
            m_scratch.resize(required);
        solver.step(m_system, m_t, dt, m_state, m_scratch);
        m_t += dt;
        record_history();
    }

    [[nodiscard]] const IDifferentialSystem& system() const noexcept { return m_system; }
    [[nodiscard]] std::span<const f64> initial_state() const noexcept { return m_initial; }
    [[nodiscard]] std::span<const f64> state() const noexcept { return m_state; }
    [[nodiscard]] f64 time() const noexcept { return m_t; }
    [[nodiscard]] std::size_t dimension() const noexcept { return m_state.size(); }
    [[nodiscard]] std::size_t history_size() const noexcept { return m_history.size(); }

    [[nodiscard]] std::span<const f64> history_state(std::size_t sample) const noexcept {
        if (sample >= m_history.size()) return {};
        const OdeHistorySample& h = m_history[sample];
        return std::span<const f64>{m_history_values.data() + h.offset, m_state.size()};
    }

    [[nodiscard]] f64 history_time(std::size_t sample) const noexcept {
        return sample < m_history.size() ? m_history[sample].t : 0.0;
    }

private:
    memory::MemoryService& m_memory;
    const IDifferentialSystem& m_system;
    memory::SimVector<f64> m_initial;
    memory::SimVector<f64> m_state;
    memory::SimVector<f64> m_scratch;
    memory::HistoryVector<OdeHistorySample> m_history;
    memory::HistoryVector<f64> m_history_values;
    f64 m_t = 0.0;
    f64 m_t0 = 0.0;

    void record_history() {
        const std::size_t offset = m_history_values.size();
        m_history.push_back(OdeHistorySample{.t = m_t, .offset = offset});
        for (const f64 value : m_state)
            m_history_values.push_back(value);
    }
};

class EulerOdeSolver final : public IOdeSolver {
public:
    [[nodiscard]] std::string_view name() const override { return "Euler"; }
    [[nodiscard]] std::size_t required_scratch_values(std::size_t dimension) const noexcept override {
        return dimension;
    }

    void step(const IDifferentialSystem& system,
              f64 t,
              f64 dt,
              std::span<f64> state,
              std::span<f64> scratch) const override
    {
        const std::size_t n = state.size();
        auto dydt = scratch.first(n);
        system.evaluate(t, state, dydt);
        for (std::size_t i = 0; i < n; ++i)
            state[i] += dt * dydt[i];
    }
};

class Rk4OdeSolver final : public IOdeSolver {
public:
    [[nodiscard]] std::string_view name() const override { return "RK4"; }
    [[nodiscard]] std::size_t required_scratch_values(std::size_t dimension) const noexcept override {
        return dimension * 5u;
    }

    void step(const IDifferentialSystem& system,
              f64 t,
              f64 dt,
              std::span<f64> state,
              std::span<f64> scratch) const override
    {
        const std::size_t n = state.size();
        auto k1 = scratch.subspan(0u, n);
        auto k2 = scratch.subspan(n, n);
        auto k3 = scratch.subspan(2u * n, n);
        auto k4 = scratch.subspan(3u * n, n);
        auto tmp = scratch.subspan(4u * n, n);

        system.evaluate(t, state, k1);

        for (std::size_t i = 0; i < n; ++i)
            tmp[i] = state[i] + 0.5 * dt * k1[i];
        system.evaluate(t + 0.5 * dt, tmp, k2);

        for (std::size_t i = 0; i < n; ++i)
            tmp[i] = state[i] + 0.5 * dt * k2[i];
        system.evaluate(t + 0.5 * dt, tmp, k3);

        for (std::size_t i = 0; i < n; ++i)
            tmp[i] = state[i] + dt * k3[i];
        system.evaluate(t + dt, tmp, k4);

        for (std::size_t i = 0; i < n; ++i)
            state[i] += (dt / 6.0) * (k1[i] + 2.0 * k2[i] + 2.0 * k3[i] + k4[i]);
    }
};

class ExponentialGrowthSystem final : public IDifferentialSystem {
public:
    explicit ExponentialGrowthSystem(f64 rate = 1.0) : m_rate(rate) {}

    [[nodiscard]] EquationSystemMetadata metadata() const override {
        return {
            .name = "Exponential growth",
            .formula = "y' = r y",
            .variables = "y"
        };
    }

    [[nodiscard]] std::size_t dimension() const override { return 1u; }

    void evaluate(f64, std::span<const f64> state, std::span<f64> derivative) const override {
        derivative[0] = m_rate * state[0];
    }

private:
    f64 m_rate = 1.0;
};

class HarmonicOscillatorSystem final : public IDifferentialSystem {
public:
    explicit HarmonicOscillatorSystem(f64 omega = 1.0) : m_omega(omega) {}

    [[nodiscard]] EquationSystemMetadata metadata() const override {
        return {
            .name = "Harmonic oscillator",
            .formula = "x' = v, v' = -omega^2 x",
            .variables = "x, v"
        };
    }

    [[nodiscard]] std::size_t dimension() const override { return 2u; }

    void evaluate(f64, std::span<const f64> state, std::span<f64> derivative) const override {
        derivative[0] = state[1];
        derivative[1] = -(m_omega * m_omega) * state[0];
    }

    [[nodiscard]] f64 omega() const noexcept { return m_omega; }

private:
    f64 m_omega = 1.0;
};

class DampedOscillatorSystem final : public IDifferentialSystem {
public:
    DampedOscillatorSystem(f64 omega = 1.0, f64 gamma = 0.15)
        : m_omega(omega)
        , m_gamma(gamma)
    {}

    [[nodiscard]] EquationSystemMetadata metadata() const override {
        return {
            .name = "Damped oscillator",
            .formula = "x' = v, v' = -2 gamma v - omega^2 x",
            .variables = "x, v"
        };
    }

    [[nodiscard]] std::size_t dimension() const override { return 2u; }

    void evaluate(f64, std::span<const f64> state, std::span<f64> derivative) const override {
        derivative[0] = state[1];
        derivative[1] = -2.0 * m_gamma * state[1] - (m_omega * m_omega) * state[0];
    }

    [[nodiscard]] f64 omega() const noexcept { return m_omega; }
    [[nodiscard]] f64 gamma() const noexcept { return m_gamma; }

private:
    f64 m_omega = 1.0;
    f64 m_gamma = 0.15;
};

class VanDerPolSystem final : public IDifferentialSystem {
public:
    explicit VanDerPolSystem(f64 mu = 1.4) : m_mu(mu) {}

    [[nodiscard]] EquationSystemMetadata metadata() const override {
        return {
            .name = "Van der Pol oscillator",
            .formula = "x' = v, v' = mu(1 - x^2)v - x",
            .variables = "x, v"
        };
    }

    [[nodiscard]] std::size_t dimension() const override { return 2u; }

    void evaluate(f64, std::span<const f64> state, std::span<f64> derivative) const override {
        derivative[0] = state[1];
        derivative[1] = m_mu * (1.0 - state[0] * state[0]) * state[1] - state[0];
    }

    [[nodiscard]] f64 mu() const noexcept { return m_mu; }

private:
    f64 m_mu = 1.4;
};

class PredatorPreySystem final : public IDifferentialSystem {
public:
    PredatorPreySystem(f64 alpha = 1.1, f64 beta = 0.4, f64 delta = 0.25, f64 gamma = 0.9)
        : m_alpha(alpha)
        , m_beta(beta)
        , m_delta(delta)
        , m_gamma(gamma)
    {}

    [[nodiscard]] EquationSystemMetadata metadata() const override {
        return {
            .name = "Predator-prey",
            .formula = "prey' = alpha prey - beta prey predator, predator' = delta prey predator - gamma predator",
            .variables = "prey, predator"
        };
    }

    [[nodiscard]] std::size_t dimension() const override { return 2u; }

    void evaluate(f64, std::span<const f64> state, std::span<f64> derivative) const override {
        const f64 prey = state[0];
        const f64 predator = state[1];
        derivative[0] = m_alpha * prey - m_beta * prey * predator;
        derivative[1] = m_delta * prey * predator - m_gamma * predator;
    }

private:
    f64 m_alpha = 1.1;
    f64 m_beta = 0.4;
    f64 m_delta = 0.25;
    f64 m_gamma = 0.9;
};

class LorenzSystem final : public IDifferentialSystem {
public:
    LorenzSystem(f64 sigma = 10.0, f64 rho = 28.0, f64 beta = 8.0 / 3.0)
        : m_sigma(sigma)
        , m_rho(rho)
        , m_beta(beta)
    {}

    [[nodiscard]] EquationSystemMetadata metadata() const override {
        return {
            .name = "Lorenz attractor",
            .formula = "x' = sigma(y-x), y' = x(rho-z)-y, z' = xy-beta z",
            .variables = "x, y, z"
        };
    }

    [[nodiscard]] std::size_t dimension() const override { return 3u; }

    void evaluate(f64, std::span<const f64> state, std::span<f64> derivative) const override {
        const f64 x = state[0];
        const f64 y = state[1];
        const f64 z = state[2];
        derivative[0] = m_sigma * (y - x);
        derivative[1] = x * (m_rho - z) - y;
        derivative[2] = x * y - m_beta * z;
    }

    [[nodiscard]] f64 sigma() const noexcept { return m_sigma; }
    [[nodiscard]] f64 rho() const noexcept { return m_rho; }
    [[nodiscard]] f64 beta() const noexcept { return m_beta; }

private:
    f64 m_sigma = 10.0;
    f64 m_rho = 28.0;
    f64 m_beta = 8.0 / 3.0;
};

class SimplePendulumSystem final : public IDifferentialSystem {
public:
    SimplePendulumSystem(f64 gravity = 9.80665, f64 length = 1.0, f64 damping = 0.0)
        : m_gravity(gravity)
        , m_length(length > 0.0 ? length : 1.0)
        , m_damping(damping)
    {}

    [[nodiscard]] EquationSystemMetadata metadata() const override {
        return {
            .name = "Simple pendulum",
            .formula = "theta' = omega, omega' = -(g/L) sin(theta) - c omega",
            .variables = "theta, omega"
        };
    }

    [[nodiscard]] std::size_t dimension() const override { return 2u; }

    void evaluate(f64, std::span<const f64> state, std::span<f64> derivative) const override {
        derivative[0] = state[1];
        derivative[1] = -(m_gravity / m_length) * std::sin(state[0]) - m_damping * state[1];
    }

    [[nodiscard]] f64 gravity() const noexcept { return m_gravity; }
    [[nodiscard]] f64 length() const noexcept { return m_length; }
    [[nodiscard]] f64 damping() const noexcept { return m_damping; }

    [[nodiscard]] f64 energy(std::span<const f64> state) const noexcept {
        const f64 theta = state.size() > 0 ? state[0] : 0.0;
        const f64 omega = state.size() > 1 ? state[1] : 0.0;
        return 0.5 * m_length * m_length * omega * omega
             + m_gravity * m_length * (1.0 - std::cos(theta));
    }

private:
    f64 m_gravity = 9.80665;
    f64 m_length = 1.0;
    f64 m_damping = 0.0;
};

class PlanarNBodyGravitySystem final : public IDifferentialSystem {
public:
    PlanarNBodyGravitySystem(std::vector<f64> masses,
                             f64 gravitational_constant = 1.0,
                             f64 softening = 1.0e-6)
        : m_masses(std::move(masses))
        , m_gravitational_constant(gravitational_constant)
        , m_softening(std::max(0.0, softening))
    {}

    [[nodiscard]] EquationSystemMetadata metadata() const override {
        return {
            .name = "Planar N-body gravity",
            .formula = "x_i' = v_i, v_i' = sum_{j!=i} G m_j (x_j-x_i) / (|x_j-x_i|^2 + eps^2)^(3/2)",
            .variables = "x_i, y_i, vx_i, vy_i for each body"
        };
    }

    [[nodiscard]] std::size_t body_count() const noexcept { return m_masses.size(); }
    [[nodiscard]] std::size_t dimension() const override { return m_masses.size() * 4u; }
    [[nodiscard]] std::span<const f64> masses() const noexcept { return m_masses; }
    [[nodiscard]] f64 gravitational_constant() const noexcept { return m_gravitational_constant; }
    [[nodiscard]] f64 softening() const noexcept { return m_softening; }

    void evaluate(f64, std::span<const f64> state, std::span<f64> derivative) const override {
        const std::size_t n = m_masses.size();
        for (std::size_t i = 0; i < n; ++i) {
            const std::size_t oi = i * 4u;
            derivative[oi + 0u] = state[oi + 2u];
            derivative[oi + 1u] = state[oi + 3u];
            derivative[oi + 2u] = 0.0;
            derivative[oi + 3u] = 0.0;
        }

        const f64 eps2 = m_softening * m_softening;
        for (std::size_t i = 0; i < n; ++i) {
            const std::size_t oi = i * 4u;
            for (std::size_t j = 0; j < n; ++j) {
                if (i == j) continue;
                const std::size_t oj = j * 4u;
                const f64 dx = state[oj + 0u] - state[oi + 0u];
                const f64 dy = state[oj + 1u] - state[oi + 1u];
                const f64 r2 = dx * dx + dy * dy + eps2;
                const f64 inv_r = 1.0 / std::sqrt(r2);
                const f64 inv_r3 = inv_r * inv_r * inv_r;
                const f64 scale = m_gravitational_constant * m_masses[j] * inv_r3;
                derivative[oi + 2u] += scale * dx;
                derivative[oi + 3u] += scale * dy;
            }
        }
    }

    [[nodiscard]] f64 total_energy(std::span<const f64> state) const noexcept {
        const std::size_t n = m_masses.size();
        f64 kinetic = 0.0;
        f64 potential = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            const std::size_t oi = i * 4u;
            const f64 vx = state[oi + 2u];
            const f64 vy = state[oi + 3u];
            kinetic += 0.5 * m_masses[i] * (vx * vx + vy * vy);
            for (std::size_t j = i + 1u; j < n; ++j) {
                const std::size_t oj = j * 4u;
                const f64 dx = state[oj + 0u] - state[oi + 0u];
                const f64 dy = state[oj + 1u] - state[oi + 1u];
                const f64 r = std::sqrt(dx * dx + dy * dy + m_softening * m_softening);
                potential -= m_gravitational_constant * m_masses[i] * m_masses[j] / r;
            }
        }
        return kinetic + potential;
    }

private:
    std::vector<f64> m_masses;
    f64 m_gravitational_constant = 1.0;
    f64 m_softening = 1.0e-6;
};

} // namespace ndde::sim
