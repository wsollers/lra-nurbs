#pragma once
// sim/DelayDifferentialSystem.hpp
// General delay differential equation systems and fixed-step DDE solvers.

#include "math/Scalars.hpp"
#include "memory/MemoryService.hpp"
#include "sim/DifferentialSystem.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <span>
#include <string_view>

namespace ndde::sim {

class DelayHistoryView {
public:
    using QueryFn = void (*)(const void* context, f64 t, std::span<f64> out);

    DelayHistoryView() = default;
    DelayHistoryView(const void* context, QueryFn query) noexcept
        : m_context(context)
        , m_query(query)
    {}

    void query(f64 t, std::span<f64> out) const {
        assert(m_context != nullptr);
        assert(m_query != nullptr);
        m_query(m_context, t, out);
    }

private:
    const void* m_context = nullptr;
    QueryFn m_query = nullptr;
};

class IDelayDifferentialSystem {
public:
    virtual ~IDelayDifferentialSystem() = default;

    [[nodiscard]] virtual EquationSystemMetadata metadata() const = 0;
    [[nodiscard]] virtual std::size_t dimension() const = 0;
    [[nodiscard]] virtual f64 max_delay() const noexcept = 0;

    virtual void evaluate(f64 t,
                          std::span<const f64> state,
                          const DelayHistoryView& history,
                          std::span<f64> derivative) const = 0;

protected:
    IDelayDifferentialSystem() = default;
    IDelayDifferentialSystem(const IDelayDifferentialSystem&) = default;
    IDelayDifferentialSystem& operator=(const IDelayDifferentialSystem&) = default;
    IDelayDifferentialSystem(IDelayDifferentialSystem&&) = default;
    IDelayDifferentialSystem& operator=(IDelayDifferentialSystem&&) = default;
};

class IDdeSolver {
public:
    virtual ~IDdeSolver() = default;

    [[nodiscard]] virtual std::string_view name() const = 0;
    [[nodiscard]] virtual std::size_t required_scratch_values(std::size_t dimension) const noexcept = 0;

    virtual void step(const IDelayDifferentialSystem& system,
                      const DelayHistoryView& history,
                      f64 t,
                      f64 dt,
                      std::span<f64> state,
                      std::span<f64> scratch) const = 0;

protected:
    IDdeSolver() = default;
    IDdeSolver(const IDdeSolver&) = default;
    IDdeSolver& operator=(const IDdeSolver&) = default;
    IDdeSolver(IDdeSolver&&) = default;
    IDdeSolver& operator=(IDdeSolver&&) = default;
};

struct DdeHistorySample {
    f64 t = 0.0;
    std::size_t offset = 0;
};

class DelayInitialValueProblem {
public:
    using HistoryInitializer = void (*)(f64 t, std::span<f64> out, void* user);

    DelayInitialValueProblem(memory::MemoryService& memory,
                             const IDelayDifferentialSystem& system,
                             std::span<const f64> initial_state,
                             f64 t0 = 0.0,
                             f64 history_sample_dt = 1.0 / 120.0,
                             HistoryInitializer history_initializer = nullptr,
                             void* history_initializer_user = nullptr)
        : m_memory(memory)
        , m_system(system)
        , m_initial(memory.simulation().make_vector<f64>())
        , m_state(memory.simulation().make_vector<f64>())
        , m_scratch(memory.simulation().make_vector<f64>())
        , m_history(memory.history().make_vector<DdeHistorySample>())
        , m_history_values(memory.history().make_vector<f64>())
        , m_t(t0)
        , m_t0(t0)
        , m_history_sample_dt(history_sample_dt)
        , m_history_initializer(history_initializer)
        , m_history_initializer_user(history_initializer_user)
    {
        assert(initial_state.size() == system.dimension());
        m_initial.assign(initial_state.begin(), initial_state.end());
        m_state.assign(initial_state.begin(), initial_state.end());
        if (m_history_initializer)
            m_history_initializer(m_t0, m_initial, m_history_initializer_user);
        m_state.assign(m_initial.begin(), m_initial.end());
        sample_initial_history();
    }

    void reset() {
        m_state.assign(m_initial.begin(), m_initial.end());
        m_scratch.clear();
        m_history.clear();
        m_history_values.clear();
        m_t = m_t0;
        sample_initial_history();
    }

    void step(const IDdeSolver& solver, f64 dt) {
        const std::size_t required = solver.required_scratch_values(m_state.size());
        if (m_scratch.size() < required)
            m_scratch.resize(required);
        solver.step(m_system, history_view(), m_t, dt, m_state, m_scratch);
        m_t += dt;
        record_history(m_t, m_state);
    }

    void query_history(f64 t, std::span<f64> out) const {
        assert(out.size() == m_state.size());
        if (m_history.empty() || m_state.empty())
            return;

        if (t <= m_history.front().t) {
            copy_history_sample(0u, out);
            return;
        }

        if (t >= m_history.back().t) {
            copy_history_sample(m_history.size() - 1u, out);
            return;
        }

        const auto hi_it = std::lower_bound(m_history.begin(), m_history.end(), t,
            [](const DdeHistorySample& sample, f64 value) {
                return sample.t < value;
            });
        const std::size_t hi = static_cast<std::size_t>(std::distance(m_history.begin(), hi_it));
        const std::size_t lo = hi - 1u;
        const f64 t0 = m_history[lo].t;
        const f64 t1 = m_history[hi].t;
        const f64 alpha = (t1 > t0) ? ((t - t0) / (t1 - t0)) : 0.0;
        const auto y0 = history_state(lo);
        const auto y1 = history_state(hi);
        for (std::size_t i = 0; i < out.size(); ++i)
            out[i] = y0[i] + alpha * (y1[i] - y0[i]);
    }

    [[nodiscard]] const IDelayDifferentialSystem& system() const noexcept { return m_system; }
    [[nodiscard]] std::span<const f64> initial_state() const noexcept { return m_initial; }
    [[nodiscard]] std::span<const f64> state() const noexcept { return m_state; }
    [[nodiscard]] f64 time() const noexcept { return m_t; }
    [[nodiscard]] std::size_t dimension() const noexcept { return m_state.size(); }
    [[nodiscard]] std::size_t history_size() const noexcept { return m_history.size(); }

    [[nodiscard]] std::span<const f64> history_state(std::size_t sample) const noexcept {
        if (sample >= m_history.size()) return {};
        const DdeHistorySample& h = m_history[sample];
        return std::span<const f64>{m_history_values.data() + h.offset, m_state.size()};
    }

    [[nodiscard]] f64 history_time(std::size_t sample) const noexcept {
        return sample < m_history.size() ? m_history[sample].t : 0.0;
    }

private:
    memory::MemoryService& m_memory;
    const IDelayDifferentialSystem& m_system;
    memory::SimVector<f64> m_initial;
    memory::SimVector<f64> m_state;
    memory::SimVector<f64> m_scratch;
    memory::HistoryVector<DdeHistorySample> m_history;
    memory::HistoryVector<f64> m_history_values;
    f64 m_t = 0.0;
    f64 m_t0 = 0.0;
    f64 m_history_sample_dt = 1.0 / 120.0;
    HistoryInitializer m_history_initializer = nullptr;
    void* m_history_initializer_user = nullptr;

    [[nodiscard]] DelayHistoryView history_view() const noexcept {
        return DelayHistoryView{
            this,
            [](const void* context, f64 t, std::span<f64> out) {
                static_cast<const DelayInitialValueProblem*>(context)->query_history(t, out);
            }
        };
    }

    void sample_initial_history() {
        const f64 delay = std::max(0.0, m_system.max_delay());
        if (delay == 0.0) {
            record_history(m_t0, m_state);
            return;
        }

        const f64 requested_dt = m_history_sample_dt > 0.0 ? m_history_sample_dt : delay;
        const std::size_t steps = std::max<std::size_t>(1u, static_cast<std::size_t>(std::ceil(delay / requested_dt)));
        const f64 dt = delay / static_cast<f64>(steps);
        const f64 start = m_t0 - delay;
        for (std::size_t i = 0; i <= steps; ++i) {
            const f64 t = (i == steps) ? m_t0 : start + static_cast<f64>(i) * dt;
            record_initial_history(t);
        }
    }

    void record_initial_history(f64 t) {
        const std::size_t offset = m_history_values.size();
        for (std::size_t i = 0; i < m_state.size(); ++i)
            m_history_values.push_back(0.0);

        std::span<f64> out{m_history_values.data() + offset, m_state.size()};
        if (m_history_initializer) {
            m_history_initializer(t, out, m_history_initializer_user);
        } else {
            for (std::size_t i = 0; i < m_state.size(); ++i)
                out[i] = m_initial[i];
        }

        m_history.push_back(DdeHistorySample{.t = t, .offset = offset});
        if (t == m_t0)
            m_state.assign(out.begin(), out.end());
    }

    void record_history(f64 t, std::span<const f64> state) {
        const std::size_t offset = m_history_values.size();
        m_history.push_back(DdeHistorySample{.t = t, .offset = offset});
        for (const f64 value : state)
            m_history_values.push_back(value);
        compact_history_if_needed(t);
    }

    [[nodiscard]] std::size_t max_retained_history_samples() const noexcept {
        constexpr f64 kDisplayMarginSeconds = 10.0;
        constexpr std::size_t kMinimumSamples = 4096u;
        const f64 dt = m_history_sample_dt > 0.0 ? m_history_sample_dt : (1.0 / 120.0);
        const f64 window = std::max(0.0, m_system.max_delay()) + kDisplayMarginSeconds;
        return std::max(kMinimumSamples,
                        static_cast<std::size_t>(std::ceil(window / dt)) + 2u);
    }

    void compact_history_if_needed(f64 now) {
        const std::size_t max_samples = max_retained_history_samples();
        if (m_history.size() <= max_samples)
            return;

        constexpr f64 kDisplayMarginSeconds = 10.0;
        const f64 cutoff = now - std::max(0.0, m_system.max_delay()) - kDisplayMarginSeconds;
        std::size_t first_keep = 0;
        while (first_keep + 2u < m_history.size() && m_history[first_keep].t < cutoff)
            ++first_keep;
        if (first_keep == 0)
            first_keep = m_history.size() > max_samples ? m_history.size() - max_samples : 0;
        if (first_keep == 0)
            return;

        auto compact_samples = m_memory.history().make_vector<DdeHistorySample>();
        auto compact_values = m_memory.history().make_vector<f64>();
        const std::size_t dimension = m_state.size();
        compact_samples.reserve(m_history.size() - first_keep);
        compact_values.reserve((m_history.size() - first_keep) * dimension);

        for (std::size_t sample = first_keep; sample < m_history.size(); ++sample) {
            const auto state = history_state(sample);
            const std::size_t offset = compact_values.size();
            compact_samples.push_back(DdeHistorySample{.t = m_history[sample].t, .offset = offset});
            for (const f64 value : state)
                compact_values.push_back(value);
        }

        m_history = std::move(compact_samples);
        m_history_values = std::move(compact_values);
    }

    void copy_history_sample(std::size_t sample, std::span<f64> out) const {
        const auto state = history_state(sample);
        for (std::size_t i = 0; i < out.size(); ++i)
            out[i] = state[i];
    }
};

class EulerDdeSolver final : public IDdeSolver {
public:
    [[nodiscard]] std::string_view name() const override { return "Euler DDE"; }
    [[nodiscard]] std::size_t required_scratch_values(std::size_t dimension) const noexcept override {
        return dimension;
    }

    void step(const IDelayDifferentialSystem& system,
              const DelayHistoryView& history,
              f64 t,
              f64 dt,
              std::span<f64> state,
              std::span<f64> scratch) const override
    {
        const std::size_t n = state.size();
        auto dydt = scratch.first(n);
        system.evaluate(t, state, history, dydt);
        for (std::size_t i = 0; i < n; ++i)
            state[i] += dt * dydt[i];
    }
};

class LinearDelaySystem final : public IDelayDifferentialSystem {
public:
    LinearDelaySystem(f64 current_weight = 0.0, f64 delayed_weight = 1.0, f64 delay = 1.0)
        : m_current_weight(current_weight)
        , m_delayed_weight(delayed_weight)
        , m_delay(delay)
    {}

    [[nodiscard]] EquationSystemMetadata metadata() const override {
        return {
            .name = "Linear delay",
            .formula = "y' = a y(t) + b y(t - tau)",
            .variables = "y"
        };
    }

    [[nodiscard]] std::size_t dimension() const override { return 1u; }
    [[nodiscard]] f64 max_delay() const noexcept override { return m_delay; }

    void evaluate(f64 t,
                  std::span<const f64> state,
                  const DelayHistoryView& history,
                  std::span<f64> derivative) const override
    {
        f64 delayed_value = 0.0;
        history.query(t - m_delay, std::span<f64>{&delayed_value, 1u});
        derivative[0] = m_current_weight * state[0] + m_delayed_weight * delayed_value;
    }

private:
    f64 m_current_weight = 0.0;
    f64 m_delayed_weight = 1.0;
    f64 m_delay = 1.0;
};

class BoundedDelayedFeedbackSystem final : public IDelayDifferentialSystem {
public:
    BoundedDelayedFeedbackSystem(f64 damping = 0.8, f64 feedback = 1.4, f64 delay = 1.2)
        : m_damping(damping)
        , m_feedback(feedback)
        , m_delay(delay)
    {}

    [[nodiscard]] EquationSystemMetadata metadata() const override {
        return {
            .name = "Bounded delayed feedback",
            .formula = "x' = -a x(t) + b tanh(x(t - tau))",
            .variables = "x"
        };
    }

    [[nodiscard]] std::size_t dimension() const override { return 1u; }
    [[nodiscard]] f64 max_delay() const noexcept override { return m_delay; }

    void evaluate(f64 t,
                  std::span<const f64> state,
                  const DelayHistoryView& history,
                  std::span<f64> derivative) const override
    {
        f64 delayed_value = 0.0;
        history.query(t - m_delay, std::span<f64>{&delayed_value, 1u});
        derivative[0] = -m_damping * state[0] + m_feedback * std::tanh(delayed_value);
    }

    [[nodiscard]] f64 damping() const noexcept { return m_damping; }
    [[nodiscard]] f64 feedback() const noexcept { return m_feedback; }
    [[nodiscard]] f64 delay() const noexcept { return m_delay; }
    [[nodiscard]] f64 expected_bound() const noexcept {
        return m_damping > 0.0 ? std::abs(m_feedback / m_damping) : std::abs(m_feedback);
    }

private:
    f64 m_damping = 0.8;
    f64 m_feedback = 1.4;
    f64 m_delay = 1.2;
};

} // namespace ndde::sim
