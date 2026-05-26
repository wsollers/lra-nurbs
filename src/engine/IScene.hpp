#pragma once
// engine/IScene.hpp
// IScene: the minimal contract between Engine and any scene.
//
// Design
// ──────
// Engine knows nothing about surfaces, particles, panels, or simulation.
// It knows only that a scene can:
//   - advance and render itself each frame  (on_frame)
//   - be notified when the swapchain resizes (on_resize)
//   - report a human-readable name          (name)
//
// Panels, hotkeys, and UI layout are the scene's private concern.
// The engine does not iterate panels or manage windows.
//
// Lifetime contract
// ─────────────────
// Engine owns the active scene via unique_ptr<IScene>.
// Switching scenes: scenes request a switch through EngineAPI. Engine defers
// the actual replacement until the current frame has fully ended. All Vulkan
// work is flushed before destruction.
//
// Legacy note: this interface is retained while older scene components are
// migrated.  New simulation runtime code uses ISimulation.

#include "math/Scalars.hpp"  // f32
#include "memory/Containers.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace ndde {

struct ParticleSnapshot {
    std::uint64_t id = 0;
    std::string role;
    std::string label;
    float u = 0.f;
    float v = 0.f;
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
};

struct SceneSnapshot {
    std::string name;
    bool paused = false;
    float sim_time = 0.f;
    float sim_speed = 0.f;
    std::size_t particle_count = 0;
    std::string status;
    memory::PersistentVector<ParticleSnapshot> particles;
};

class IScene {
public:
    virtual ~IScene() = default;

    // Called once per frame after ImGui::NewFrame() and before the engine
    // freezes ImGui draw data for renderer-thread command recording.
    // dt is wall-clock seconds since the previous frame.
    // The scene is responsible for all ImGui window calls this frame.
    virtual void on_frame(f32 dt) = 0;

    // Called when the primary swapchain has been recreated.
    // The scene should rebuild any projection matrices or viewport state
    // that depends on framebuffer dimensions.
    // Default: no-op (most scenes recompute projections every frame anyway).
    virtual void on_resize(u32 /*w*/, u32 /*h*/) {}

    // Short human-readable identifier used in the scene selector UI.
    [[nodiscard]] virtual std::string_view name() const = 0;

    [[nodiscard]] virtual SceneSnapshot snapshot() const {
        return SceneSnapshot{
            .name = std::string(name()),
            .paused = paused(),
            .status = "Scene"
        };
    }

    // Simulation controls. Scenes that own simulation state override these;
    // static/read-only scenes can keep the defaults.
    virtual void set_paused(bool /*paused*/) {}
    [[nodiscard]] virtual bool paused() const noexcept { return false; }

    // Raw key events forwarded by Engine from GLFW after ImGui receives them.
    // Parameters are GLFW-style integer key/action/modifier values without
    // requiring this interface to include GLFW headers.
    virtual void on_key_event(int /*key*/, int /*action*/, int /*mods*/) {}

protected:
    IScene() = default;
    IScene(const IScene&) = default;
    IScene& operator=(const IScene&) = default;
    IScene(IScene&&) = default;
    IScene& operator=(IScene&&) = default;
};

} // namespace ndde
