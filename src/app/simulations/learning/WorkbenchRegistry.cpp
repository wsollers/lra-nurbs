#include "app/simulations/learning/WorkbenchRegistry.hpp"

#include <utility>

namespace ndde {

WorkbenchRegistry::WorkbenchRegistry(std::pmr::memory_resource* resource)
    : m_entries(resource)
{}

void WorkbenchRegistry::add(WorkbenchMetadata metadata, WorkbenchFactory factory) {
    m_entries.push_back(Entry{
        .metadata = std::move(metadata),
        .factory = std::move(factory)
    });
}

const WorkbenchMetadata* WorkbenchRegistry::metadata(std::size_t index) const noexcept {
    if (index >= m_entries.size()) return nullptr;
    return &m_entries[index].metadata;
}

const WorkbenchMetadata* WorkbenchRegistry::find_metadata(std::string_view id) const noexcept {
    for (const Entry& entry : m_entries) {
        if (entry.metadata.id == id) return &entry.metadata;
    }
    return nullptr;
}

memory::Unique<IWorkbench> WorkbenchRegistry::create(memory::MemoryService& memory, std::size_t index) {
    if (index >= m_entries.size()) return {};
    return m_entries[index].factory(memory);
}

memory::Unique<IWorkbench> WorkbenchRegistry::create(memory::MemoryService& memory, std::string_view id) {
    for (std::size_t index = 0; index < m_entries.size(); ++index) {
        if (m_entries[index].metadata.id == id) {
            return create(memory, index);
        }
    }
    return {};
}

void register_learning_workbenches(WorkbenchRegistry& registry) {
    register_function_derivative_workbench(registry);
}

} // namespace ndde
