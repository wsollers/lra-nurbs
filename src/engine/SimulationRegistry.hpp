#pragma once
// engine/SimulationRegistry.hpp
// Engine-owned simulation catalog and factory registry.

#include "engine/SimulationRuntime.hpp"
#include "memory/Containers.hpp"
#include "memory/MemoryService.hpp"
#include "memory/Unique.hpp"

#include <functional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace ndde {

struct SimulationDescriptor {
    std::string id;
    std::string title;
    std::string summary;
    std::string thumbnail_path;
};

class SimulationRegistry {
public:
    explicit SimulationRegistry(memory::MemoryService& memory) noexcept;

    void add(SimulationDescriptor descriptor, SimulationRuntime::SimulationFactory factory);

    template <class Simulation, class... Args>
    void add_runtime(std::string name, Args&&... args) {
        SimulationDescriptor descriptor{
            .id = name,
            .title = name
        };
        add_runtime<Simulation>(std::move(descriptor), std::forward<Args>(args)...);
    }

    template <class Simulation, class... Args>
    void add_runtime(SimulationDescriptor descriptor, Args&&... args) {
        add(std::move(descriptor),
            [args_tuple = std::tuple<std::decay_t<Args>...>(std::forward<Args>(args)...)]
            (memory::MemoryService& memory) mutable -> memory::Unique<ISimulation> {
                return std::apply([&memory](auto&&... unpacked) -> memory::Unique<ISimulation> {
                    if constexpr (std::is_constructible_v<Simulation, memory::MemoryService*, decltype(unpacked)...>) {
                        return memory.simulation().make_unique_as<ISimulation, Simulation>(
                            &memory, std::forward<decltype(unpacked)>(unpacked)...);
                    } else {
                        return memory.simulation().make_unique_as<ISimulation, Simulation>(
                            std::forward<decltype(unpacked)>(unpacked)...);
                    }
                }, args_tuple);
            });
    }

    [[nodiscard]] std::size_t size() const noexcept { return m_entries.size(); }
    [[nodiscard]] SimulationRuntime* get(std::size_t index) noexcept;
    [[nodiscard]] const SimulationRuntime* get(std::size_t index) const noexcept;
    [[nodiscard]] const SimulationDescriptor* descriptor(std::size_t index) const noexcept;

private:
    struct Entry {
        SimulationDescriptor descriptor;
        memory::Unique<SimulationRuntime> runtime;
    };

    memory::MemoryService& m_memory;
    memory::PersistentVector<Entry> m_entries;
};

} // namespace ndde
