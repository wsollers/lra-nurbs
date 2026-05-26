#pragma once
// app/SurfaceRegistry.hpp
// Named surface definitions used by simulation scenes.

#include "math/SineRationalSurface.hpp"
#include "math/Surfaces.hpp"
#include "memory/MemoryService.hpp"
#include "memory/Unique.hpp"
#include "numeric/ops.hpp"

#include <string_view>

namespace ndde {

class MultiWellWaveSurface final : public ndde::math::ISurface {
public:
    explicit MultiWellWaveSurface(f32 extent = 4.f) : m_extent(extent) {}

    [[nodiscard]] Vec3 evaluate(f32 x, f32 y, f32 = 0.f) const override {
        return {x, y, height(x, y)};
    }

    [[nodiscard]] f32 height(f32 x, f32 y) const noexcept {
        const f32 g0 = 1.15f * ops::exp(-((x - 1.4f) * (x - 1.4f) + (y - 0.6f) * (y - 0.6f)));
        const f32 g1 = -0.85f * ops::exp(-(((x + 1.2f) * (x + 1.2f)) / 0.8f
                                            + ((y + 1.0f) * (y + 1.0f)) / 1.4f));
        const f32 g2 = 0.65f * ops::exp(-(((x - 0.2f) * (x - 0.2f)) / 1.8f
                                           + ((y - 1.6f) * (y - 1.6f)) / 0.6f));
        const f32 w0 = 0.18f * ops::sin(2.f * x) * ops::cos(2.f * y);
        const f32 w1 = 0.08f * ops::sin(4.f * x + y) * ops::cos(3.f * y - x);
        return g0 + g1 + g2 + w0 + w1;
    }

    [[nodiscard]] f32 u_min(f32 = 0.f) const override { return -m_extent; }
    [[nodiscard]] f32 u_max(f32 = 0.f) const override { return  m_extent; }
    [[nodiscard]] f32 v_min(f32 = 0.f) const override { return -m_extent; }
    [[nodiscard]] f32 v_max(f32 = 0.f) const override { return  m_extent; }
    [[nodiscard]] ndde::math::SurfaceMetadata metadata(f32 t = 0.f) const override {
        ndde::math::SurfaceMetadata data = ndde::math::ISurface::metadata(t);
        data.name = "Multi-Well Wave Surface";
        data.formula = "three Gaussian wells + 0.18 sin(2x)cos(2y) + 0.08 sin(4x+y)cos(3y-x)";
        data.has_analytic_derivatives = false;
        data.parameters = {{
            {.name = "extent", .value = m_extent, .description = "square domain half-width"}
        }};
        data.parameter_count = 1u;
        return data;
    }
    [[nodiscard]] f32 extent() const noexcept { return m_extent; }

private:
    f32 m_extent = 4.f;
};

class WavePredatorPreySurface final : public ndde::math::ISurface {
public:
    explicit WavePredatorPreySurface(f32 extent = 4.f) : m_extent(extent) {}

    [[nodiscard]] Vec3 evaluate(f32 x, f32 y, f32 = 0.f) const override {
        return {x, y, height(x, y)};
    }

    [[nodiscard]] f32 height(f32 x, f32 y) const noexcept {
        return ops::sin(x) + ops::cos(y)
             + 0.5f * (ops::sin(2.f * x) + ops::cos(2.f * y));
    }

    [[nodiscard]] f32 u_min(f32 = 0.f) const override { return -m_extent; }
    [[nodiscard]] f32 u_max(f32 = 0.f) const override { return  m_extent; }
    [[nodiscard]] f32 v_min(f32 = 0.f) const override { return -m_extent; }
    [[nodiscard]] f32 v_max(f32 = 0.f) const override { return  m_extent; }
    [[nodiscard]] ndde::math::SurfaceMetadata metadata(f32 t = 0.f) const override {
        ndde::math::SurfaceMetadata data = ndde::math::ISurface::metadata(t);
        data.name = "Wave Predator-Prey Surface";
        data.formula = "z = sin x + cos y + 0.5(sin 2x + cos 2y)";
        data.has_analytic_derivatives = false;
        data.parameters = {{
            {.name = "extent", .value = m_extent, .description = "square domain half-width"}
        }};
        data.parameter_count = 1u;
        return data;
    }
    [[nodiscard]] f32 extent() const noexcept { return m_extent; }

private:
    f32 m_extent = 4.f;
};

enum class SurfaceKey : u8 {
    SineRational,
    MultiWell,
    WavePredatorPrey
};

struct SurfaceDescriptor {
    SurfaceKey key = SurfaceKey::SineRational;
    std::string_view id;
    std::string_view display_name;
    std::string_view formula;
};

class SurfaceRegistry {
public:
    [[nodiscard]] static SurfaceDescriptor describe(SurfaceKey key) noexcept {
        switch (key) {
            case SurfaceKey::MultiWell:
                return {key, "multi-well", "Multi-Well",
                    "z = Gaussian wells + 0.18 sin(2x)cos(2y) + 0.08 sin(4x+y)cos(3y-x)"};
            case SurfaceKey::WavePredatorPrey:
                return {key, "wave-predator-prey", "Wave Predator-Prey",
                    "z = sin x + cos y + 0.5(sin 2x + cos 2y)"};
            case SurfaceKey::SineRational:
            default:
                return {key, "sine-rational", "Sine-Rational",
                    "z = [3/(1+(x+y+1)^2)] sin(2x)cos(2y) + 0.1 sin(5x)sin(5y)"};
        }
    }

    [[nodiscard]] static memory::Unique<ndde::math::SineRationalSurface>
    make_sine_rational(memory::MemoryService* mem = nullptr, f32 extent = 4.f) {
        return mem ? mem->simulation().make_unique<ndde::math::SineRationalSurface>(extent)
                   : memory::make_unique<ndde::math::SineRationalSurface>(
                            std::pmr::get_default_resource(), extent);
    }

    [[nodiscard]] static memory::Unique<MultiWellWaveSurface>
    make_multi_well(memory::MemoryService* mem = nullptr, f32 extent = 4.f) {
        return mem ? mem->simulation().make_unique<MultiWellWaveSurface>(extent)
                   : memory::make_unique<MultiWellWaveSurface>(
                            std::pmr::get_default_resource(), extent);
    }

    [[nodiscard]] static memory::Unique<WavePredatorPreySurface>
    make_wave_predator_prey(memory::MemoryService* mem = nullptr, f32 extent = 4.f) {
        return mem ? mem->simulation().make_unique<WavePredatorPreySurface>(extent)
                   : memory::make_unique<WavePredatorPreySurface>(
                            std::pmr::get_default_resource(), extent);
    }
};

} // namespace ndde
