#include "simulation/context/SimulationContext.hpp"

#include <utility>

namespace ndde::sim {

SimulationContext::SimulationContext(SimulationContextDescriptor descriptor)
    : m_descriptor(std::move(descriptor))
{}

SurfaceHandle SimulationContext::add_surface(SurfaceDescriptor descriptor) {
    std::string name = descriptor.name;
    return m_surfaces.add(std::move(name), std::move(descriptor));
}

CurveHandle SimulationContext::add_curve(CurveDescriptor descriptor) {
    std::string name = descriptor.name;
    return m_curves.add(std::move(name), std::move(descriptor));
}

ParticleSystemHandle SimulationContext::add_particles(ParticleSystemDescriptor descriptor) {
    std::string name = descriptor.name;
    return m_particles.add(std::move(name), std::move(descriptor));
}

DeformationHandle SimulationContext::add_deformation(DeformationDescriptor descriptor) {
    std::string name = descriptor.name;
    return m_deformations.add(std::move(name), std::move(descriptor));
}

OverlayHandle SimulationContext::add_overlay(OverlayDescriptor descriptor) {
    std::string name = descriptor.name;
    return m_overlays.add(std::move(name), std::move(descriptor));
}

FieldHandle SimulationContext::add_field(FieldDescriptor descriptor) {
    std::string name = descriptor.name;
    return m_fields.add(std::move(name), std::move(descriptor));
}

HistoryBufferHandle SimulationContext::add_history_buffer(HistoryBufferDescriptor descriptor) {
    std::string name = descriptor.name;
    return m_history_buffers.add(std::move(name), std::move(descriptor));
}

} // namespace ndde::sim
