#include "units/QuantityTypes.hpp"

#include <gtest/gtest.h>

#include <mp-units/systems/si.h>

namespace {

TEST(MPUnitsIntegration, CreatesConvertsAndCombinesQuantities) {
    using namespace mp_units;
    using namespace mp_units::si::unit_symbols;

    quantity distance = 120.0 * km;
    quantity duration = 2.0 * h;
    quantity speed = distance / duration;

    EXPECT_NEAR(distance.numerical_value_in(m), 120000.0, 1e-9);
    EXPECT_NEAR(duration.numerical_value_in(s), 7200.0, 1e-9);
    EXPECT_NEAR(speed.numerical_value_in(km / h), 60.0, 1e-9);
    EXPECT_NEAR(speed.numerical_value_in(m / s), 1000.0 / 60.0, 1e-9);
}

TEST(MPUnitsIntegration, ProjectQuantityVocabularyCarriesSerializedValues) {
    ndde::units::QuantityValue value{
        .value = 0.25,
        .unit = "Hz",
        .kind = ndde::units::QuantityKind::TransitionRate
    };

    EXPECT_DOUBLE_EQ(value.value, 0.25);
    EXPECT_EQ(value.unit, "Hz");
    EXPECT_EQ(value.kind, ndde::units::QuantityKind::TransitionRate);
}

} // namespace
