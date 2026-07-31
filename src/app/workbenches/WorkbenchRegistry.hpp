#pragma once
// app/workbenches/WorkbenchRegistry.hpp
// Explicit metadata + factory registry for gallery workbenches.

#include "app/workbenches/Workbench.hpp"
#include "memory/Containers.hpp"
#include "memory/MemoryService.hpp"
#include "memory/Unique.hpp"

#include <functional>
#include <memory_resource>
#include <string>
#include <string_view>

namespace ndde {

class WorkbenchRegistry {
public:
    using WorkbenchFactory = std::function<memory::Unique<IWorkbench>(memory::MemoryService&)>;

    explicit WorkbenchRegistry(std::pmr::memory_resource* resource = std::pmr::get_default_resource());

    void add(WorkbenchMetadata metadata, WorkbenchFactory factory);

    [[nodiscard]] std::size_t size() const noexcept { return m_entries.size(); }
    [[nodiscard]] const WorkbenchMetadata* metadata(std::size_t index) const noexcept;
    [[nodiscard]] const WorkbenchMetadata* find_metadata(std::string_view id) const noexcept;
    [[nodiscard]] memory::Unique<IWorkbench> create(memory::MemoryService& memory, std::size_t index);
    [[nodiscard]] memory::Unique<IWorkbench> create(memory::MemoryService& memory, std::string_view id);

private:
    struct Entry {
        WorkbenchMetadata metadata;
        WorkbenchFactory factory;
    };

    memory::PersistentVector<Entry> m_entries;
};

void register_app_workbenches(WorkbenchRegistry& registry);
void register_function_derivative_workbench(WorkbenchRegistry& registry);

} // namespace ndde
