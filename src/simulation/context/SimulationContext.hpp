#pragma once
// simulation/context/SimulationContext.hpp
// Runtime-neutral context assembled by workbenches.

#include "math/GeometryTypes.hpp"
#include "simulation/context/ObjectStore.hpp"

#include <functional>
#include <string>

namespace ndde::sim {

struct SurfaceDescriptor {
    std::string name;
};

struct CurveDescriptor {
    std::string name;
    std::string formula;
    Vec4 color{1.f, 1.f, 1.f, 1.f};
    std::function<f32(f32)> evaluate;
};

struct ParticleSystemDescriptor {
    std::string name;
    SurfaceHandle surface;
};

struct DeformationDescriptor {
    std::string name;
    SurfaceHandle target;
};

enum class OverlayCoordinateSpace : u8 {
    Cartesian2D,
    Polar2D
};

struct OverlayGradationDescriptor {
    f32 major_step = 1.f;
    f32 minor_step = 0.2f;
    bool dynamic_steps = true;
};

struct OverlayLabelDescriptor {
    std::string x = "x";
    std::string y = "y";
    bool show = true;
};

struct OverlayDescriptor {
    std::string name;
    std::string kind;
    OverlayCoordinateSpace coordinate_space = OverlayCoordinateSpace::Cartesian2D;
    OverlayGradationDescriptor gradations{};
    OverlayLabelDescriptor labels{};
    bool show_grid = true;
    bool show_axes = true;
    bool show_polar_rings = false;
    bool show_polar_spokes = false;
};

struct FieldDescriptor {
    std::string name;
};

struct HistoryBufferDescriptor {
    std::string name;
};

struct SimulationContextDescriptor {
    std::string name;
};

class SimulationContext {
public:
    explicit SimulationContext(SimulationContextDescriptor descriptor = {});

    [[nodiscard]] const SimulationContextDescriptor& descriptor() const noexcept { return m_descriptor; }

    [[nodiscard]] SurfaceHandle add_surface(SurfaceDescriptor descriptor);
    [[nodiscard]] CurveHandle add_curve(CurveDescriptor descriptor);
    [[nodiscard]] ParticleSystemHandle add_particles(ParticleSystemDescriptor descriptor);
    [[nodiscard]] DeformationHandle add_deformation(DeformationDescriptor descriptor);
    [[nodiscard]] OverlayHandle add_overlay(OverlayDescriptor descriptor);
    [[nodiscard]] FieldHandle add_field(FieldDescriptor descriptor);
    [[nodiscard]] HistoryBufferHandle add_history_buffer(HistoryBufferDescriptor descriptor);

    [[nodiscard]] SurfaceDescriptor* surface(SurfaceHandle handle) noexcept { return m_surfaces.get(handle); }
    [[nodiscard]] CurveDescriptor* curve(CurveHandle handle) noexcept { return m_curves.get(handle); }
    [[nodiscard]] OverlayDescriptor* overlay(OverlayHandle handle) noexcept { return m_overlays.get(handle); }

    [[nodiscard]] const SurfaceDescriptor* surface(SurfaceHandle handle) const noexcept { return m_surfaces.get(handle); }
    [[nodiscard]] const CurveDescriptor* curve(CurveHandle handle) const noexcept { return m_curves.get(handle); }
    [[nodiscard]] const OverlayDescriptor* overlay(OverlayHandle handle) const noexcept { return m_overlays.get(handle); }

    [[nodiscard]] const ObjectStore<SurfaceHandle, SurfaceDescriptor>& surfaces() const noexcept { return m_surfaces; }
    [[nodiscard]] const ObjectStore<CurveHandle, CurveDescriptor>& curves() const noexcept { return m_curves; }
    [[nodiscard]] const ObjectStore<OverlayHandle, OverlayDescriptor>& overlays() const noexcept { return m_overlays; }

private:
    SimulationContextDescriptor m_descriptor;
    ObjectStore<SurfaceHandle, SurfaceDescriptor> m_surfaces;
    ObjectStore<CurveHandle, CurveDescriptor> m_curves;
    ObjectStore<ParticleSystemHandle, ParticleSystemDescriptor> m_particles;
    ObjectStore<DeformationHandle, DeformationDescriptor> m_deformations;
    ObjectStore<OverlayHandle, OverlayDescriptor> m_overlays;
    ObjectStore<FieldHandle, FieldDescriptor> m_fields;
    ObjectStore<HistoryBufferHandle, HistoryBufferDescriptor> m_history_buffers;
};

} // namespace ndde::sim
