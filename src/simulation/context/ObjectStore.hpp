#pragma once
// simulation/context/ObjectStore.hpp
// Small typed handle store for simulation context assembly.

#include "simulation/context/SimulationObjectHandles.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ndde::sim {

template <class Handle, class T>
class ObjectStore {
public:
    [[nodiscard]] Handle add(std::string name, T object) {
        const u32 index = static_cast<u32>(m_slots.size());
        const u32 generation = ++m_next_generation;
        Handle handle{.index = index, .generation = generation};
        m_slots.push_back(Slot{
            .handle = handle,
            .name = std::move(name),
            .object = std::move(object),
            .active = true
        });
        if (!m_slots.back().name.empty()) {
            m_name_index[m_slots.back().name] = handle;
        }
        return handle;
    }

    [[nodiscard]] T* get(Handle handle) noexcept {
        Slot* slot = slot_for(handle);
        return slot ? &slot->object : nullptr;
    }

    [[nodiscard]] const T* get(Handle handle) const noexcept {
        const Slot* slot = slot_for(handle);
        return slot ? &slot->object : nullptr;
    }

    [[nodiscard]] std::optional<Handle> find(std::string_view name) const {
        const auto it = m_name_index.find(std::string(name));
        if (it == m_name_index.end()) return std::nullopt;
        if (!get(it->second)) return std::nullopt;
        return it->second;
    }

    template <class Fn>
    void for_each(Fn&& fn) const {
        for (const Slot& slot : m_slots) {
            if (slot.active) {
                fn(slot.handle, slot.object);
            }
        }
    }

    [[nodiscard]] std::size_t size() const noexcept { return m_slots.size(); }
    [[nodiscard]] bool empty() const noexcept { return m_slots.empty(); }

private:
    struct Slot {
        Handle handle{};
        std::string name;
        T object;
        bool active = false;
    };

    std::vector<Slot> m_slots;
    std::unordered_map<std::string, Handle> m_name_index;
    u32 m_next_generation = 0u;

    [[nodiscard]] Slot* slot_for(Handle handle) noexcept {
        if (!handle || handle.index >= m_slots.size()) return nullptr;
        Slot& slot = m_slots[handle.index];
        if (!slot.active || slot.handle.generation != handle.generation) return nullptr;
        return &slot;
    }

    [[nodiscard]] const Slot* slot_for(Handle handle) const noexcept {
        if (!handle || handle.index >= m_slots.size()) return nullptr;
        const Slot& slot = m_slots[handle.index];
        if (!slot.active || slot.handle.generation != handle.generation) return nullptr;
        return &slot;
    }
};

} // namespace ndde::sim
