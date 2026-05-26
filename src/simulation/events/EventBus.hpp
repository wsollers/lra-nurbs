#pragma once
// simulation/events/EventBus.hpp
// Typed synchronous event bus.
//
// Hot-path contract (dispatch):
//   1. Call all typed subscribers inline — zero allocation, wait-free.
//   2. Push an EventRecord into the attached EventRing — one atomic store.
//   3. NO std::format. NO heap allocation. NO mutex.
//
// Formatting and display happen in EventLog::drain(), called once per frame
// after the simulation tick completes.
//
// Thread safety: single-producer model — dispatch() must be called from the
// simulation tick thread only. subscribe/unsubscribe are main-thread only.

#include "math/Scalars.hpp"
#include "simulation/events/EventRecord.hpp"
#include "simulation/events/EventRing.hpp"

#include <algorithm>
#include <functional>
#include <memory>
#include <memory_resource>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace ndde::events {

class EventBus {
public:
    EventBus() = default;
    ~EventBus() = default;

    EventBus(const EventBus&)            = delete;
    EventBus& operator=(const EventBus&) = delete;
    EventBus(EventBus&&)                 = delete;
    EventBus& operator=(EventBus&&)      = delete;

    // Attach the ring that dispatch() will push records into.
    // Called by EventLog during init. Ring lifetime must exceed bus.
    void attach_ring(EventRing* ring) noexcept { m_ring = ring; }
    void detach_ring()                noexcept { m_ring = nullptr; }

    // ── Subscribe ─────────────────────────────────────────────────────────────
    // Returns an opaque token. Store it; pass to unsubscribe<E>() to remove.
    // Handlers are called synchronously in dispatch() on the sim thread.
    // Handlers must be fast and non-blocking.
    template <class E>
    [[nodiscard]] u64 subscribe(std::function<void(const E&)> handler) {
        const u64 token = m_next_token++;
        get_or_create_list<E>().entries.push_back(TypedEntry<E>{
            .token = token,
            .handler = std::move(handler)
        });
        return token;
    }

    template <class E>
    void unsubscribe(u64 token) {
        auto* list = find_list<E>();
        if (!list) {
            return;
        }
        list->entries.erase(
            std::remove_if(list->entries.begin(), list->entries.end(),
                [token](const TypedEntry<E>& e){ return e.token == token; }),
            list->entries.end());
    }

    void clear_all_subscribers() noexcept {
        for (auto& entry : m_subscribers) {
            if (entry.second) {
                entry.second->destroy();
            }
        }
        m_subscribers.clear();
        m_subscriber_storage.release();
    }

    // ── Dispatch — HOT PATH ───────────────────────────────────────────────────
    // Explicit record overload: caller supplies the EventRecord.
    // This is the preferred overload — zero overhead on the push side.
    template <class E>
    void dispatch(const E& event, const EventRecord& record) {
        if (auto* list = find_list<E>()) {
            for (auto& entry : list->entries)
                entry.handler(event);
        }
        if (m_ring)
            (void)m_ring->push(record);
    }

    // Convenience overload: record is built from event via make_record<E>.
    // make_record specialisations are in EventBus.cpp.
    template <class E>
    void dispatch(const E& event) {
        dispatch(event, make_record(event));
    }

    template <class E>
    void dispatch(const E& event, u64 sequence) {
        EventRecord record = make_record(event);
        record.sequence = sequence;
        dispatch(event, record);
    }

    [[nodiscard]] bool has_ring() const noexcept { return m_ring != nullptr; }
    [[nodiscard]] std::size_t subscriber_type_count() const noexcept {
        return m_subscribers.size();
    }

private:
    struct ListBase {
        virtual ~ListBase() = default;
        virtual void destroy() noexcept = 0;

        ListBase() = default;
        ListBase(const ListBase&) = delete;
        ListBase& operator=(const ListBase&) = delete;
        ListBase(ListBase&&) = delete;
        ListBase& operator=(ListBase&&) = delete;
    };

    template <class E>
    struct TypedEntry {
        u64 token;
        std::function<void(const E&)> handler;
    };

    template <class E>
    struct TypedList final : ListBase {
        std::vector<TypedEntry<E>> entries;

        void destroy() noexcept override {
            std::destroy_at(this);
        }
    };

    u64 m_next_token = u64(1);
    std::pmr::monotonic_buffer_resource m_subscriber_storage{1024u * 8u};
    std::pmr::polymorphic_allocator<std::byte> m_subscriber_allocator{&m_subscriber_storage};
    std::unordered_map<std::type_index, ListBase*> m_subscribers;
    EventRing* m_ring = nullptr;

    template <class E>
    [[nodiscard]] TypedList<E>* allocate_list() {
        void* storage = m_subscriber_allocator.allocate_bytes(sizeof(TypedList<E>), alignof(TypedList<E>));
        try {
            return std::construct_at(static_cast<TypedList<E>*>(storage));
        } catch (...) {
            m_subscriber_allocator.deallocate_bytes(storage, sizeof(TypedList<E>), alignof(TypedList<E>));
            throw;
        }
    }

    template <class E>
    [[nodiscard]] TypedList<E>* find_list() noexcept {
        const auto it = m_subscribers.find(std::type_index(typeid(E)));
        if (it == m_subscribers.end()) {
            return nullptr;
        }
        return static_cast<TypedList<E>*>(it->second);
    }

    template <class E>
    [[nodiscard]] TypedList<E>& get_or_create_list() {
        const std::type_index key{typeid(E)};
        auto it = m_subscribers.find(key);
        if (it == m_subscribers.end()) {
            it = m_subscribers.emplace(key, allocate_list<E>()).first;
        }
        return *static_cast<TypedList<E>*>(it->second);
    }

    // make_record<E>: convert typed event struct to EventRecord.
    // Specialisations defined in EventBus.cpp for every event type.
    template <class E>
    static EventRecord make_record(const E& event);
};

// ── ScopedSub — RAII subscription wrapper ─────────────────────────────────────
// Analogous to ServiceHandle. Unsubscribes on destruction.
// SimulationUI holds a collection of these — all subs removed on on_stop().

template <class E>
class ScopedSub {
public:
    ScopedSub() = default;

    ScopedSub(EventBus& bus, std::function<void(const E&)> handler)
        : m_bus(&bus)
        , m_token(bus.subscribe<E>(std::move(handler)))
    {}

    ~ScopedSub() { reset(); }

    void reset() noexcept {
        if (m_bus && m_token) {
            m_bus->unsubscribe<E>(m_token);
            m_bus   = nullptr;
            m_token = u64(0);
        }
    }

    ScopedSub(const ScopedSub&)            = delete;
    ScopedSub& operator=(const ScopedSub&) = delete;

    ScopedSub(ScopedSub&& o) noexcept
        : m_bus(o.m_bus), m_token(o.m_token)
    { o.m_bus = nullptr; o.m_token = u64(0); }

    ScopedSub& operator=(ScopedSub&& o) noexcept {
        reset();
        m_bus = o.m_bus; m_token = o.m_token;
        o.m_bus = nullptr; o.m_token = u64(0);
        return *this;
    }

    [[nodiscard]] bool active() const noexcept { return m_bus && m_token; }

private:
    EventBus* m_bus   = nullptr;
    u64       m_token = u64(0);
};

} // namespace ndde::events
