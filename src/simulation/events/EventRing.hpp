#pragma once
// simulation/events/EventRing.hpp
// SPSC lock-free ring buffer over an externally owned byte slab.
// Producer: simulation tick — push() is wait-free (one atomic store).
// Consumer: drain() on the main thread — lock-free.
// Identical pattern to TelemetryBuffer, proven in the existing codebase.

#include "simulation/events/EventRecord.hpp"
#include "math/Scalars.hpp"
#include <atomic>
#include <span>

namespace ndde::events {

class EventRing {
public:
    EventRing() = default;
    ~EventRing() = default;

    EventRing(const EventRing&)            = delete;
    EventRing& operator=(const EventRing&) = delete;
    EventRing(EventRing&&)                 = delete;
    EventRing& operator=(EventRing&&)      = delete;

    // Attach to an external slab. Not thread-safe — call before any push/consume.
    void attach(void* slab, u64 slab_bytes) noexcept {
        m_data     = static_cast<EventRecord*>(slab);
        m_capacity = slab_bytes / static_cast<u64>(sizeof(EventRecord));
        m_head.store(u64(0), std::memory_order_relaxed);
        m_tail.store(u64(0), std::memory_order_relaxed);
        m_dropped.store(u64(0), std::memory_order_relaxed);
    }

    void detach() noexcept { m_data = nullptr; m_capacity = u64(0); }

    // Reset indices between simulation runs. Not thread-safe.
    void reset() noexcept {
        m_head.store(u64(0), std::memory_order_relaxed);
        m_tail.store(u64(0), std::memory_order_relaxed);
        m_dropped.store(u64(0), std::memory_order_relaxed);
    }

    // Producer — wait-free. Returns false if full (record dropped).
    [[nodiscard]] bool push(const EventRecord& r) noexcept {
        if (!m_data) return false;
        const u64 head = m_head.load(std::memory_order_relaxed);
        const u64 next = (head + u64(1)) % m_capacity;
        if (next == m_tail.load(std::memory_order_acquire)) {
            m_dropped.fetch_add(u64(1), std::memory_order_relaxed);
            return false;
        }
        m_data[head] = r;
        m_head.store(next, std::memory_order_release);
        return true;
    }

    // Consumer — lock-free. Drains into out_span. Returns records written.
    [[nodiscard]] u64 consume_all(std::span<EventRecord> out_span) noexcept {
        u64 written = u64(0);
        while (written < static_cast<u64>(out_span.size())) {
            const u64 tail = m_tail.load(std::memory_order_relaxed);
            if (tail == m_head.load(std::memory_order_acquire)) break;
            out_span[static_cast<std::size_t>(written++)] = m_data[tail];
            m_tail.store((tail + u64(1)) % m_capacity, std::memory_order_release);
        }
        return written;
    }

    [[nodiscard]] u64  capacity()    const noexcept { return m_capacity; }
    [[nodiscard]] bool empty()       const noexcept {
        return m_head.load(std::memory_order_acquire)
            == m_tail.load(std::memory_order_acquire);
    }
    [[nodiscard]] u64  dropped()     const noexcept {
        return m_dropped.load(std::memory_order_relaxed);
    }
    [[nodiscard]] u64  approx_size() const noexcept {
        const u64 h = m_head.load(std::memory_order_relaxed);
        const u64 t = m_tail.load(std::memory_order_relaxed);
        return h >= t ? h - t : m_capacity - t + h;
    }

private:
    EventRecord* m_data     = nullptr;
    u64          m_capacity = u64(0);

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

} // namespace ndde::events
