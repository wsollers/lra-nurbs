#include "sim/HistoryBuffer.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace {

TEST(HistoryBuffer, QueriesAcrossRingWrapBoundary) {
    ndde::sim::HistoryBuffer history{4u};

    history.push(0.f, {0.f, 0.f});
    history.push(1.f, {10.f, -1.f});
    history.push(2.f, {20.f, -2.f});
    history.push(3.f, {30.f, -3.f});
    history.push(4.f, {40.f, -4.f});

    EXPECT_EQ(history.size(), 4u);
    EXPECT_FLOAT_EQ(history.oldest_t(), 1.f);
    EXPECT_FLOAT_EQ(history.newest_t(), 4.f);

    const glm::vec2 middle = history.query(2.5f);
    EXPECT_FLOAT_EQ(middle.x, 25.f);
    EXPECT_FLOAT_EQ(middle.y, -2.5f);

    const glm::vec2 physical_wrap = history.query(3.5f);
    EXPECT_FLOAT_EQ(physical_wrap.x, 35.f);
    EXPECT_FLOAT_EQ(physical_wrap.y, -3.5f);
}

TEST(HistoryBuffer, ToVectorReturnsChronologicalRecordsAfterWrap) {
    ndde::sim::HistoryBuffer history{3u};

    history.push(0.f, {0.f, 0.f});
    history.push(1.f, {1.f, 10.f});
    history.push(2.f, {2.f, 20.f});
    history.push(3.f, {3.f, 30.f});
    history.push(4.f, {4.f, 40.f});

    const auto records = history.to_vector();
    ASSERT_EQ(records.size(), 3u);

    EXPECT_FLOAT_EQ(records[0].t, 2.f);
    EXPECT_FLOAT_EQ(records[0].uv.x, 2.f);
    EXPECT_FLOAT_EQ(records[0].uv.y, 20.f);

    EXPECT_FLOAT_EQ(records[1].t, 3.f);
    EXPECT_FLOAT_EQ(records[1].uv.x, 3.f);
    EXPECT_FLOAT_EQ(records[1].uv.y, 30.f);

    EXPECT_FLOAT_EQ(records[2].t, 4.f);
    EXPECT_FLOAT_EQ(records[2].uv.x, 4.f);
    EXPECT_FLOAT_EQ(records[2].uv.y, 40.f);
}

TEST(HistoryBuffer, QueryClampsToWrappedWindowExtents) {
    ndde::sim::HistoryBuffer history{3u};

    history.push(0.f, {0.f, 0.f});
    history.push(1.f, {1.f, 10.f});
    history.push(2.f, {2.f, 20.f});
    history.push(3.f, {3.f, 30.f});

    const glm::vec2 before = history.query(0.5f);
    EXPECT_FLOAT_EQ(before.x, 1.f);
    EXPECT_FLOAT_EQ(before.y, 10.f);

    const glm::vec2 after = history.query(4.f);
    EXPECT_FLOAT_EQ(after.x, 3.f);
    EXPECT_FLOAT_EQ(after.y, 30.f);
}

} // namespace
