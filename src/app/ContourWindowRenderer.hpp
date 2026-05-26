#pragma once
// app/ContourWindowRenderer.hpp
// Shared submission path for the second-window contour map.

#include "app/ParticleSystem.hpp"
#include "app/SurfaceMeshCache.hpp"
#include "engine/EngineAPI.hpp"
#include "engine/RenderSubmission.hpp"
#include "memory/Containers.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

namespace ndde {

struct ContourWindowOptions {
    f32 extent = 4.f;
    bool draw_wire = false;
    Vec4 wire_color{0.95f, 0.95f, 1.f, 0.32f};
    f32 trail_alpha_floor = 0.70f;
    bool draw_heads = false;
    f32 head_radius = 0.055f;
};

inline void submit_contour_window(EngineAPI& api,
                                  const SurfaceMeshCache& mesh,
                                  const ParticleSystem& particles,
                                  const ContourWindowOptions& options)
{
    const Vec2 sz = api.viewport_size2();
    if (sz.x <= 0.f || sz.y <= 0.f || mesh.contour_count() == 0) return;

    const f32 aspect = sz.x / std::max(sz.y, 1.f);
    const Mat4 mvp = aspect >= 1.f
        ? glm::ortho(-options.extent * aspect, options.extent * aspect, -options.extent, options.extent, -1.f, 1.f)
        : glm::ortho(-options.extent, options.extent, -options.extent / aspect, options.extent / aspect, -1.f, 1.f);

    submit_transformed_vertices(api, RenderTarget::Contour2D, mesh.contour_vertices(), mesh.contour_count(),
        Topology::TriangleList, DrawMode::VertexColor, {1,1,1,1}, mvp,
        [](Vertex& vertex, u32) {
            vertex.pos.z = 0.f;
        });

    if (options.draw_wire && mesh.wire_count() > 0) {
        submit_transformed_vertices(api, RenderTarget::Contour2D, mesh.wire_vertices(), mesh.wire_count(),
            Topology::LineList, DrawMode::VertexColor, {1,1,1,1}, mvp,
            [&options](Vertex& vertex, u32) {
                vertex.pos.z = 0.f;
                vertex.color = options.wire_color;
            });
    }

    for (const Particle& particle : particles.particles()) {
        const u32 n = particle.trail_vertex_count();
        if (n < 2) continue;

        memory::FrameVector<Vertex> trail_vertices(n);
        particle.tessellate_trail({trail_vertices.data(), n});
        submit_transformed_vertices(api, RenderTarget::Contour2D, trail_vertices, n,
            Topology::LineStrip, DrawMode::VertexColor, {1,1,1,1}, mvp,
            [&options](Vertex& vertex, u32) {
                vertex.pos.z = 0.f;
                vertex.color.a = std::max(vertex.color.a, options.trail_alpha_floor);
            });

        if (options.draw_heads) {
            const glm::vec2 uv = particle.head_uv();
            const Vec4 col = particle.head_colour();
            const f32 r = options.head_radius;
            submit_generated_vertices(api, RenderTarget::Contour2D, 4u,
                Topology::LineList, DrawMode::VertexColor, {1,1,1,1}, mvp,
                [uv, col, r](Vertex& out, u32 i) {
                    switch (i) {
                        case 0: out = {{uv.x - r, uv.y, 0.f}, col}; break;
                        case 1: out = {{uv.x + r, uv.y, 0.f}, col}; break;
                        case 2: out = {{uv.x, uv.y - r, 0.f}, col}; break;
                        default: out = {{uv.x, uv.y + r, 0.f}, col}; break;
                    }
                });
        }
    }
}

} // namespace ndde
