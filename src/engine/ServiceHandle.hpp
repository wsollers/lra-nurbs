#pragma once
// engine/ServiceHandle.hpp
// Move-only RAII registration handle used by engine-owned services.

#include "math/Scalars.hpp"

#include <cassert>
#include <functional>
#include <utility>

namespace ndde {

class ServiceHandle {
public:
    ServiceHandle() = default;
    explicit ServiceHandle(std::function<void()> release) : m_release(std::move(release)) {}
    ServiceHandle(std::function<void()> release, const u64* generation)
        : m_release(std::move(release))
        , m_generation(generation)
        , m_expected_generation(generation ? *generation : 0u)
    {}

    ~ServiceHandle() { reset(); }

    ServiceHandle(const ServiceHandle&) = delete;
    ServiceHandle& operator=(const ServiceHandle&) = delete;

    ServiceHandle(ServiceHandle&& other) noexcept
        : m_release(std::move(other.m_release))
        , m_generation(other.m_generation)
        , m_expected_generation(other.m_expected_generation)
    {
        other.m_release = {};
        other.m_generation = nullptr;
        other.m_expected_generation = 0;
    }

    ServiceHandle& operator=(ServiceHandle&& other) noexcept {
        if (this == &other) return *this;
        reset();
        m_release = std::move(other.m_release);
        m_generation = other.m_generation;
        m_expected_generation = other.m_expected_generation;
        other.m_release = {};
        other.m_generation = nullptr;
        other.m_expected_generation = 0;
        return *this;
    }

    void reset() noexcept {
        if (!m_release) return;
        assert_alive();
        auto release = std::move(m_release);
        m_release = {};
        m_generation = nullptr;
        m_expected_generation = 0;
        release();
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return static_cast<bool>(m_release);
    }

    void assert_alive() const noexcept {
#ifndef NDEBUG
        if (m_generation)
            assert(*m_generation == m_expected_generation && "service handle used after its service generation changed");
#endif
    }

private:
    std::function<void()> m_release;
    const u64* m_generation = nullptr;
    u64 m_expected_generation = 0;
};

} // namespace ndde
