#include "app/GaussianRipple.hpp"
#include "app/SurfaceRegistry.hpp"
#include "math/SineRationalSurface.hpp"
#include "math/Surfaces.hpp"
#include "simulation/fields/MetricRipple.hpp"

#include <gtest/gtest.h>

#include <memory>

TEST(SurfaceMetadata, SineRationalExposesFormulaDomainAndDerivativeSupport) {
    ndde::math::SineRationalSurface surface{4.f};
    const ndde::math::SurfaceMetadata metadata = surface.metadata();

    EXPECT_EQ(metadata.name, "Sine-Rational Surface");
    EXPECT_FALSE(metadata.formula.empty());
    EXPECT_TRUE(metadata.has_analytic_derivatives);
    EXPECT_FALSE(metadata.deformable);
    EXPECT_FLOAT_EQ(metadata.domain.u_min, -4.f);
    EXPECT_FLOAT_EQ(metadata.domain.u_max, 4.f);
    ASSERT_EQ(metadata.parameter_count, 1u);
    EXPECT_EQ(metadata.parameters[0].name, "extent");
}

TEST(SurfaceMetadata, GaussianRippleReportsDeformableParameters) {
    ndde::GaussianRipple ripple;
    const ndde::math::SurfaceMetadata metadata = ripple.metadata();

    EXPECT_EQ(metadata.name, "Gaussian Ripple");
    EXPECT_TRUE(metadata.deformable);
    EXPECT_TRUE(metadata.time_varying);
    ASSERT_GE(metadata.parameter_count, 5u);
    EXPECT_EQ(metadata.parameters[0].name, "amplitude");
}

TEST(SurfaceMetadata, AppRegistrySurfacesExposeNames) {
    const auto multi = ndde::SurfaceRegistry::make_multi_well();
    const auto wave = ndde::SurfaceRegistry::make_wave_predator_prey();

    EXPECT_EQ(multi->metadata().name, "Multi-Well Wave Surface");
    EXPECT_EQ(wave->metadata().name, "Wave Predator-Prey Surface");
}

TEST(FieldCompositor, MetricRippleContributesMetricAndSurfaceDisplacement) {
    ndde::simulation::FieldCompositor fields;
    ndde::simulation::MetricRipple::Params params;
    params.u = 0.f;
    params.v = 0.f;
    params.t0 = 0.f;
    params.amplitude = 0.5f;
    params.omega = 6.28f;
    params.k_wave = 3.f;
    params.alpha = 0.25f;
    params.beta = 0.1f;
    params.epsilon = 0.2f;
    params.geometry_scale = 2.f;

    fields.add(std::make_shared<ndde::simulation::MetricRipple>(params));

    const float metric = fields.metric_factor(0.25f, 0.f, 0.25f);
    const float displacement = fields.surface_displacement(0.25f, 0.f, 0.25f);

    EXPECT_NE(metric, 1.f);
    EXPECT_NE(displacement, 0.f);
    EXPECT_NEAR(metric, 1.f + 0.2f * displacement / 2.f, 1e-5f);
}
