#include "engine/logging/LoggerService.hpp"

#include <gtest/gtest.h>

namespace {

using namespace ndde;

TEST(LoggerService, StoresOwnedMessagesWithStableIds) {
    LoggerService logger;
    logger.init(LoggerConfig{.max_records = u64(8), .max_string_bytes = u64(1024)});

    std::string source = "engine ready";
    const LogRecordId id = logger.write(LogSeverity::Info, LogCategory::Engine, {}, source);
    source = "mutated";

    ASSERT_EQ(logger.records().size(), 1u);
    EXPECT_EQ(logger.records().front().id, id);
    EXPECT_EQ(logger.message(id), "engine ready");
    EXPECT_EQ(logger.message(LogRecordId{u64(999)}), "");
}

TEST(LoggerService, EvictsOldestByRecordCapacityAndTracksDrops) {
    LoggerService logger;
    logger.init(LoggerConfig{.max_records = u64(2), .max_string_bytes = u64(1024)});

    const LogRecordId first = logger.write(LogSeverity::Info, LogCategory::Engine, {}, "one");
    const LogRecordId second = logger.write(LogSeverity::Info, LogCategory::Engine, {}, "two");
    const LogRecordId third = logger.write(LogSeverity::Info, LogCategory::Engine, {}, "three");

    ASSERT_EQ(logger.records().size(), 2u);
    EXPECT_EQ(logger.records()[0].id, second);
    EXPECT_EQ(logger.records()[1].id, third);
    EXPECT_EQ(logger.message(first), "");
    EXPECT_EQ(logger.dropped_records(), u64(1));
}

TEST(LoggerService, EvictsOldestByStringCapacityAndTracksBytes) {
    LoggerService logger;
    logger.init(LoggerConfig{.max_records = u64(8), .max_string_bytes = u64(6)});

    const LogRecordId first = logger.write(LogSeverity::Info, LogCategory::Engine, {}, "abcd");
    const LogRecordId second = logger.write(LogSeverity::Info, LogCategory::Engine, {}, "efg");

    ASSERT_EQ(logger.records().size(), 1u);
    EXPECT_EQ(logger.records().front().id, second);
    EXPECT_EQ(logger.message(first), "");
    EXPECT_EQ(logger.string_bytes(), u64(3));
    EXPECT_EQ(logger.dropped_records(), u64(1));
    EXPECT_EQ(logger.dropped_string_bytes(), u64(4));
}

TEST(LoggerService, KeepsNewestOversizedMessageWhenStringCapacityIsSmall) {
    LoggerService logger;
    logger.init(LoggerConfig{.max_records = u64(8), .max_string_bytes = u64(3)});

    const LogRecordId id = logger.write(LogSeverity::Info, LogCategory::Engine, {}, "oversized");

    ASSERT_EQ(logger.records().size(), 1u);
    EXPECT_EQ(logger.records().front().id, id);
    EXPECT_EQ(logger.message(id), "oversized");
    EXPECT_EQ(logger.string_bytes(), u64(9));
    EXPECT_EQ(logger.dropped_records(), u64(0));
}

TEST(LoggerService, FiltersSeverityAndCategory) {
    LoggerService logger;
    logger.init();

    (void)logger.write(LogSeverity::Debug, LogCategory::Engine, {}, "debug");
    (void)logger.write(LogSeverity::Warning, LogCategory::Capture, {}, "capture warning");
    (void)logger.write(LogSeverity::Error, LogCategory::Diagnostics, {}, "diagnostic error");

    const std::vector<LogRecord> warnings = logger.records_at_or_above(LogSeverity::Warning);
    ASSERT_EQ(warnings.size(), 2u);
    EXPECT_EQ(warnings[0].severity, LogSeverity::Warning);
    EXPECT_EQ(warnings[1].severity, LogSeverity::Error);

    const std::vector<LogRecord> capture = logger.records_in_category(LogCategory::Capture);
    ASSERT_EQ(capture.size(), 1u);
    EXPECT_EQ(capture.front().category, LogCategory::Capture);
}

TEST(LoggerService, KeepsEventAndDiagnosticReferencesStructured) {
    LoggerService logger;
    logger.init();

    const EventRef event{
        .channel = EventChannelId::Simulation,
        .type = EventTypeId{"event.agent_captured"},
        .sequence = u64(17),
        .tick = u64(42),
        .sim_time = f32(3.5f)
    };
    const LogRecordId event_record = logger.write_event(event,
                                                        LogSeverity::Info,
                                                        LogCategory::Simulation,
                                                        "agent captured");
    const LogRecordId diagnostic_record = logger.write_diagnostic(DiagnosticId{u64(9)},
                                                                  LogSeverity::Error,
                                                                  "metric invalid");

    ASSERT_EQ(logger.records().size(), 2u);
    EXPECT_EQ(logger.records()[0].id, event_record);
    ASSERT_TRUE(logger.records()[0].event.has_value());
    EXPECT_EQ(logger.records()[0].event->channel, EventChannelId::Simulation);
    EXPECT_EQ(logger.records()[0].event->sequence, u64(17));
    EXPECT_EQ(logger.records()[0].tick, u64(42));
    EXPECT_FLOAT_EQ(logger.records()[0].sim_time, f32(3.5f));

    EXPECT_EQ(logger.records()[1].id, diagnostic_record);
    ASSERT_TRUE(logger.records()[1].diagnostic.has_value());
    EXPECT_EQ(logger.records()[1].diagnostic->value, u64(9));
    EXPECT_EQ(logger.records()[1].category, LogCategory::Diagnostics);
}

TEST(LoggerService, KeepsResourceReferencesStructured) {
    LoggerService logger;
    logger.init();

    const ResourceId resource{u64(42)};
    const LogRecordId id = logger.write_resource(resource,
                                                 LogSeverity::Info,
                                                 "resource loaded");

    ASSERT_EQ(logger.records().size(), 1u);
    EXPECT_EQ(logger.records().front().id, id);
    EXPECT_EQ(logger.records().front().category, LogCategory::Resource);
    ASSERT_TRUE(logger.records().front().resource.has_value());
    EXPECT_EQ(*logger.records().front().resource, resource);
    EXPECT_EQ(logger.message(id), "resource loaded");
}

TEST(LoggerService, ClearRemovesRecordsButPreservesDropCounters) {
    LoggerService logger;
    logger.init(LoggerConfig{.max_records = u64(1), .max_string_bytes = u64(1024)});

    (void)logger.write(LogSeverity::Info, LogCategory::Engine, {}, "one");
    (void)logger.write(LogSeverity::Info, LogCategory::Engine, {}, "two");
    ASSERT_EQ(logger.dropped_records(), u64(1));

    logger.clear();
    EXPECT_TRUE(logger.records().empty());
    EXPECT_EQ(logger.string_bytes(), u64(0));
    EXPECT_EQ(logger.dropped_records(), u64(1));
}

} // namespace
