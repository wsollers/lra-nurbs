#pragma once
// engine/RenderSubmission.hpp
// Small renderer-neutral helpers over EngineAPI arena allocation/submission.

#include "engine/EngineAPI.hpp"
#include "engine/PrimitiveRenderer.hpp"

#include <algorithm>
#include <utility>
#include <span>

namespace ndde {

template <class WriteVertex>
inline void submit_generated_vertices(EngineAPI& api,
                                      RenderTarget target,
                                      u32 vertex_count,
                                      Topology topology,
                                      DrawMode mode,
                                      Vec4 color,
                                      Mat4 mvp,
                                      WriteVertex&& write_vertex)
{
    if (vertex_count == 0) return;
    PrimitiveRenderer primitives(api);
    primitives.generated(target, vertex_count, topology, mode, color, mvp,
                         std::forward<WriteVertex>(write_vertex));
}

inline void submit_vertices(EngineAPI& api,
                            RenderTarget target,
                            std::span<const Vertex> vertices,
                            u32 vertex_count,
                            Topology topology,
                            DrawMode mode,
                            Vec4 color,
                            Mat4 mvp)
{
    const u32 count = std::min(vertex_count, static_cast<u32>(vertices.size()));
    PrimitiveRenderer primitives(api);
    primitives.vertices(target, vertices, count, topology, mode, color, mvp);
}

template <class TransformVertex>
inline void submit_transformed_vertices(EngineAPI& api,
                                        RenderTarget target,
                                        std::span<const Vertex> vertices,
                                        u32 vertex_count,
                                        Topology topology,
                                        DrawMode mode,
                                        Vec4 color,
                                        Mat4 mvp,
                                        TransformVertex&& transform_vertex)
{
    const u32 count = std::min(vertex_count, static_cast<u32>(vertices.size()));
    submit_generated_vertices(api, target, count, topology, mode, color, mvp,
        [&vertices, &transform_vertex](Vertex& out, u32 i) {
            out = vertices[i];
            transform_vertex(out, i);
        });
}

} // namespace ndde
