#pragma once
// units/QuantityTypes.hpp
// Project-facing quantity vocabulary and serialized quantity values.

#include "math/Scalars.hpp"

#include <string>

namespace ndde::units {

enum class QuantityKind {
    Dimensionless,
    Time,
    Frequency,
    Length,
    Angle,
    Area,
    Curvature,
    Speed,
    Acceleration,
    Probability,
    TransitionRate,
    DwellTime,
    NURBSWeight,
    ParameterT,
    MetricFactor,
    Energy
};

struct QuantityValue {
    f64 value = 0.0;
    std::string unit;
    QuantityKind kind = QuantityKind::Dimensionless;
};

} // namespace ndde::units
