#pragma once
// memory/Containers.hpp
// Central container policy aliases.
//
// The architectural rule is that code depends on lifetime policy names, not
// directly on STL container choices.  All lifetime aliases are PMR-backed so
// storage can bind to the matching MemoryService scope.

#include "math/Scalars.hpp"

#include <cassert>
#include <iostream>
#include <memory_resource>
#include <string_view>
#include <typeinfo>
#include <vector>

namespace ndde::memory {

template <class T>
class LifetimeVector : public std::pmr::vector<T> {
public:
    using Base = std::pmr::vector<T>;
    using Base::Base;

    void bind_generation(const u64* generation, std::string_view lifetime_name = {}) noexcept {
#ifndef NDEBUG
        m_generation = generation;
        m_expected_generation = generation ? *generation : 0u;
        m_lifetime_name = lifetime_name;
#else
        (void)generation;
        (void)lifetime_name;
#endif
    }

    [[nodiscard]] LifetimeVector make_same_lifetime_vector() const {
        LifetimeVector out{Base::get_allocator().resource()};
#ifndef NDEBUG
        out.bind_generation(m_generation, m_lifetime_name);
#endif
        return out;
    }

    void assert_alive() const noexcept {
#ifndef NDEBUG
        if (m_generation && *m_generation != m_expected_generation) {
            std::cerr << "[MemoryService] stale LifetimeVector<" << typeid(T).name()
                      << "> scope='" << m_lifetime_name
                      << "' expected_generation=" << m_expected_generation
                      << " current_generation=" << *m_generation << '\n';
            assert(*m_generation == m_expected_generation && "memory lifetime vector used after its scope was reset");
        }
#endif
    }

    decltype(auto) begin() noexcept { assert_alive(); return Base::begin(); }
    decltype(auto) begin() const noexcept { assert_alive(); return Base::begin(); }
    decltype(auto) cbegin() const noexcept { assert_alive(); return Base::cbegin(); }
    decltype(auto) end() noexcept { assert_alive(); return Base::end(); }
    decltype(auto) end() const noexcept { assert_alive(); return Base::end(); }
    decltype(auto) cend() const noexcept { assert_alive(); return Base::cend(); }

    [[nodiscard]] bool empty() const noexcept { assert_alive(); return Base::empty(); }
    [[nodiscard]] std::size_t size() const noexcept { assert_alive(); return Base::size(); }
    [[nodiscard]] std::size_t capacity() const noexcept { assert_alive(); return Base::capacity(); }

    T* data() noexcept { assert_alive(); return Base::data(); }
    const T* data() const noexcept { assert_alive(); return Base::data(); }

    T& front() noexcept { assert_alive(); return Base::front(); }
    const T& front() const noexcept { assert_alive(); return Base::front(); }
    T& back() noexcept { assert_alive(); return Base::back(); }
    const T& back() const noexcept { assert_alive(); return Base::back(); }
    T& operator[](std::size_t index) noexcept { assert_alive(); return Base::operator[](index); }
    const T& operator[](std::size_t index) const noexcept { assert_alive(); return Base::operator[](index); }

    void clear() noexcept { assert_alive(); Base::clear(); }
    void reserve(std::size_t count) { assert_alive(); Base::reserve(count); }
    void resize(std::size_t count) { assert_alive(); Base::resize(count); }
    void resize(std::size_t count, const T& value) { assert_alive(); Base::resize(count, value); }

    template <class... Args>
    decltype(auto) emplace_back(Args&&... args) {
        assert_alive();
        return Base::emplace_back(std::forward<Args>(args)...);
    }

    void push_back(const T& value) {
        assert_alive();
        Base::push_back(value);
    }

    void push_back(T&& value) {
        assert_alive();
        Base::push_back(std::move(value));
    }

    template <class InputIt>
    void assign(InputIt first, InputIt last) {
        assert_alive();
        Base::assign(first, last);
    }

private:
#ifndef NDEBUG
    const u64* m_generation = nullptr;
    u64 m_expected_generation = 0;
    std::string_view m_lifetime_name;
#endif
};

template <class T>
using FrameVector = LifetimeVector<T>;

template <class T>
using ViewVector = LifetimeVector<T>;

template <class T>
using SimVector = LifetimeVector<T>;

template <class T>
using CacheVector = LifetimeVector<T>;

template <class T>
using HistoryVector = LifetimeVector<T>;

template <class T>
using PersistentVector = LifetimeVector<T>;

} // namespace ndde::memory
