#pragma once
// engine/PrimitiveRenderer.hpp
// Domain-neutral draw helpers over EngineAPI transient vertex submission.

#include "engine/EngineAPI.hpp"

#include <algorithm>
#include <span>

namespace ndde {

class PrimitiveRenderer {
public:
    explicit PrimitiveRenderer(EngineAPI& api) noexcept
        : m_api(api)
    {}

    template <class WriteVertex>
    void generated(RenderTarget target,
                   u32 vertex_count,
                   Topology topology,
                   DrawMode mode,
                   Vec4 color,
                   Mat4 mvp,
                   WriteVertex&& write_vertex) const
    {
        if (vertex_count == 0) return;
        auto slice = m_api.acquire(vertex_count);
        for (u32 i = 0; i < vertex_count; ++i)
            write_vertex(slice.vertices()[i], i);
        m_api.submit_to(target, slice, topology, mode, color, mvp);
    }

    void vertices(RenderTarget target,
                  std::span<const Vertex> source,
                  u32 vertex_count,
                  Topology topology,
                  DrawMode mode,
                  Vec4 color,
                  Mat4 mvp) const
    {
        const u32 count = std::min(vertex_count, static_cast<u32>(source.size()));
        generated(target, count, topology, mode, color, mvp,
            [&source](Vertex& out, u32 i) {
                out = source[i];
            });
    }

    void line(RenderTarget target,
              Vec3 a,
              Vec3 b,
              Vec4 color,
              Mat4 mvp,
              DrawMode mode = DrawMode::VertexColor) const
    {
        generated(target, 2, Topology::LineList, mode, color, mvp,
            [=](Vertex& out, u32 i) {
                out = Vertex{ i == 0 ? a : b, color };
            });
    }

    void point(RenderTarget target,
               Vec3 p,
               Vec4 color,
               Mat4 mvp,
               DrawMode mode = DrawMode::UniformColor) const
    {
        generated(target, 1, Topology::LineStrip, mode, color, mvp,
            [=](Vertex& out, u32) {
                out = Vertex{ p, color };
            });
    }

private:
    EngineAPI& m_api;
};

} // namespace ndde
