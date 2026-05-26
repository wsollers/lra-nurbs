#pragma once
// telemetry/TelemetryService.hpp
// Engine-owned service. Manages slab lifetime, the SPSC ring, and the writer.
//
// ── Lifecycle ─────────────────────────────────────────────────────────────────
//   Engine::start()              → init() + on_app_started()
//   Engine::switch_simulation()  → on_sim_stopped() + on_sim_started()
//   Engine::~Engine()            → on_app_stopping() + destroy()
//
// ── Per-tick recording ────────────────────────────────────────────────────────
//   Engine::run_frame()          → record_particles() [wait-free push]
//   Every N ticks or sim-stop   → flush()             [consumer drain]
//
// ── Thread safety ─────────────────────────────────────────────────────────────
//   record() / record_particles() — producer; wait-free; safe to call from
//                                   any single thread per simulation.
//   flush()                        — consumer; lock-free; must be called from
//                                   a single consumer thread (main thread).
//   All other methods              — not thread-safe; call from main thread only.

#include "telemetry/TelemetryBuffer.hpp"
#include "telemetry/TelemetryRecord.hpp"
#include "telemetry/TelemetryWriter.hpp"
#include "engine/events/AppEvent.hpp"
#include "engine/events/SimEvent.hpp"
#include "math/Scalars.hpp"
#include <cstddef>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string_view>
#include <vector>

namespace ndde::telemetry {

class TelemetryService {
public:
    TelemetryService() = default;
    ~TelemetryService() { destroy(); }

    TelemetryService(const TelemetryService&)            = delete;
    TelemetryService& operator=(const TelemetryService&) = delete;
    TelemetryService(TelemetryService&&)                 = delete;
    TelemetryService& operator=(TelemetryService&&)      = delete;

    void set_owner_guard(std::function<bool(std::string_view)> guard);

    // ── Lifecycle (called by Engine — main thread) ────────────────────────────

    /// Allocate the ring buffer slab and configure the writer.
    /// buffer_records: ring capacity in records (default ≈ 30 MB at 64 B/record).
    /// Idempotent — safe to call multiple times; re-allocates if size changes.
    void init(u64 buffer_records      = u64(512) * u64(1024),
              std::filesystem::path output_dir = "telemetry");

    /// Release the slab. Called by Engine destructor.
    void destroy() noexcept;

    // ── App/Sim event hooks (main thread) ─────────────────────────────────────

    void on_app_started(const events::AppStarted& e);
    void on_app_stopping(const events::AppStopping& e);

    void on_sim_started(const events::SimStarted& e);
    void on_sim_stopped(const events::SimStopped& e);

    // ── Producer API (simulation tick — wait-free) ────────────────────────────

    /// Record a single primary telemetry row.
    /// Returns false if the ring was full (record dropped — check dropped()).
    [[nodiscard]] bool record(const TelemetryRecord& r) noexcept {
        if (!m_enabled || !m_writer.is_open()) return false;
        m_total_records.fetch_add(u64(1), std::memory_order_relaxed);
        return m_ring.push(r);
    }

    /// Record a single pursuer/target extension row.
    [[nodiscard]] bool record_ext(const TelemetryExtRecord& r) {
        if (!require_owner_thread("TelemetryService::record_ext")) return false;
        if (!m_enabled || !m_writer.is_open()) return false;
        std::scoped_lock lock(m_ext_mutex);
        m_ext_scratch.push_back(r);   // buffered for flush — not via ring
        return true;
    }

    /// Convenience: record all particles from a SceneSnapshot particle list.
    /// Populates tick, sim_time, wall_ms, packed_id, u, v, x, y, z, role.
    /// noise_sigma, angle, geodesic_k default to 0/1 — override via the
    /// on_telemetry_tick ISimulation hook for richer data.
    template <class ParticleSnapshotRange>
    void record_particles(const ParticleSnapshotRange& particles,
                          u64   tick,
                          f32   sim_time,
                          f32   wall_ms);

    // ── Consumer API (main thread) ────────────────────────────────────────────

    /// Drain the ring into the open CSV.
    /// Returns number of primary records flushed.
    u64 flush();

    // ── Diagnostics ───────────────────────────────────────────────────────────

    [[nodiscard]] u64  capacity()      const noexcept { return m_ring.capacity(); }
    [[nodiscard]] u64  approx_queued() const noexcept { return m_ring.approx_size(); }
    [[nodiscard]] u64  total_records() const noexcept {
        return m_total_records.load(std::memory_order_relaxed);
    }
    [[nodiscard]] u64  total_ticks()   const noexcept {
        return m_total_ticks.load(std::memory_order_relaxed);
    }
    [[nodiscard]] u64  dropped()       const noexcept { return m_ring.dropped(); }
    [[nodiscard]] bool enabled()       const noexcept { return m_enabled; }
    void set_enabled(bool e) noexcept { m_enabled = e; }

private:
    bool                         m_enabled = true;
    std::vector<std::byte>       m_slab;            ///< raw backing store for the ring
    TelemetryBuffer              m_ring;
    TelemetryWriter              m_writer;

    std::atomic<u64>             m_total_records{u64(0)};
    std::atomic<u64>             m_total_ticks{u64(0)};

    // Scratch buffers for flush() — allocated once, reused across all calls.
    std::vector<TelemetryRecord>    m_flush_scratch;
    std::vector<TelemetryExtRecord> m_ext_scratch;
    mutable std::mutex m_ext_mutex;
    std::function<bool(std::string_view)> m_owner_guard;

    [[nodiscard]] bool require_owner_thread(std::string_view api_name) const;
};

// ── Template implementation ───────────────────────────────────────────────────

// Map ParticleSnapshot::role string to ParticleRole enum.
// ParticleSnapshot stores role as a human-readable std::string ("Leader",
// "Chaser", etc.) produced by role_name(). We reverse that here so the
// packed_id carries the numeric role without any heap allocation.
inline ParticleRole role_from_string(const std::string& s) noexcept {
    if (s == "Leader")  return ParticleRole::Leader;
    if (s == "Chaser")  return ParticleRole::Chaser;
    if (s == "Avoider") return ParticleRole::Avoider;
    return ParticleRole::Neutral;
}

template <class ParticleSnapshotRange>
void TelemetryService::record_particles(const ParticleSnapshotRange& particles,
                                         u64 tick,
                                         f32 sim_time,
                                         f32 wall_ms) {
    if (!m_enabled || !m_writer.is_open()) return;
    m_total_ticks.fetch_add(u64(1), std::memory_order_relaxed);

    for (const auto& p : particles) {
        TelemetryRecord r;
        r.tick          = tick;
        r.sim_time      = sim_time;
        r.wall_ms       = wall_ms;
        r.packed_id     = pack_particle_id(p.id, role_from_string(p.role));
        r.noise_sigma   = f32(0);    // populated by on_telemetry_tick override
        r.speed         = f32(0);    // populated by on_telemetry_tick override
        r.u             = p.u;
        r.v             = p.v;
        r.x             = p.x;
        r.y             = p.y;
        r.z             = p.z;
        r.angle         = f32(0);    // populated by on_telemetry_tick override
        r.geodesic_k    = f32(0);    // populated by on_telemetry_tick override
        r.metric_factor = f32(1);    // Phase 2 — conformal factor placeholder

        m_total_records.fetch_add(u64(1), std::memory_order_relaxed);
        // push() is [[nodiscard]] — explicitly discard; ring tracks drop count.
        (void)m_ring.push(r);
    }
}

} // namespace ndde::telemetry
