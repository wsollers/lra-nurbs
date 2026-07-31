#pragma once
// engine/coordinates/CoordinateOverlayService.hpp
// Engine-owned coordinate canvas generation for simulations and workbenches.

#include "engine/RenderService.hpp"
#include "engine/text/TextOverlayService.hpp"

#include <string>

namespace ndde {

enum class CoordinateSpaceKind : u8 {
    Cartesian2D,
    Polar2D
};

struct CoordinateVisibleBounds2D {
    f32 left = -1.f;
    f32 right = 1.f;
    f32 bottom = -1.f;
    f32 top = 1.f;

    [[nodiscard]] f32 width() const noexcept { return right - left; }
    [[nodiscard]] f32 height() const noexcept { return top - bottom; }
};

struct AxisGradationConfig {
    f32 major_step = 1.f;
    f32 minor_step = 0.2f;
    bool dynamic_steps = true;
};

struct AxisLabelConfig {
    std::string x = "x";
    std::string y = "y";
    bool show = true;
};

struct ResolvedAxisGradations {
    f32 minor_step = 0.2f;
    f32 major_step = 1.f;
};

struct ResolvedAxisLabels2D {
    bool show = true;
    TextDrawCommand x_axis{};
    TextDrawCommand y_axis{};
};

struct CoordinateOverlayDescriptor {
    CoordinateSpaceKind space = CoordinateSpaceKind::Cartesian2D;
    AxisGradationConfig gradations{};
    AxisLabelConfig labels{};
    bool show_grid = true;
    bool show_axes = true;
    bool show_polar_rings = false;
    bool show_polar_spokes = false;
};

class CoordinateOverlayService {
public:
    [[nodiscard]] static CoordinateVisibleBounds2D visible_bounds(RenderService& render,
                                                                  RenderViewId view) noexcept;

    [[nodiscard]] static ResolvedAxisGradations resolve_gradations(CoordinateVisibleBounds2D bounds,
                                                                   AxisGradationConfig config) noexcept;

    [[nodiscard]] static ResolvedAxisLabels2D resolve_axis_labels(CoordinateVisibleBounds2D bounds,
                                                                  const AxisLabelConfig& labels);

    static void submit(RenderService& render,
                       TextOverlayService& text,
                       memory::MemoryService& memory,
                       RenderViewId view,
                       const CoordinateOverlayDescriptor& descriptor,
                       Mat4 mvp);
};

} // namespace ndde
