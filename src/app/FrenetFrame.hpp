#pragma once
// app/FrenetFrame.hpp
// FrenetFrame, SurfaceFrame, and make_surface_frame.
//
// Moved from GaussianSurface.hpp (B1 refactor) so that AnimatedCurve.hpp
// and particle overlay helpers can include these types without dragging in the
// full GaussianSurface definition.
//
// Mathematical context
// ────────────────────
// FrenetFrame: the moving frame {T, N, B} of a space curve p(s).
//   T = p'(s) / |p'(s)|             unit tangent
//   N = T'(s) / |T'(s)|             principal normal (toward centre of curvature)
//   B = T × N                        binormal (normal to osculating plane)
//   kappa = |T'(s)|                  curvature (reciprocal of osculating radius)
//   tau   = -(B' · N)               torsion  (rate of osculating-plane rotation)
//
// SurfaceFrame: the coordinate tangent frame of a graph surface p(u,v) at a
// foot-point, together with the first fundamental form coefficients E, F, G
// and (optionally) the normal and geodesic curvatures of the traced curve.
//
// make_surface_frame: computes SurfaceFrame from any ISurface& at (u, v, t).

#include "math/Scalars.hpp"
#include "math/Surfaces.hpp"
#include <glm/glm.hpp>

namespace ndde {

// == FrenetFrame ==============================================================

struct FrenetFrame {
    Vec3 T = {1.f, 0.f, 0.f};  ///< unit tangent
    Vec3 N = {0.f, 1.f, 0.f};  ///< principal normal
    Vec3 B = {0.f, 0.f, 1.f};  ///< binormal
    f32  kappa = 0.f;            ///< curvature kappa
    f32  tau   = 0.f;            ///< torsion tau
    f32  speed = 0.f;            ///< |p'(t)|
};

// == SurfaceFrame =============================================================
// Coordinate tangent frame of p(u,v) at a foot-point.
//
//   Dx = dp/du   |Dx|^2 = E
//   Dy = dp/dv   |Dy|^2 = G     Dx.Dy = F
//   n  = unit surface normal
//
// With FrenetFrame:
//   kappa_n = kappa*(N.n)              normal curvature   (surface property)
//   kappa_g = kappa*|N - (N.n)n|      geodesic curvature (curve-on-surface)
//   kappa^2 = kappa_n^2 + kappa_g^2   Meusnier-Pythagorean identity

struct SurfaceFrame {
    Vec3 Dx;              ///< dp/du -- unnormalized
    Vec3 Dy;              ///< dp/dv -- unnormalized
    Vec3 normal;          ///< unit surface normal
    Vec3 geodesic_normal; ///< T x normal, tangent-plane normal to heading
    f32  E = 0.f;         ///< |Dx|^2
    f32  F = 0.f;         ///< Dx.Dy
    f32  G = 0.f;         ///< |Dy|^2
    f32  kappa_n = 0.f;   ///< normal curvature in T-direction
    f32  kappa_g = 0.f;   ///< geodesic curvature
};

// make_surface_frame: compute the coordinate tangent frame at parameter (u,v,t).
// Accepts any ISurface& and a simulation time t so that deforming surfaces
// (IDeformableSurface) return geometry at the correct instant.
// t defaults to 0.f so existing call sites without a time argument still compile.
[[nodiscard]] inline SurfaceFrame
make_surface_frame(const ndde::math::ISurface& surface,
                   f32 u, f32 v,
                   f32 t = 0.f,
                   const FrenetFrame* fr = nullptr) noexcept
{
    SurfaceFrame sf;
    sf.Dx     = surface.du(u, v, t);
    sf.Dy     = surface.dv(u, v, t);
    sf.E      = glm::dot(sf.Dx, sf.Dx);
    sf.F      = glm::dot(sf.Dx, sf.Dy);
    sf.G      = glm::dot(sf.Dy, sf.Dy);
    sf.normal = surface.unit_normal(u, v, t);
    if (fr && fr->kappa > 1e-6f) {
        const f32 NdotN = glm::dot(fr->N, sf.normal);
        sf.kappa_n = fr->kappa * NdotN;
        sf.kappa_g = fr->kappa * glm::length(fr->N - NdotN * sf.normal);
        const Vec3 g = glm::cross(fr->T, sf.normal);
        sf.geodesic_normal = glm::length(g) > 1e-6f ? glm::normalize(g) : Vec3{0.f, 0.f, 0.f};
    }
    return sf;
}

} // namespace ndde
