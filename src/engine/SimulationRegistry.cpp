#include "engine/SimulationRegistry.hpp"

#include <utility>

namespace ndde {

SimulationRegistry::SimulationRegistry(memory::MemoryService& memory) noexcept
    : m_memory(memory)
    , m_entries(memory.persistent().resource())
{}

void SimulationRegistry::add(SimulationDescriptor descriptor,
                             SimulationRuntime::SimulationFactory factory) {
    auto runtime = m_memory.persistent().make_unique<SimulationRuntime>(
        descriptor.title,
        std::move(factory));
    m_entries.push_back(Entry{
        .descriptor = std::move(descriptor),
        .runtime = std::move(runtime)
    });
}

SimulationRuntime* SimulationRegistry::get(std::size_t index) noexcept {
    if (index >= m_entries.size()) return nullptr;
    return m_entries[index].runtime.get();
}

const SimulationRuntime* SimulationRegistry::get(std::size_t index) const noexcept {
    if (index >= m_entries.size()) return nullptr;
    return m_entries[index].runtime.get();
}

const SimulationDescriptor* SimulationRegistry::descriptor(std::size_t index) const noexcept {
    if (index >= m_entries.size()) return nullptr;
    return &m_entries[index].descriptor;
}

} // namespace ndde
