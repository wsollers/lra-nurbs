#pragma once
// app/SimulationPanelModels.hpp
// Lightweight view/control data shared by reusable simulation panels.

#include "engine/SimulationMetadata.hpp"
#include "memory/Containers.hpp"

#include <functional>
#include <string>
#include <string_view>

namespace ndde {

struct SimulationControls {
    bool* paused = nullptr;
    f32* sim_speed = nullptr;
    std::function<void()> reset;
};

struct SimulationCommand {
    std::string name;
    std::string label;
    std::string hotkey;
    std::function<void()> execute;
};

struct SimulationCommandList {
    memory::PersistentVector<SimulationCommand> commands;

    void run(std::string_view name) const {
        for (const SimulationCommand& command : commands) {
            if (command.name == name && command.execute) {
                command.execute();
                return;
            }
        }
    }
};

} // namespace ndde
