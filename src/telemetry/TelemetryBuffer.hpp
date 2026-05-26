#pragma once
// telemetry/TelemetryBuffer.hpp
// SPSC (single-producer / single-consumer) ring buffer over an externally
// owned byte slab.
//
// ── Threading model ───────────────────────────────────────────────────────────
// Producer: simulation tick thread — calls push(). Wait-free.
// Consumer: main thread at sim-stop, or a dedicated flush thread — calls
//           consume_all(). Lock-free.
//
// The two atomic indices (m_head, m_tail) are placed on separate cache lines
// via alignas(64) to eliminate false sharing between producer and consumer.
//
// ── Memory ownership ──────────────────────────────────────────────────────────
// The buffer does NOT own the slab. The caller (TelemetryService) owns the
// allocation and must guarantee it outlives any push/consume calls.
// attach() / detach() are not thread-safe — call them only before the
// producer and consumer threads have started, or after they have stopped.

#include "telemetry/TelemetryRecord.hpp"
#include "math/Scalars.hpp"
#include <atomic>
#include <span>

namespace ndde::telemetry {

class TelemetryBuffer {
public:
    TelemetryBuffer() = default;
    ~TelemetryBuffer() = default;

    TelemetryBuffer(const TelemetryBuffer&)            = delete;
    TelemetryBuffer& operator=(const TelemetryBuffer&) = delete;
    TelemetryBuffer(TelemetryBuffer&&)                 = delete;
    TelemetryBuffer& operator=(TelemetryBuffer&&)      = delete;

    // ── Setup (not thread-safe) ───────────────────────────────────────────────

    /// Attach the buffer to an external slab.
    /// slab_bytes is rounded down to the nearest multiple of sizeof(TelemetryRecord).
    void attach(void* slab, u64 slab_bytes) noexcept {
        m_data     = static_cast<TelemetryRecord*>(slab);
        m_capacity = slab_bytes / static_cast<u64>(sizeof(TelemetryRecord));
        m_head.store(u64(0), std::memory_order_relaxed);
        m_tail.store(u64(0), std::memory_order_relaxed);
        m_dropped.store(u64(0), std::memory_order_relaxed);
    }

    void detach() noexcept {
        m_data     = nullptr;
        m_capacity = u64(0);
    }

    /// Reset indices without releasing the slab. Call between simulation runs.
    /// Not thread-safe — call only when producer and consumer are idle.
    void reset() noexcept {
        m_head.store(u64(0), std::memory_order_relaxed);
        m_tail.store(u64(0), std::memory_order_relaxed);
        m_dropped.store(u64(0), std::memory_order_relaxed);
    }

    // ── Producer API — wait-free ──────────────────────────────────────────────

    /// Write one record into the ring.
    /// Returns false and increments dropped_count if the buffer is full.
    [[nodiscard]] bool push(const TelemetryRecord& record) noexcept {
        if (!m_data) return false;

        const u64 head = m_head.load(std::memory_order_relaxed);
        const u64 next = (head + u64(1)) % m_capacity;

        if (next == m_tail.load(std::memory_order_acquire)) {
            // Buffer full — drop and count.
            m_dropped.fetch_add(u64(1), std::memory_order_relaxed);
            return false;
        }

        m_data[head] = record;
        m_head.store(next, std::memory_order_release);
        return true;
    }

    // ── Consumer API — lock-free ──────────────────────────────────────────────

    /// Drain all currently available records into out_span.
    /// Returns the number of records written (≤ out_span.size()).
    /// The caller must supply a span at least capacity() elements wide to
    /// guarantee no records are left behind in a single call.
    [[nodiscard]] u64 consume_all(std::span<TelemetryRecord> out_span) noexcept {
        u64 written = u64(0);
        while (written < static_cast<u64>(out_span.size())) {
            const u64 tail = m_tail.load(std::memory_order_relaxed);
            if (tail == m_head.load(std::memory_order_acquire)) break;
            out_span[static_cast<std::size_t>(written++)] = m_data[tail];
            m_tail.store((tail + u64(1)) % m_capacity, std::memory_order_release);
        }
        return written;
    }

    // ── Diagnostics ───────────────────────────────────────────────────────────

    [[nodiscard]] u64  capacity() const noexcept { return m_capacity; }
    [[nodiscard]] u64  dropped()  const noexcept {
        return m_dropped.load(std::memory_order_relaxed);
    }
    [[nodiscard]] bool empty() const noexcept {
        return m_head.load(std::memory_order_acquire)
            == m_tail.load(std::memory_order_acquire);
    }

    /// Approximate occupancy across threads — not exact, fine for stats panels.
    [[nodiscard]] u64 approx_size() const noexcept {
        const u64 h = m_head.load(std::memory_order_relaxed);
        const u64 t = m_tail.load(std::memory_order_relaxed);
        return (h >= t) ? (h - t) : (m_capacity - t + h);
    }

private:
    TelemetryRecord* m_data     = nullptr;
    u64              m_capacity = u64(0);

    // alignas(64) places m_head and m_tail on separate cache lines to prevent
    // false sharing between producer and consumer threads. MSVC C4324 (structure
    // padded due to alignment specifier) is expected and intentional here.
#ifdef _MSC_VER
#   pragma warning(push)
#   pragma warning(disable : 4324)
#endif
    alignas(64) std::atomic<u64> m_head{u64(0)};
    alignas(64) std::atomic<u64> m_tail{u64(0)};
#ifdef _MSC_VER
#   pragma warning(pop)
#endif

    std::atomic<u64> m_dropped{u64(0)};
};

} // namespace ndde::telemetry
