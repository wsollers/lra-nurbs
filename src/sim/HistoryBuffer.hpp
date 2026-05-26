#pragma once
// sim/HistoryBuffer.hpp
// Ring buffer of timestamped parameter-space positions.
//
// Purpose
// ───────
// DelayPursuitEquation needs to know where the leader WAS at time (t - tau).
// HistoryBuffer stores a sliding window of (time, uv) records and provides
// linear interpolation for arbitrary past times within the window.
//
// Design
// ──────
// Fixed-capacity ring buffer.  Oldest records are overwritten as new ones
// arrive.  The capacity is chosen so that the oldest record is always at
// least (tau + margin) seconds in the past.  A good rule of thumb:
//   capacity = ceil((tau + 1.0) / dt_record) + 1
// where dt_record is the minimum inter-record interval.
//
// Records are stored chronologically; push() appends; query() binary-searches
// for the two bracketing records and linearly interpolates.
//
// Thread safety: none -- intended for single-threaded simulation.
//
// Predicate logic (for the formal repository):
//   query(t_past): exists unique i such that
//     records[i].t <= t_past <= records[i+1].t
//     => return lerp(records[i].uv, records[i+1].uv, alpha)
//     where alpha = (t_past - records[i].t) / (records[i+1].t - records[i].t)
//   If t_past < oldest record: return oldest record (clamp left)
//   If t_past > newest record: return newest record (clamp right)

#include "memory/Containers.hpp"

#include <glm/glm.hpp>
#include <cstddef>
#include <cmath>
#include <memory_resource>
#include <utility>

namespace ndde::sim {

class HistoryBuffer {
public:
    struct Record {
        float      t  = 0.f;
        glm::vec2  uv = {0.f, 0.f};
    };

    // capacity: maximum number of records held simultaneously.
    // dt_min:   minimum time between stored records (rate-limiter).
    //           Records more frequent than dt_min are silently dropped.
    explicit HistoryBuffer(std::size_t capacity = 4096,
                           float dt_min = 0.f,
                           std::pmr::memory_resource* resource = std::pmr::get_default_resource())
        : m_capacity(capacity), m_dt_min(dt_min), m_records(resource)
    {
        m_records.reserve(capacity);
    }

    explicit HistoryBuffer(std::size_t capacity,
                           float dt_min,
                           memory::HistoryVector<Record> records)
        : m_capacity(capacity), m_dt_min(dt_min), m_records(std::move(records))
    {
        m_records.reserve(capacity);
    }

    // ── Push ──────────────────────────────────────────────────────────────────
    // Append a new record.  If the buffer is at capacity, the oldest record
    // is overwritten (ring buffer semantics via m_head).
    // Rate-limiting: if t - m_last_push_t < dt_min, the record is dropped.
    void push(float t, glm::vec2 uv) {
        if (!m_records.empty() && (t - m_last_push_t) < m_dt_min)
            return;
        m_last_push_t = t;

        Record rec{ t, uv };
        if (m_records.size() < m_capacity) {
            m_records.push_back(rec);
        } else {
            m_records[m_head] = rec;
            m_head = (m_head + 1) % m_capacity;
        }
    }

    // ── Query ─────────────────────────────────────────────────────────────────
    // Return the interpolated uv at time t_past.
    // Clamps to the available window if t_past is out of range.
    [[nodiscard]] glm::vec2 query(float t_past) const {
        const std::size_t n = m_records.size();
        if (n == 0) return {0.f, 0.f};
        if (n == 1) return m_records[0].uv;

        // Records in chronological order (oldest first).
        // When the buffer wraps, elements starting at m_head are oldest.
        auto get = [&](std::size_t i) -> const Record& {
            if (m_records.size() < m_capacity)
                return m_records[i];          // not yet wrapped
            return m_records[(m_head + i) % m_capacity];
        };

        // Clamp left
        if (t_past <= get(0).t) return get(0).uv;
        // Clamp right
        if (t_past >= get(n-1).t) return get(n-1).uv;

        // Binary search for bracketing pair
        std::size_t lo = 0, hi = n - 1;
        while (lo + 1 < hi) {
            const std::size_t mid = (lo + hi) / 2;
            if (get(mid).t <= t_past) lo = mid;
            else                       hi = mid;
        }

        const Record& r0 = get(lo);
        const Record& r1 = get(lo + 1);
        const float span = r1.t - r0.t;
        const float alpha = (span < 1e-12f) ? 0.f : (t_past - r0.t) / span;
        return r0.uv + alpha * (r1.uv - r0.uv);
    }

    // ── State queries ─────────────────────────────────────────────────────────
    [[nodiscard]] bool    empty()    const noexcept { return m_records.empty(); }
    [[nodiscard]] float   oldest_t() const noexcept {
        if (m_records.empty()) return 0.f;
        if (m_records.size() < m_capacity) return m_records.front().t;
        return m_records[m_head].t;
    }
    [[nodiscard]] float   newest_t() const noexcept {
        if (m_records.empty()) return 0.f;
        if (m_records.size() < m_capacity) return m_records.back().t;
        const std::size_t prev = (m_head + m_capacity - 1) % m_capacity;
        return m_records[prev].t;
    }
    [[nodiscard]] std::size_t size()     const noexcept { return m_records.size(); }
    [[nodiscard]] std::size_t capacity() const noexcept { return m_capacity; }

    void clear() {
        m_records.clear();
        m_head = 0;
        m_last_push_t = -1e30f;
    }

    // Return all records in chronological order (oldest first).
    // When the buffer has not yet wrapped: a simple copy of m_records.
    // When wrapped: reorders the two halves around m_head.
    // Cost: O(n) time and space.  Only call for export/debug, not per-frame.
    [[nodiscard]] memory::HistoryVector<Record> to_vector() const {
        memory::HistoryVector<Record> out = m_records.make_same_lifetime_vector();
        const std::size_t n = m_records.size();
        out.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            if (n < m_capacity)
                out.push_back(m_records[i]);           // not yet wrapped
            else
                out.push_back(m_records[(m_head + i) % m_capacity]);  // wrapped
        }
        return out;   // already chronological by construction
    }

private:
    std::size_t       m_capacity;
    float             m_dt_min;
    memory::HistoryVector<Record> m_records;
    std::size_t       m_head = 0;       // index of oldest record (when full)
    float             m_last_push_t = -1e30f;
};

} // namespace ndde::sim
