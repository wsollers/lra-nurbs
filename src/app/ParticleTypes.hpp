#pragma once
// app/ParticleTypes.hpp
// Shared particle vocabulary: roles, metadata, and visual trail policy.

#include "memory/Containers.hpp"
#include "math/Scalars.hpp"
#include "math/GeometryTypes.hpp"
#include <glm/glm.hpp>
#include <cstdint>
#include <string>
#include <string_view>

namespace ndde {

using ParticleId = u64;

enum class ParticleRole : u8 {
    Neutral,
    Leader,
    Chaser,
    Avoider
};

[[nodiscard]] inline std::string_view role_name(ParticleRole role) noexcept {
    switch (role) {
        case ParticleRole::Leader: return "Leader";
        case ParticleRole::Chaser: return "Chaser";
        case ParticleRole::Avoider: return "Avoider";
        case ParticleRole::Neutral: default: return "Neutral";
    }
}

enum class TrailMode : u8 {
    None,
    Finite,
    Persistent,
    StaticCurve
};

struct TrailConfig {
    TrailMode mode = TrailMode::Finite;
    u32       max_points = 1200;
    f32     min_spacing = 0.015f;
};

struct TrailSample {
    glm::vec2 uv{0.f, 0.f};
    Vec3 world{0.f, 0.f, 0.f};
    f32 time = 0.f;
};

struct ParticleMetadata {
    std::string              label;
    std::string              role;
    memory::FrameVector<std::string> behaviors;
    memory::FrameVector<std::string> constraints;
    memory::FrameVector<std::string> goals;
};

} // namespace ndde
