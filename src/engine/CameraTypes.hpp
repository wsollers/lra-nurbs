#pragma once
// engine/CameraTypes.hpp
// Renderer-neutral camera state shared by render views and interaction code.

#include "math/GeometryTypes.hpp"
#include "math/Scalars.hpp"

namespace ndde {

enum class CameraProjection : u8 {
    Perspective,
    Orthographic
};

enum class CameraViewProfile : u8 {
    Auto,
    PerspectiveSurface3D,
    Orthographic2D,
    Locked,
    FreeFlight,
    FollowParticle
};

struct CameraState {
    Vec3 target{0.f, 0.f, 0.f};
    f32 yaw = 0.72f;
    f32 pitch = 0.55f;
    f32 zoom = 1.f;
};

enum class CameraPreset : u8 {
    Home,
    Top,
    Front,
    Side
};

} // namespace ndde
