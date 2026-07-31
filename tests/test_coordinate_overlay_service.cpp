#include "engine/coordinates/CoordinateOverlayService.hpp"

#include <gtest/gtest.h>

namespace {

using namespace ndde;

TEST(CoordinateOverlayService, DynamicGradationsFollowVisibleSpan) {
    const auto wide = CoordinateOverlayService::resolve_gradations(
        CoordinateVisibleBounds2D{.left = -10.f, .right = 10.f, .bottom = -2.f, .top = 2.f},
        AxisGradationConfig{.dynamic_steps = true});
    const auto close = CoordinateOverlayService::resolve_gradations(
        CoordinateVisibleBounds2D{.left = -0.2f, .right = 0.2f, .bottom = -0.1f, .top = 0.1f},
        AxisGradationConfig{.dynamic_steps = true});

    EXPECT_FLOAT_EQ(wide.minor_step, 1.f);
    EXPECT_FLOAT_EQ(wide.major_step, 5.f);
    EXPECT_FLOAT_EQ(close.minor_step, 0.02f);
    EXPECT_FLOAT_EQ(close.major_step, 0.1f);
}

TEST(CoordinateOverlayService, StaticGradationsClampToValidSteps) {
    const auto gradations = CoordinateOverlayService::resolve_gradations(
        CoordinateVisibleBounds2D{.left = -1.f, .right = 1.f, .bottom = -1.f, .top = 1.f},
        AxisGradationConfig{.major_step = 0.1f, .minor_step = -2.f, .dynamic_steps = false});

    EXPECT_FLOAT_EQ(gradations.minor_step, 0.001f);
    EXPECT_FLOAT_EQ(gradations.major_step, 0.1f);
}

TEST(CoordinateOverlayService, AxisLabelsUseDomainBoundsAndMathFont) {
    const auto labels = CoordinateOverlayService::resolve_axis_labels(
        CoordinateVisibleBounds2D{.left = -3.f, .right = 7.f, .bottom = -5.f, .top = 11.f},
        AxisLabelConfig{.x = "u", .y = "v", .show = true});

    ASSERT_TRUE(labels.show);
    EXPECT_EQ(labels.x_axis.space, TextCoordinateSpace::Domain);
    EXPECT_EQ(labels.x_axis.anchor, TextAnchor::Center);
    EXPECT_EQ(labels.x_axis.font, TextFontRole::Math);
    EXPECT_EQ(labels.x_axis.text, "u");
    EXPECT_FLOAT_EQ(labels.x_axis.position.x, 7.f);
    EXPECT_FLOAT_EQ(labels.x_axis.position.y, 0.f);
    EXPECT_EQ(labels.y_axis.text, "v");
    EXPECT_FLOAT_EQ(labels.y_axis.position.x, 0.f);
    EXPECT_FLOAT_EQ(labels.y_axis.position.y, 11.f);
}

TEST(CoordinateOverlayService, HiddenAxisLabelsStayResolvedButMarkedHidden) {
    const auto labels = CoordinateOverlayService::resolve_axis_labels(
        CoordinateVisibleBounds2D{},
        AxisLabelConfig{.x = "x", .y = "y", .show = false});

    EXPECT_FALSE(labels.show);
}

} // namespace
