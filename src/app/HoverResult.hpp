#pragma once
// app/HoverResult.hpp
// Data produced each frame by curve-hover hit testing.
// Shared hover/snap payload for geometry debug panels and render overlays.

namespace ndde {

struct HoverResult {
    // ── Snap point ────────────────────────────────────────────────────────────
    bool  hit       = false;
    f32 world_x   = 0.f;
    f32 world_y   = 0.f;
    f32 world_z   = 0.f;   ///< Always 0 for 2D curves; used by helix
    int   curve_idx = -1;
    int   snap_idx  = -1;
    f32 snap_t    = 0.f;   ///< Curve parameter at snap point

    // ── Secant (2D) ───────────────────────────────────────────────────────────
    bool  has_secant  = false;
    f32 secant_x0   = 0.f;
    f32 secant_y0   = 0.f;
    f32 secant_x1   = 0.f;
    f32 secant_y1   = 0.f;
    f32 slope       = 0.f;
    f32 intercept   = 0.f;

    // ── Frenet–Serret frame (valid in 2D and 3D) ──────────────────────────────
    // T = unit tangent,  N = unit normal,  B = unit binormal
    // In 2D: B = (0,0,1) always, N lies in the xy-plane
    bool  has_tangent    = false;
    f32 tangent_slope  = 0.f;  ///< dy/dx (2D only; for readout)
    f32 T[3]           = {};   ///< Unit tangent vector
    f32 N[3]           = {};   ///< Unit normal vector (principal normal)
    f32 B[3]           = {};   ///< Unit binormal vector
    f32 kappa          = 0.f;  ///< Curvature κ = |p' × p''| / |p'|³
    f32 tau            = 0.f;  ///< Torsion   τ = (p'×p'')·p''' / |p'×p''|²
    f32 speed          = 0.f;  ///< |p'(t)| — arc-length rate
    f32 osc_radius     = 0.f;  ///< Osculating circle radius = 1/κ (0 if κ=0)
};

} // namespace ndde
