// math/Surfaces.cpp
#include "math/Surfaces.hpp"
#include "numeric/ops.hpp"
#include <glm/glm.hpp>
#include <cmath>
#include <stdexcept>

namespace ndde::math {

// ── ISurface base ─────────────────────────────────────────────────────────────

SurfaceMetadata ISurface::metadata(float t) const {
    return SurfaceMetadata{
        .name = "Generic Surface",
        .formula = "p(u,v)",
        .domain = SurfaceDomainInfo{
            .u_min = u_min(t),
            .u_max = u_max(t),
            .v_min = v_min(t),
            .v_max = v_max(t)
        },
        .has_analytic_derivatives = false,
        .deformable = false,
        .time_varying = is_time_varying()
    };
}

Vec3 ISurface::du(float u, float v, float t) const {
    constexpr float h = 1e-4f;
    return (evaluate(u + h, v, t) - evaluate(u - h, v, t)) / (2.f * h);
}

Vec3 ISurface::dv(float u, float v, float t) const {
    constexpr float h = 1e-4f;
    return (evaluate(u, v + h, t) - evaluate(u, v - h, t)) / (2.f * h);
}

Vec3 ISurface::unit_normal(float u, float v, float t) const {
    const Vec3 cross = ops::cross(du(u, v, t), dv(u, v, t));
    const float len  = ops::length(cross);
    return (len > 1e-8f) ? cross / len : Vec3{0.f, 0.f, 1.f};
}

float ISurface::gaussian_curvature(float u, float v, float t) const {
    const Vec3 fu = du(u, v, t);
    const Vec3 fv = dv(u, v, t);
    const float E = ops::dot(fu, fu);
    const float F = ops::dot(fu, fv);
    const float G = ops::dot(fv, fv);

    constexpr float h = 1e-3f;
    const Vec3 fuu = (evaluate(u+h,v,t) - 2.f*evaluate(u,v,t) + evaluate(u-h,v,t)) / (h*h);
    const Vec3 fuv = (evaluate(u+h,v+h,t) - evaluate(u+h,v-h,t)
                    - evaluate(u-h,v+h,t) + evaluate(u-h,v-h,t)) / (4.f*h*h);
    const Vec3 fvv = (evaluate(u,v+h,t) - 2.f*evaluate(u,v,t) + evaluate(u,v-h,t)) / (h*h);
    const Vec3 n   = unit_normal(u, v, t);

    const float L = ops::dot(fuu, n);
    const float M = ops::dot(fuv, n);
    const float N = ops::dot(fvv, n);

    const float denom = E*G - F*F;
    return (ops::abs(denom) < 1e-12f) ? 0.f : (L*N - M*M) / denom;
}

float ISurface::mean_curvature(float u, float v, float t) const {
    const Vec3 fu = du(u, v, t);
    const Vec3 fv = dv(u, v, t);
    const float E = ops::dot(fu, fu);
    const float F = ops::dot(fu, fv);
    const float G = ops::dot(fv, fv);

    constexpr float h = 1e-3f;
    const Vec3 fuu = (evaluate(u+h,v,t) - 2.f*evaluate(u,v,t) + evaluate(u-h,v,t)) / (h*h);
    const Vec3 fuv = (evaluate(u+h,v+h,t) - evaluate(u+h,v-h,t)
                    - evaluate(u-h,v+h,t) + evaluate(u-h,v-h,t)) / (4.f*h*h);
    const Vec3 fvv = (evaluate(u,v+h,t) - 2.f*evaluate(u,v,t) + evaluate(u,v-h,t)) / (h*h);
    const Vec3 n   = unit_normal(u, v, t);

    const float L = ops::dot(fuu, n);
    const float M = ops::dot(fuv, n);
    const float N = ops::dot(fvv, n);

    const float denom = 2.f * (E*G - F*F);
    return (ops::abs(denom) < 1e-12f) ? 0.f : (E*N + G*L - 2.f*F*M) / denom;
}

u32 ISurface::wireframe_vertex_count(u32 u_lines, u32 v_lines) const noexcept {
    return (u_lines + 1u) * v_lines * 2u +
           (v_lines + 1u) * u_lines * 2u;
}

void ISurface::tessellate_wireframe(std::span<Vertex> out,
                                    u32 u_lines, u32 v_lines,
                                    float t, Vec4 color) const
{
    const u32 needed = wireframe_vertex_count(u_lines, v_lines);
    if (out.size() < static_cast<std::size_t>(needed))
        throw std::invalid_argument("[ISurface::tessellate_wireframe] span too small");

    const float u0   = u_min(t), u1 = u_max(t);
    const float v0   = v_min(t), v1 = v_max(t);
    const float du_s = (u1 - u0) / static_cast<float>(u_lines);
    const float dv_s = (v1 - v0) / static_cast<float>(v_lines);

    u32 idx = 0;

    for (u32 i = 0; i <= u_lines; ++i) {
        const float u = u0 + static_cast<float>(i) * du_s;
        for (u32 j = 0; j < v_lines; ++j) {
            const float va = v0 + static_cast<float>(j)   * dv_s;
            const float vb = v0 + static_cast<float>(j+1) * dv_s;
            out[idx++] = Vertex{ evaluate(u, va, t), color };
            out[idx++] = Vertex{ evaluate(u, vb, t), color };
        }
    }

    for (u32 j = 0; j <= v_lines; ++j) {
        const float v = v0 + static_cast<float>(j) * dv_s;
        for (u32 i = 0; i < u_lines; ++i) {
            const float ua = u0 + static_cast<float>(i)   * du_s;
            const float ub = u0 + static_cast<float>(i+1) * du_s;
            out[idx++] = Vertex{ evaluate(ua, v, t), color };
            out[idx++] = Vertex{ evaluate(ub, v, t), color };
        }
    }
}

// ── Paraboloid ────────────────────────────────────────────────────────────────

Paraboloid::Paraboloid(float a, float u_max, float vmin, float vmax)
    : m_a(a), m_umax(u_max), m_vmin(vmin), m_vmax(vmax)
{
    if (a <= 0.f)     throw std::invalid_argument("[Paraboloid] a must be > 0");
    if (u_max <= 0.f) throw std::invalid_argument("[Paraboloid] u_max must be > 0");
}

Vec3 Paraboloid::evaluate(float u, float v, float /*t*/) const {
    return Vec3{ u * ops::cos(v), u * ops::sin(v), m_a * u * u };
}

Vec3 Paraboloid::du(float u, float v, float /*t*/) const {
    return Vec3{ ops::cos(v), ops::sin(v), 2.f * m_a * u };
}

Vec3 Paraboloid::dv(float u, float v, float /*t*/) const {
    return Vec3{ -u * ops::sin(v), u * ops::cos(v), 0.f };
}

Vec3 Paraboloid::unit_normal(float u, float v, float /*t*/) const {
    if (ops::abs(u) < 1e-7f) return Vec3{ 0.f, 0.f, 1.f };
    const float denom = ops::abs(u) * ops::sqrt(1.f + 4.f*m_a*m_a*u*u);
    return Vec3{ -2.f*m_a*u*ops::cos(v), -2.f*m_a*u*ops::sin(v), u } / denom;
}

float Paraboloid::gaussian_curvature(float u, float /*v*/, float /*t*/) const {
    const float base = 1.f + 4.f*m_a*m_a*u*u;
    return 4.f*m_a*m_a / (base * base);
}

float Paraboloid::mean_curvature(float u, float /*v*/, float /*t*/) const {
    const float s = 1.f + 4.f*m_a*m_a*u*u;
    return m_a * (2.f + 4.f*m_a*m_a*u*u) / ops::pow(s, 1.5f);
}

float Paraboloid::kappa1(float u) const noexcept {
    const float s = 1.f + 4.f*m_a*m_a*u*u;
    return 2.f*m_a / ops::pow(s, 1.5f);
}

float Paraboloid::kappa2(float u) const noexcept {
    return 2.f*m_a / ops::sqrt(1.f + 4.f*m_a*m_a*u*u);
}

SurfaceMetadata Paraboloid::metadata(float t) const {
    SurfaceMetadata data = ISurface::metadata(t);
    data.name = "Paraboloid";
    data.formula = "z = a r^2";
    data.has_analytic_derivatives = true;
    data.parameters = {{
        {.name = "a", .value = m_a, .description = "curvature scale"},
        {.name = "u_max", .value = m_umax, .description = "radial domain extent"}
    }};
    data.parameter_count = 2u;
    return data;
}

// ── Torus ─────────────────────────────────────────────────────────────────────

Torus::Torus(float R, float r)
    : m_R(R), m_r(r)
{
    if (R <= 0.f) throw std::invalid_argument("[Torus] R must be > 0");
    if (r <= 0.f) throw std::invalid_argument("[Torus] r must be > 0");
    if (r >= R)   throw std::invalid_argument("[Torus] r must be < R (no self-intersection)");
}

Vec3 Torus::evaluate(float u, float v, float /*t*/) const {
    const float cu = ops::cos(u), su = ops::sin(u);
    const float cv = ops::cos(v), sv = ops::sin(v);
    const float rho = m_R + m_r * cv;
    return Vec3{ rho * cu, rho * su, m_r * sv };
}

Vec3 Torus::du(float u, float v, float /*t*/) const {
    const float rho = m_R + m_r * ops::cos(v);
    return Vec3{ -rho * ops::sin(u), rho * ops::cos(u), 0.f };
}

Vec3 Torus::dv(float u, float v, float /*t*/) const {
    const float sv = ops::sin(v), cv = ops::cos(v);
    const float cu = ops::cos(u), su = ops::sin(u);
    return Vec3{ -m_r * sv * cu, -m_r * sv * su, m_r * cv };
}

Vec3 Torus::unit_normal(float u, float v, float /*t*/) const {
    return Vec3{ ops::cos(v)*ops::cos(u), ops::cos(v)*ops::sin(u), ops::sin(v) };
}

float Torus::gaussian_curvature(float /*u*/, float v, float /*t*/) const {
    const float cv  = ops::cos(v);
    const float rho = m_R + m_r * cv;
    if (ops::abs(rho) < 1e-9f) return 0.f;
    return cv / (m_r * rho);
}

float Torus::mean_curvature(float /*u*/, float v, float /*t*/) const {
    const float cv  = ops::cos(v);
    const float rho = m_R + m_r * cv;
    if (ops::abs(rho) < 1e-9f) return 0.f;
    return (m_R + 2.f * m_r * cv) / (2.f * m_r * rho);
}

SurfaceMetadata Torus::metadata(float t) const {
    SurfaceMetadata data = ISurface::metadata(t);
    data.name = "Torus";
    data.formula = "((R + r cos v) cos u, (R + r cos v) sin u, r sin v)";
    data.has_analytic_derivatives = true;
    data.parameters = {{
        {.name = "R", .value = m_R, .description = "major radius"},
        {.name = "r", .value = m_r, .description = "minor radius"}
    }};
    data.parameter_count = 2u;
    return data;
}

} // namespace ndde::math
