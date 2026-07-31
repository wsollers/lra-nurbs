#pragma once
// simulation/context/WorkbenchBuildContext.hpp
// Assembly API exposed to workbenches.

#include "simulation/context/SimulationContext.hpp"

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ndde::sim {

class WorkbenchBuildContext {
public:
    void clear() {
        m_contexts.clear();
        m_name_index.clear();
    }

    [[nodiscard]] SimulationContextHandle add_simulation(SimulationContextDescriptor descriptor) {
        const u32 index = static_cast<u32>(m_contexts.size());
        const u32 generation = ++m_next_generation;
        SimulationContextHandle handle{.index = index, .generation = generation};
        m_contexts.push_back(ContextSlot{
            .handle = handle,
            .context = SimulationContext(std::move(descriptor)),
            .active = true
        });
        const std::string& name = m_contexts.back().context.descriptor().name;
        if (!name.empty()) {
            m_name_index[name] = handle;
        }
        return handle;
    }

    [[nodiscard]] SimulationContext* context(SimulationContextHandle handle) noexcept {
        ContextSlot* slot = slot_for(handle);
        return slot ? &slot->context : nullptr;
    }

    [[nodiscard]] const SimulationContext* context(SimulationContextHandle handle) const noexcept {
        const ContextSlot* slot = slot_for(handle);
        return slot ? &slot->context : nullptr;
    }

    [[nodiscard]] SurfaceHandle add_surface(SimulationContextHandle sim, SurfaceDescriptor descriptor) {
        if (SimulationContext* target = context(sim)) return target->add_surface(std::move(descriptor));
        return {};
    }

    [[nodiscard]] CurveHandle add_curve(SimulationContextHandle sim, CurveDescriptor descriptor) {
        if (SimulationContext* target = context(sim)) return target->add_curve(std::move(descriptor));
        return {};
    }

    [[nodiscard]] ParticleSystemHandle add_particles(SimulationContextHandle sim, ParticleSystemDescriptor descriptor) {
        if (SimulationContext* target = context(sim)) return target->add_particles(std::move(descriptor));
        return {};
    }

    [[nodiscard]] DeformationHandle add_deformation(SimulationContextHandle sim, DeformationDescriptor descriptor) {
        if (SimulationContext* target = context(sim)) return target->add_deformation(std::move(descriptor));
        return {};
    }

    [[nodiscard]] OverlayHandle add_overlay(SimulationContextHandle sim, OverlayDescriptor descriptor) {
        if (SimulationContext* target = context(sim)) return target->add_overlay(std::move(descriptor));
        return {};
    }

    [[nodiscard]] FieldHandle add_field(SimulationContextHandle sim, FieldDescriptor descriptor) {
        if (SimulationContext* target = context(sim)) return target->add_field(std::move(descriptor));
        return {};
    }

    [[nodiscard]] HistoryBufferHandle add_history_buffer(SimulationContextHandle sim,
                                                         HistoryBufferDescriptor descriptor) {
        if (SimulationContext* target = context(sim)) return target->add_history_buffer(std::move(descriptor));
        return {};
    }

    template <class Fn>
    void for_each_simulation(Fn&& fn) const {
        for (const ContextSlot& slot : m_contexts) {
            if (slot.active) {
                fn(slot.handle, slot.context);
            }
        }
    }

    [[nodiscard]] std::size_t simulation_count() const noexcept { return m_contexts.size(); }

private:
    struct ContextSlot {
        SimulationContextHandle handle;
        SimulationContext context;
        bool active = false;
    };

    std::vector<ContextSlot> m_contexts;
    std::unordered_map<std::string, SimulationContextHandle> m_name_index;
    u32 m_next_generation = 0u;

    [[nodiscard]] ContextSlot* slot_for(SimulationContextHandle handle) noexcept {
        if (!handle || handle.index >= m_contexts.size()) return nullptr;
        ContextSlot& slot = m_contexts[handle.index];
        if (!slot.active || slot.handle.generation != handle.generation) return nullptr;
        return &slot;
    }

    [[nodiscard]] const ContextSlot* slot_for(SimulationContextHandle handle) const noexcept {
        if (!handle || handle.index >= m_contexts.size()) return nullptr;
        const ContextSlot& slot = m_contexts[handle.index];
        if (!slot.active || slot.handle.generation != handle.generation) return nullptr;
        return &slot;
    }
};

} // namespace ndde::sim
