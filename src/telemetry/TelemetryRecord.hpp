#pragma once
// telemetry/TelemetryRecord.hpp
// POD records written into the SPSC ring buffer every simulation tick.
//
// ── Packed particle identity ──────────────────────────────────────────────────
// TelemetryRecord stores no separate role field. Role is encoded into the
// high-order 4 bits of packed_id:
//
//   bit 63 ─────────────────────────────────── bit 0
//   [ 4-bit role ][ 60-bit raw particle id ]
//    63..60         59..0
//
//   ROLE_SHIFT = 60
//   ROLE_MASK  = u64(0xF) << 60             = 0xF000'0000'0000'0000
//   ID_MASK    = ~ROLE_MASK                  = 0x0FFF'FFFF'FFFF'FFFF
//
// Encoding:   packed_id = (u64(role) << ROLE_SHIFT) | (raw_id & ID_MASK)
// Decoding:   role   = u8(packed_id >> ROLE_SHIFT)
//             raw_id = packed_id & ID_MASK
//
// 60-bit id space: 1,152,921,504,606,846,975 unique particles — sufficient.
// 4-bit role space: 16 roles — matches current ParticleRole enum with headroom.
//
// Python decode (one line):
//   df['role']   = (df['packed_id'] >> 60).astype('uint8')
//   df['raw_id'] = df['packed_id']  & 0x0FFFFFFFFFFFFFFF
//
// ── Layout — TelemetryRecord, 64 bytes, 8-byte aligned ───────────────────────
//
//  offset  field         type  bytes  notes
//  ──────  ─────         ────  ─────  ─────────────────────────────────────────
//   0      tick          u64     8    frame counter since sim start
//   8      sim_time      f32     4    simulated seconds since sim start
//  12      wall_ms       f32     4    wall-clock ms since sim start
//  16      packed_id     u64     8    role(4b) | raw_particle_id(60b)
//  24      noise_sigma   f32     4    diffusion coefficient magnitude σ
//                                     0.f for fully deterministic particles
//                                     not the Wiener increment — the coefficient
//  28      speed         f32     4    parameter-space speed ||(du/dt, dv/dt)||
//  32      u             f32     4    surface parameter u ∈ [0,1)
//  36      v             f32     4    surface parameter v ∈ [0,1)
//  40      x             f32     4    world position x
//  44      y             f32     4    world position y
//  48      z             f32     4    world position z
//  52      angle         f32     4    heading, radians — ParticleState::angle
//                                     finite-diff gives angular velocity in Python
//  56      geodesic_k    f32     4    geodesic curvature κ_g — not derivable
//                                     post-hoc without the full metric tensor
//  60      metric_factor f32     4    RESERVED — write 1.f
//                                     Phase 2: local conformal factor (1 + εh)
//                                     when dynamic metric perturbations are active
//  ──────────────────────────────────────────────────────────────────────────────
//  TOTAL                        64   one cache line, 8-byte multiple ✓
//
// ── Layout — TelemetryExtRecord, 32 bytes, 8-byte aligned ────────────────────
// Sparse supplement emitted only for Chaser/Avoider particles when a target
// is active. Records the DELAYED target UV that the pursuer was reacting to.
// Join to TelemetryRecord on (tick, particle_id == pursuer_packed_id).
//
//  offset  field             type  bytes
//  ──────  ─────             ────  ─────
//   0      tick              u64     8
//   8      pursuer_packed_id u64     8    packed_id of the chasing particle
//  16      target_packed_id  u64     8    packed_id of the target particle
//  24      delay_u           f32     4    target's delayed u (what pursuer saw)
//  28      delay_v           f32     4    target's delayed v
//  ──────────────────────────────────────
//  TOTAL                             32   4-byte multiple, 8-byte aligned ✓

#include "math/Scalars.hpp"
#include "app/ParticleTypes.hpp"

namespace ndde::telemetry {

// ── Packing helpers ───────────────────────────────────────────────────────────

inline constexpr u64 ROLE_SHIFT = u64(60);
inline constexpr u64 ROLE_MASK  = u64(0xF) << ROLE_SHIFT;
inline constexpr u64 ID_MASK    = ~ROLE_MASK;

/// Pack a raw particle id and role into a single u64.
[[nodiscard]] inline constexpr u64 pack_particle_id(ParticleId raw_id,
                                                     ParticleRole role) noexcept {
    return (static_cast<u64>(role) << ROLE_SHIFT) | (raw_id & ID_MASK);
}

/// Extract the raw 60-bit particle id from a packed value.
[[nodiscard]] inline constexpr ParticleId unpack_raw_id(u64 packed) noexcept {
    return packed & ID_MASK;
}

/// Extract the role from a packed value.
[[nodiscard]] inline constexpr ParticleRole unpack_role(u64 packed) noexcept {
    return static_cast<ParticleRole>((packed >> ROLE_SHIFT) & u64(0xF));
}

// ── TelemetryRecord ───────────────────────────────────────────────────────────

struct TelemetryRecord {
    u64 tick          = u64(0);
    f32 sim_time      = f32(0);
    f32 wall_ms       = f32(0);
    u64 packed_id     = u64(0);   ///< role(4b) | raw_particle_id(60b)
    f32 noise_sigma   = f32(0);
    f32 speed         = f32(0);
    f32 u             = f32(0);
    f32 v             = f32(0);
    f32 x             = f32(0);
    f32 y             = f32(0);
    f32 z             = f32(0);
    f32 angle         = f32(0);
    f32 geodesic_k    = f32(0);
    f32 metric_factor = f32(1);   ///< reserved — write 1.f until Phase 2
};

static_assert(sizeof(TelemetryRecord) == 64,
    "TelemetryRecord must be exactly 64 bytes (one cache line). "
    "Recheck field types — every field must use ndde scalar aliases.");

static_assert(alignof(TelemetryRecord) == 8,
    "TelemetryRecord must be 8-byte aligned for SPSC ring correctness.");

static_assert(offsetof(TelemetryRecord, tick)          ==  0);
static_assert(offsetof(TelemetryRecord, sim_time)      ==  8);
static_assert(offsetof(TelemetryRecord, wall_ms)       == 12);
static_assert(offsetof(TelemetryRecord, packed_id)     == 16);
static_assert(offsetof(TelemetryRecord, noise_sigma)   == 24);
static_assert(offsetof(TelemetryRecord, speed)         == 28);
static_assert(offsetof(TelemetryRecord, u)             == 32);
static_assert(offsetof(TelemetryRecord, v)             == 36);
static_assert(offsetof(TelemetryRecord, x)             == 40);
static_assert(offsetof(TelemetryRecord, y)             == 44);
static_assert(offsetof(TelemetryRecord, z)             == 48);
static_assert(offsetof(TelemetryRecord, angle)         == 52);
static_assert(offsetof(TelemetryRecord, geodesic_k)    == 56);
static_assert(offsetof(TelemetryRecord, metric_factor) == 60);

/// CSV column header — column order matches struct field order exactly.
inline constexpr const char* csv_header =
    "tick,sim_time,wall_ms,packed_id,"
    "noise_sigma,speed,u,v,x,y,z,angle,geodesic_k,metric_factor\n";

// ── TelemetryExtRecord ────────────────────────────────────────────────────────

struct TelemetryExtRecord {
    u64 tick              = u64(0);
    u64 pursuer_packed_id = u64(0);  ///< pack_particle_id(pursuer)
    u64 target_packed_id  = u64(0);  ///< pack_particle_id(target)
    f32 delay_u           = f32(0);  ///< target's delayed u — what pursuer saw
    f32 delay_v           = f32(0);  ///< target's delayed v
};

static_assert(sizeof(TelemetryExtRecord) == 32,
    "TelemetryExtRecord must be exactly 32 bytes.");

static_assert(alignof(TelemetryExtRecord) == 8,
    "TelemetryExtRecord must be 8-byte aligned.");

static_assert(offsetof(TelemetryExtRecord, tick)              ==  0);
static_assert(offsetof(TelemetryExtRecord, pursuer_packed_id) ==  8);
static_assert(offsetof(TelemetryExtRecord, target_packed_id)  == 16);
static_assert(offsetof(TelemetryExtRecord, delay_u)           == 24);
static_assert(offsetof(TelemetryExtRecord, delay_v)           == 28);

/// CSV column header for the ext file.
inline constexpr const char* ext_csv_header =
    "tick,pursuer_packed_id,target_packed_id,delay_u,delay_v\n";

} // namespace ndde::telemetry
