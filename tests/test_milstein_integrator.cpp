#include "sim/BrownianMotion.hpp"
#include "sim/MilsteinIntegrator.hpp"
#include "math/Surfaces.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace {

struct Moments {
    double mean = 0.0;
    double variance = 0.0;
};

[[nodiscard]] Moments sample_moments(const std::vector<float>& values) {
    double mean = 0.0;
    for (const float value : values)
        mean += static_cast<double>(value);
    mean /= static_cast<double>(values.size());

    double variance = 0.0;
    for (const float value : values) {
        const double delta = static_cast<double>(value) - mean;
        variance += delta * delta;
    }
    variance /= static_cast<double>(values.size() - 1u);
    return {.mean = mean, .variance = variance};
}

} // namespace

TEST(MilsteinIntegrator, PureBrownianMotionHasExpectedLongRunVariance) {
    constexpr ndde::f32 sigma = ndde::f32(0.7);
    constexpr ndde::f32 dt = ndde::f32(0.005);
    constexpr ndde::u32 steps = 200u;
    constexpr ndde::u32 samples = 4096u;
    constexpr ndde::f32 total_time = dt * static_cast<ndde::f32>(steps);
    constexpr double expected_variance = static_cast<double>(sigma * sigma * total_time);

    ndde::sim::MilsteinIntegrator::set_global_seed(0x4d49535445494eull);

    ndde::math::Paraboloid surface;
    ndde::sim::BrownianMotion equation{ndde::sim::BrownianMotion::Params{
        .sigma = sigma,
        .drift_strength = ndde::f32(0)
    }};
    ndde::sim::MilsteinIntegrator integrator;

    std::vector<float> final_u;
    std::vector<float> final_v;
    final_u.reserve(samples);
    final_v.reserve(samples);

    for (ndde::u32 i = 0; i < samples; ++i) {
        ndde::sim::ParticleState state{};
        for (ndde::u32 step = 0; step < steps; ++step) {
            integrator.step(state, equation, surface,
                            static_cast<ndde::f32>(step) * dt, dt);
        }
        final_u.push_back(state.uv.x);
        final_v.push_back(state.uv.y);
    }

    const Moments u = sample_moments(final_u);
    const Moments v = sample_moments(final_v);

    EXPECT_NEAR(u.mean, 0.0, 0.04);
    EXPECT_NEAR(v.mean, 0.0, 0.04);
    EXPECT_NEAR(u.variance, expected_variance, expected_variance * 0.12);
    EXPECT_NEAR(v.variance, expected_variance, expected_variance * 0.12);
}
