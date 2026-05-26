#pragma once

#include "app/MultiWellSpawner.hpp"
#include "app/GoalStatusPanel.hpp"
#include "app/ParticleInspectorPanel.hpp"
#include "app/ParticleSystem.hpp"
#include "app/SimulationControlPanel.hpp"
#include "app/SimulationContext.hpp"
#include "app/SimulationPanelModels.hpp"
#include "app/SurfaceMeshCache.hpp"
#include "app/SurfaceRegistry.hpp"
#include "app/SwarmRecipePanel.hpp"
#include "engine/ISimulation.hpp"
#include "engine/ScopedServiceHandles.hpp"
#include "memory/Unique.hpp"

#include <string_view>

namespace ndde {

class SimulationMultiWell final : public ISimulation {
public:
    explicit SimulationMultiWell(memory::MemoryService* memory = nullptr);
    [[nodiscard]] std::string_view name() const override { return "Multi-Well Centroid"; }
    void on_register(SimulationHost& host) override;
    void on_start() override;
    void on_tick(const TickInfo& tick) override;
    void on_stop() override;
    [[nodiscard]] SceneSnapshot snapshot() const override;
    [[nodiscard]] SimulationMetadata metadata() const override;

    [[nodiscard]] RenderViewId main_view_id() const noexcept { return m_main_view; }
    [[nodiscard]] RenderViewId alternate_view_id() const noexcept { return m_alternate_view; }
    [[nodiscard]] std::size_t particle_count() const noexcept { return m_particles.size(); }

private:
    memory::Unique<MultiWellWaveSurface> m_surface;
    ParticleSystem m_particles;
    u32 m_spawn_count = 0;
    float m_sim_time = 0.f;
    float m_sim_speed = 1.f;
    bool m_paused = false;
    GoalStatus m_goal_status = GoalStatus::Running;
    MultiWellSpawner m_spawner;
    SimulationContext m_context;
    SurfaceMeshCache m_mesh;
    SimulationHost* m_host = nullptr;
    ScopedServiceHandles<PanelHandle> m_panel_handles;
    HotkeyHandle m_avoider_hotkey;
    HotkeyHandle m_centroid_hotkey;
    HotkeyHandle m_reset_hotkey;
    RenderViewHandle m_main_handle;
    RenderViewHandle m_alt_handle;
    RenderViewId m_main_view = 0;
    RenderViewId m_alternate_view = 0;
    SwarmBuildResult m_last_swarm;

    void spawn_avoider();
    void spawn_centroid_seeker();
    void reset_showcase();
    void draw_control_panel();
    void draw_swarm_panel();
    void draw_particle_panel();
    void draw_goal_panel();
    void sync_context();
    void submit_geometry();
};

} // namespace ndde
