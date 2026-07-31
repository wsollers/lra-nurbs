#pragma once
// simulation/context/SimulationObjectHandles.hpp
// Typed opaque handles for objects assembled into simulation contexts.

#include "math/Scalars.hpp"

#include <cstddef>
#include <functional>

namespace ndde::sim {

template <class Tag>
struct ObjectHandle {
    u32 index = 0;
    u32 generation = 0;

    [[nodiscard]] explicit operator bool() const noexcept { return generation != 0u; }
    [[nodiscard]] bool operator==(const ObjectHandle&) const noexcept = default;
};

struct SimulationContextTag;
struct SurfaceTag;
struct CurveTag;
struct ParticleSystemTag;
struct DeformationTag;
struct OverlayTag;
struct FieldTag;
struct HistoryBufferTag;

using SimulationContextHandle = ObjectHandle<SimulationContextTag>;
using SurfaceHandle = ObjectHandle<SurfaceTag>;
using CurveHandle = ObjectHandle<CurveTag>;
using ParticleSystemHandle = ObjectHandle<ParticleSystemTag>;
using DeformationHandle = ObjectHandle<DeformationTag>;
using OverlayHandle = ObjectHandle<OverlayTag>;
using FieldHandle = ObjectHandle<FieldTag>;
using HistoryBufferHandle = ObjectHandle<HistoryBufferTag>;

} // namespace ndde::sim

namespace std {

template <class Tag>
struct hash<ndde::sim::ObjectHandle<Tag>> {
    [[nodiscard]] std::size_t operator()(ndde::sim::ObjectHandle<Tag> handle) const noexcept {
        return (static_cast<std::size_t>(handle.generation) << 32u) ^
            static_cast<std::size_t>(handle.index);
    }
};

} // namespace std
