#pragma once
// math/Surfaces.hpp
// Parametric surface interface and concrete 2-manifolds.
//
// ISurface: smooth map  p : [u_min,u_max] x [v_min,v_max] -> R^3.
// All geometry methods accept a time parameter t (default 0.f).
// Static surfaces ignore t.  IDeformableSurface (Step 3c) uses it.
//
// Concrete classes:
//   Paraboloid  z = a(x^2+y^2),  polar chart
//   Torus       standard torus   T^2 = S^1 x S^1
//
// Torus geometry:
//   p(u,v) = ((R + r*cos(v))*cos(u), (R + r*cos(v))*sin(u), r*sin(v))
//   u in [0,2pi] periodic (longitude)  v in [0,2pi] periodic (latitude)
//   K = cos(v) / (r*(R + r*cos(v)))  -- positive outer, negative inner

#include "math/Scalars.hpp"
#include "math/GeometryTypes.hpp"
#include <array>
#include <span>
#include <stdexcept>
#include <cmath>
#include <string_view>

namespace ndde::math {

struct SurfaceDomainInfo {
    float u_min = 0.f;
    float u_max = 0.f;
    float v_min = 0.f;
    float v_max = 0.f;
};

struct SurfaceParameterInfo {
    std::string_view name;
    float value = 0.f;
    std::string_view units;
    std::string_view description;
};

struct SurfaceMetadata {
    std::string_view name = "Unnamed Surface";
    std::string_view formula;
    SurfaceDomainInfo domain{};
    bool has_analytic_derivatives = false;
    bool deformable = false;
    bool time_varying = false;
    std::array<SurfaceParameterInfo, 8> parameters{};
    u32 parameter_count = 0;
};

// ── ISurface ──────────────────────────────────────────────────────────────────
// Smooth parametric surface  p : D x R -> R^3.
// The time parameter t defaults to 0.f; static surfaces ignore it.
// IDeformableSurface (Step 3c) overrides methods to use t.

class ISurface {
public:
    virtual ~ISurface() = default;

    // Core map -- time-parameterised.
    [[nodiscard]] virtual Vec3  evaluate(float u, float v, float t = 0.f) const = 0;

    // Domain bounds -- may vary with t for growing/shrinking surfaces.
    [[nodiscard]] virtual float u_min(float t = 0.f) const = 0;
    [[nodiscard]] virtual float u_max(float t = 0.f) const = 0;
    [[nodiscard]] virtual float v_min(float t = 0.f) const = 0;
    [[nodiscard]] virtual float v_max(float t = 0.f) const = 0;

    // Periodicity -- does not change with t.
    [[nodiscard]] virtual bool is_periodic_u() const { return false; }
    [[nodiscard]] virtual bool is_periodic_v() const { return false; }

    // Is the surface time-varying?  Lets the renderer skip retessellation
    // on frames where the surface has not changed.
    [[nodiscard]] virtual bool is_time_varying() const { return false; }

    [[nodiscard]] virtual SurfaceMetadata metadata(float t = 0.f) const;

    // First-order partial derivatives (default: central finite difference).
    // Override with analytic formulas for efficiency and accuracy.
    [[nodiscard]] virtual Vec3 du(float u, float v, float t = 0.f) const;
    [[nodiscard]] virtual Vec3 dv(float u, float v, float t = 0.f) const;

    // Velocity of a surface point: dp/dt (default: zero -- static surface).
    // Used by integrators that account for surface motion.
    [[nodiscard]] virtual Vec3 dt(float u, float v, float t) const {
        (void)u; (void)v; (void)t;
        return Vec3{0.f, 0.f, 0.f};
    }

    // Unit normal  N = normalize(du x dv).
    [[nodiscard]] Vec3 unit_normal(float u, float v, float t = 0.f) const;

    // Gaussian curvature  K = (LN - M^2) / (EG - F^2).
    [[nodiscard]] float gaussian_curvature(float u, float v, float t = 0.f) const;

    // Mean curvature  H = (EN + GL - 2FM) / (2*(EG - F^2)).
    [[nodiscard]] float mean_curvature(float u, float v, float t = 0.f) const;

    // Wireframe tessellation (LineList).
    // t is passed through to evaluate() so deforming surfaces tessellate
    // at the correct geometry for the current frame.
    [[nodiscard]] u32 wireframe_vertex_count(u32 u_lines, u32 v_lines) const noexcept;

    void tessellate_wireframe(std::span<Vertex> out,
                              u32 u_lines, u32 v_lines,
                              float t     = 0.f,
                              Vec4  color = { 1.f, 1.f, 1.f, 1.f }) const;

protected:
    ISurface() = default;
    ISurface(const ISurface&) = default;
    ISurface& operator=(const ISurface&) = default;
    ISurface(ISurface&&) = default;
    ISurface& operator=(ISurface&&) = default;
};

// ── Paraboloid ────────────────────────────────────────────────────────────────
// z = a*(x^2 + y^2), polar parameterisation: p(u,v) = (u*cos(v), u*sin(v), a*u^2)
// u in [0, u_max]  v in [0, 2*pi]
// Static surface: t parameter ignored throughout.

class Paraboloid final : public ISurface {
public:
    explicit Paraboloid(float a     = 1.f,
                        float u_max = 1.5f,
                        float vmin  = 0.f,
                        float vmax  = 6.2831853f);

    [[nodiscard]] Vec3  evaluate(float u, float v, float t = 0.f) const override;
    [[nodiscard]] Vec3  du(float u, float v, float t = 0.f)       const override;
    [[nodiscard]] Vec3  dv(float u, float v, float t = 0.f)       const override;

    [[nodiscard]] float u_min(float = 0.f) const override { return 0.f;    }
    [[nodiscard]] float u_max(float = 0.f) const override { return m_umax; }
    [[nodiscard]] float v_min(float = 0.f) const override { return m_vmin; }
    [[nodiscard]] float v_max(float = 0.f) const override { return m_vmax; }

    [[nodiscard]] float a() const noexcept { return m_a; }
    [[nodiscard]] SurfaceMetadata metadata(float t = 0.f) const override;

    // Analytic overrides
    [[nodiscard]] Vec3  unit_normal(float u, float v, float t = 0.f) const;
    [[nodiscard]] float gaussian_curvature(float u, float v, float t = 0.f) const;
    [[nodiscard]] float mean_curvature(float u, float v, float t = 0.f)     const;

    [[nodiscard]] float kappa1(float u) const noexcept;
    [[nodiscard]] float kappa2(float u) const noexcept;

private:
    float m_a, m_umax, m_vmin, m_vmax;
};

// ── Torus ─────────────────────────────────────────────────────────────────────
// Standard torus T^2 = S^1 x S^1.
// p(u,v) = ((R+r*cos(v))*cos(u), (R+r*cos(v))*sin(u), r*sin(v))
// u in [0,2pi]  v in [0,2pi]  both periodic.
// R = major radius, r = minor radius (R > r > 0).
//
// Differential geometry:
//   E = (R+r*cos(v))^2   F = 0   G = r^2        (first fundamental form)
//   K = cos(v)/(r*(R+r*cos(v)))                  (Gaussian curvature)
//   H = (R+2*r*cos(v))/(2*r*(R+r*cos(v)))        (mean curvature)
//
// Static surface: t parameter ignored throughout.

class Torus final : public ISurface {
public:
    explicit Torus(float R = 2.f, float r = 0.7f);

    [[nodiscard]] Vec3  evaluate(float u, float v, float t = 0.f) const override;
    [[nodiscard]] Vec3  du(float u, float v, float t = 0.f)       const override;
    [[nodiscard]] Vec3  dv(float u, float v, float t = 0.f)       const override;

    [[nodiscard]] float u_min(float = 0.f) const override { return 0.f;       }
    [[nodiscard]] float u_max(float = 0.f) const override { return k_two_pi;  }
    [[nodiscard]] float v_min(float = 0.f) const override { return 0.f;       }
    [[nodiscard]] float v_max(float = 0.f) const override { return k_two_pi;  }

    [[nodiscard]] bool is_periodic_u() const override { return true; }
    [[nodiscard]] bool is_periodic_v() const override { return true; }

    [[nodiscard]] float R() const noexcept { return m_R; }
    [[nodiscard]] float r() const noexcept { return m_r; }
    [[nodiscard]] SurfaceMetadata metadata(float t = 0.f) const override;

    [[nodiscard]] Vec3  unit_normal(float u, float v, float t = 0.f)        const;
    [[nodiscard]] float gaussian_curvature(float u, float v, float t = 0.f) const;
    [[nodiscard]] float mean_curvature(float u, float v, float t = 0.f)     const;

private:
    float m_R, m_r;
    static constexpr float k_two_pi = 6.2831853071795864f;
};

} // namespace ndde::math

// ── IDeformableSurface ────────────────────────────────────────────────────────
// Base for surfaces whose geometry evolves with simulation time.
//
// Geometric intuition:
//   A deformable surface is a smooth family of embeddings p(u,v,t).
//   At each instant t it is a valid 2-manifold.
//   The surface velocity dp/dt (ISurface::dt) drives inertial effects
//   on particles walking on a moving surface.
//
// Usage contract (enforced by the scene simulation loop):
//   1. advance(dt) is called ONCE per frame BEFORE particle integration.
//   2. All geometry queries use m_time as the t argument.
//   3. is_time_varying() returns true -- wireframe retessellates every frame.

namespace ndde::math {

class IDeformableSurface : public ISurface {
public:
    [[nodiscard]] bool is_time_varying() const override { return true; }
    [[nodiscard]] float time() const noexcept { return m_time; }
    [[nodiscard]] SurfaceMetadata metadata(float t = 0.f) const override {
        SurfaceMetadata data = ISurface::metadata(t);
        data.deformable = true;
        data.time_varying = true;
        return data;
    }

    // Tick the surface clock.  Override for PDE-driven surfaces.
    virtual void advance(float dt) { m_time += dt; }
    virtual void reset()           { m_time = 0.f; }

protected:
    float m_time = 0.f;
};

} // namespace ndde::math
// GaussianRipple is declared in app/GaussianRipple.hpp (Step 3c)
// to avoid the circular dependency: Surfaces.hpp <-> GaussianSurface.hpp.
