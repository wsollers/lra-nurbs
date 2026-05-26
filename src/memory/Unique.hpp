#pragma once
// memory/Unique.hpp
// Lifetime-aware object ownership for MemoryService scopes.
//
// memory::Unique<T> is intentionally std::unique_ptr-shaped, but its storage is
// obtained from a std::pmr::memory_resource instead of the global heap. In debug
// builds it also carries a scope generation token so stale owners assert if used
// after their lifetime scope has been reset.

#include "math/Scalars.hpp"

#include <cassert>
#include <cstddef>
#include <memory>
#include <memory_resource>
#include <new>
#include <type_traits>
#include <utility>

namespace ndde::memory {

template <class T>
class UniqueDeleter {
public:
    UniqueDeleter() = default;

    template <class U>
        requires std::is_convertible_v<U*, T*>
    UniqueDeleter(const UniqueDeleter<U>& other) noexcept
        : m_resource(other.m_resource)
        , m_bytes(other.m_bytes)
        , m_alignment(other.m_alignment)
        , m_generation(other.m_generation)
        , m_expected_generation(other.m_expected_generation)
        , m_destroy([](T* ptr) noexcept {
            std::destroy_at(static_cast<U*>(ptr));
        })
    {}

    void operator()(T* ptr) const noexcept {
        if (!ptr) return;
        assert_alive();
        if (m_destroy)
            m_destroy(ptr);
        if (m_resource)
            m_resource->deallocate(ptr, m_bytes, m_alignment);
    }

    template <class U>
        requires std::is_convertible_v<U*, T*>
    [[nodiscard]] static UniqueDeleter for_type(std::pmr::memory_resource* resource,
                                                const u64* generation = nullptr) noexcept {
        UniqueDeleter deleter;
        deleter.m_resource = resource;
        deleter.m_bytes = sizeof(U);
        deleter.m_alignment = alignof(U);
        deleter.m_generation = generation;
        deleter.m_expected_generation = generation ? *generation : 0u;
        deleter.m_destroy = [](T* ptr) noexcept {
            std::destroy_at(static_cast<U*>(ptr));
        };
        return deleter;
    }

    void assert_alive() const noexcept {
#ifndef NDEBUG
        if (m_generation)
            assert(*m_generation == m_expected_generation && "memory::Unique used after its scope was reset");
#endif
    }

private:
    template <class>
    friend class UniqueDeleter;

    std::pmr::memory_resource* m_resource = nullptr;
    std::size_t m_bytes = 0;
    std::size_t m_alignment = alignof(T);
    const u64* m_generation = nullptr;
    u64 m_expected_generation = 0;
    void (*m_destroy)(T*) noexcept = nullptr;
};

template <class T>
using Unique = std::unique_ptr<T, UniqueDeleter<T>>;

template <class Base, class Derived>
    requires std::is_convertible_v<Derived*, Base*>
[[nodiscard]] Unique<Base> unique_cast(Unique<Derived>&& owned) noexcept {
    UniqueDeleter<Base> deleter(owned.get_deleter());
    return Unique<Base>(owned.release(), deleter);
}

template <class T, class... Args>
[[nodiscard]] Unique<T> make_unique_with_generation(std::pmr::memory_resource* resource,
                                                    const u64* generation,
                                                    Args&&... args) {
    if (!resource)
        resource = std::pmr::get_default_resource();

    void* storage = resource->allocate(sizeof(T), alignof(T));
    try {
        T* object = std::construct_at(static_cast<T*>(storage), std::forward<Args>(args)...);
        return Unique<T>(object, UniqueDeleter<T>::template for_type<T>(resource, generation));
    } catch (...) {
        resource->deallocate(storage, sizeof(T), alignof(T));
        throw;
    }
}

template <class T, class... Args>
[[nodiscard]] Unique<T> make_unique(std::pmr::memory_resource* resource, Args&&... args) {
    return make_unique_with_generation<T>(resource, nullptr, std::forward<Args>(args)...);
}

} // namespace ndde::memory
