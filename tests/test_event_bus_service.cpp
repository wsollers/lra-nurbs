#include <gtest/gtest.h>

#include "engine/SimulationHost.hpp"
#include "engine/events/EventBusService.hpp"
#include "simulation/events/EngineEventTypes.hpp"
#include "simulation/events/SimEventTypes.hpp"

#include <type_traits>

namespace {

struct TestEvent {
    ndde::u64 value = ndde::u64(0);
};

[[nodiscard]] ndde::events::EventRecord test_record(ndde::u64 value) {
    ndde::events::EventRecord record;
    record.kind = ndde::events::EventKind::AlertCustom;
    record.severity = ndde::events::EventSeverity::Notice;
    record.tick = value;
    record.val_a = static_cast<ndde::f32>(value);
    record.set_label("TestEvent");
    return record;
}

} // namespace

TEST(EventBusService, TypedSubscriberReceivesPublishedEvent) {
    ndde::EventBusService events;
    events.init();

    ndde::u64 observed = ndde::u64(0);
    auto sub = events.subscribe<TestEvent>(
        ndde::EventChannelId::Simulation,
        [&](const TestEvent& event) { observed = event.value; });

    events.publish(ndde::EventChannelId::Simulation, TestEvent{.value = ndde::u64(42)}, test_record(42));

    EXPECT_TRUE(sub.active());
    EXPECT_EQ(observed, 42u);
}

TEST(EventBusService, SubscriptionResetStopsDelivery) {
    ndde::EventBusService events;
    events.init();

    ndde::u64 calls = ndde::u64(0);
    auto sub = events.subscribe<TestEvent>(
        ndde::EventChannelId::Simulation,
        [&](const TestEvent&) { ++calls; });

    events.publish(ndde::EventChannelId::Simulation, TestEvent{}, test_record(1));
    sub.reset();
    events.publish(ndde::EventChannelId::Simulation, TestEvent{}, test_record(2));

    EXPECT_FALSE(sub.active());
    EXPECT_EQ(calls, 1u);
}

TEST(EventBusService, MoveOnlySubscriptionUnsubscribesExactlyOnce) {
    ndde::EventBusService events;
    events.init();

    ndde::u64 calls = ndde::u64(0);
    auto sub = events.subscribe<TestEvent>(
        ndde::EventChannelId::Simulation,
        [&](const TestEvent&) { ++calls; });

    ndde::Subscription moved = std::move(sub);
    EXPECT_FALSE(sub.active());
    EXPECT_TRUE(moved.active());

    moved.reset();
    moved.reset();
    events.publish(ndde::EventChannelId::Simulation, TestEvent{}, test_record(1));

    EXPECT_EQ(calls, 0u);
}

TEST(EventBusService, PublishWithRecordDrainsToChannelLog) {
    ndde::EventBusService events;
    events.init(ndde::EventBusConfig{{
        ndde::EventChannelConfig{
            .channel = ndde::EventChannelId::Simulation,
            .ring_capacity_records = ndde::u64(8),
            .max_display_records = ndde::u64(8),
            .record_to_log = true
        }
    }});

    events.publish(ndde::EventChannelId::Simulation, TestEvent{.value = ndde::u64(7)}, test_record(7));
    events.drain(ndde::EventChannelId::Simulation, 0.f, ndde::u64(7));

    const auto& entries = events.log(ndde::EventChannelId::Simulation).entries();
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_NE(entries.front().text.find("TestEvent"), std::string::npos);
}

TEST(EventBusService, PublishAssignsPerChannelSequences) {
    ndde::EventBusService events;
    events.init();

    const ndde::u64 app_sequence = events.publish(ndde::EventChannelId::App,
                                                  TestEvent{.value = ndde::u64(1)},
                                                  test_record(1));
    const ndde::u64 sim_sequence_a = events.publish(ndde::EventChannelId::Simulation,
                                                    TestEvent{.value = ndde::u64(2)},
                                                    test_record(2));
    const ndde::u64 sim_sequence_b = events.publish(ndde::EventChannelId::Simulation,
                                                    TestEvent{.value = ndde::u64(3)},
                                                    test_record(3));

    EXPECT_EQ(app_sequence, 1u);
    EXPECT_EQ(sim_sequence_a, 1u);
    EXPECT_EQ(sim_sequence_b, 2u);
    EXPECT_EQ(events.next_sequence_value(ndde::EventChannelId::App), 2u);
    EXPECT_EQ(events.next_sequence_value(ndde::EventChannelId::Simulation), 3u);
}

TEST(EventBusService, ResetChannelResetsSequence) {
    ndde::EventBusService events;
    events.init();

    (void)events.publish(ndde::EventChannelId::Simulation,
                         TestEvent{.value = ndde::u64(2)},
                         test_record(2));
    events.reset_channel(ndde::EventChannelId::Simulation);
    const ndde::u64 sequence = events.publish(ndde::EventChannelId::Simulation,
                                              TestEvent{.value = ndde::u64(3)},
                                              test_record(3));

    EXPECT_EQ(sequence, 1u);
}

TEST(EventBusService, WorkerMailboxDrainsCompactRecordsToWorkerChannel) {
    ndde::EventBusService events;
    events.init(ndde::EventBusConfig{{
        ndde::EventChannelConfig{
            .channel = ndde::EventChannelId::Worker,
            .ring_capacity_records = ndde::u64(8),
            .max_display_records = ndde::u64(8),
            .record_to_log = true
        }
    }});

    EXPECT_TRUE(events.enqueue_worker_record(test_record(11)));
    EXPECT_EQ(events.worker_mailbox_size(), 1u);

    EXPECT_EQ(events.drain_worker_mailbox(), 1u);
    EXPECT_EQ(events.worker_mailbox_size(), 0u);
    events.drain(ndde::EventChannelId::Worker, 0.f, ndde::u64(11));

    const auto& entries = events.log(ndde::EventChannelId::Worker).entries();
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_NE(entries.front().text.find("TestEvent"), std::string::npos);
    EXPECT_EQ(events.next_sequence_value(ndde::EventChannelId::Worker), 2u);
}

TEST(EventBusService, ChannelMailboxDrainsCompactRecordsToRequestedChannel) {
    ndde::EventBusService events;
    events.init(ndde::EventBusConfig{{
        ndde::EventChannelConfig{
            .channel = ndde::EventChannelId::Simulation,
            .ring_capacity_records = ndde::u64(8),
            .max_display_records = ndde::u64(8),
            .record_to_log = true
        },
        ndde::EventChannelConfig{
            .channel = ndde::EventChannelId::Worker,
            .ring_capacity_records = ndde::u64(8),
            .max_display_records = ndde::u64(8),
            .record_to_log = true
        }
    }});

    EXPECT_TRUE(events.enqueue_record(ndde::EventChannelId::Simulation, test_record(17)));
    EXPECT_EQ(events.mailbox_size(ndde::EventChannelId::Simulation), 1u);
    EXPECT_EQ(events.mailbox_size(ndde::EventChannelId::Worker), 0u);

    EXPECT_EQ(events.drain_mailbox(ndde::EventChannelId::Simulation), 1u);
    events.drain(ndde::EventChannelId::Simulation, 0.f, ndde::u64(17));

    const auto& sim_entries = events.log(ndde::EventChannelId::Simulation).entries();
    ASSERT_EQ(sim_entries.size(), 1u);
    EXPECT_NE(sim_entries.front().text.find("TestEvent"), std::string::npos);
    EXPECT_TRUE(events.log(ndde::EventChannelId::Worker).entries().empty());
}

TEST(EventBusService, WorkerMailboxReportsOverflow) {
    ndde::EventBusService events;
    events.init(ndde::EventBusConfig{{
        ndde::EventChannelConfig{
            .channel = ndde::EventChannelId::Worker,
            .ring_capacity_records = ndde::u64(1),
            .max_display_records = ndde::u64(1),
            .record_to_log = true
        }
    }});

    EXPECT_TRUE(events.enqueue_worker_record(test_record(1)));
    EXPECT_FALSE(events.enqueue_worker_record(test_record(2)));
    EXPECT_EQ(events.worker_mailbox_size(), 1u);
    EXPECT_EQ(events.worker_mailbox_dropped(), 1u);
}

TEST(EventBusService, ChannelMailboxReportsOverflow) {
    ndde::EventBusService events;
    events.init(ndde::EventBusConfig{{
        ndde::EventChannelConfig{
            .channel = ndde::EventChannelId::Simulation,
            .ring_capacity_records = ndde::u64(1),
            .max_display_records = ndde::u64(1),
            .record_to_log = true
        }
    }});

    EXPECT_TRUE(events.enqueue_record(ndde::EventChannelId::Simulation, test_record(1)));
    EXPECT_FALSE(events.enqueue_record(ndde::EventChannelId::Simulation, test_record(2)));
    EXPECT_EQ(events.mailbox_size(ndde::EventChannelId::Simulation), 1u);
    EXPECT_EQ(events.mailbox_dropped(ndde::EventChannelId::Simulation), 1u);
}

TEST(EventBusService, ChannelsAreIsolated) {
    ndde::EventBusService events;
    events.init();

    ndde::u64 app_calls = ndde::u64(0);
    ndde::u64 sim_calls = ndde::u64(0);
    auto app_sub = events.subscribe<TestEvent>(
        ndde::EventChannelId::App,
        [&](const TestEvent&) { ++app_calls; });
    auto sim_sub = events.subscribe<TestEvent>(
        ndde::EventChannelId::Simulation,
        [&](const TestEvent&) { ++sim_calls; });

    events.publish(ndde::EventChannelId::App, TestEvent{}, test_record(1));

    EXPECT_EQ(app_calls, 1u);
    EXPECT_EQ(sim_calls, 0u);
}

TEST(EventBusService, ClearScenarioChannelsLeavesAppChannelAlive) {
    ndde::EventBusService events;
    events.init();

    ndde::u64 app_calls = ndde::u64(0);
    ndde::u64 sim_calls = ndde::u64(0);
    auto app_sub = events.subscribe<TestEvent>(
        ndde::EventChannelId::App,
        [&](const TestEvent&) { ++app_calls; });
    auto sim_sub = events.subscribe<TestEvent>(
        ndde::EventChannelId::Simulation,
        [&](const TestEvent&) { ++sim_calls; });

    events.clear_scenario_channels();
    events.publish(ndde::EventChannelId::App, TestEvent{}, test_record(1));
    events.publish(ndde::EventChannelId::Simulation, TestEvent{}, test_record(2));

    EXPECT_EQ(app_calls, 1u);
    EXPECT_EQ(sim_calls, 0u);
}

TEST(EventBusService, KnownEngineEventUsesConvenienceRecordConversion) {
    ndde::EventBusService events;
    events.init();

    events.publish(ndde::EventChannelId::App, ndde::events::AppStarted{});
    events.drain(ndde::EventChannelId::App, 0.f, ndde::u64(0));

    const auto& entries = events.log(ndde::EventChannelId::App).entries();
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_NE(entries.front().text.find("AppStarted"), std::string::npos);
}

TEST(EventBus, DispatchWithoutSubscribersDoesNotCreateSubscriberList) {
    ndde::events::EventBus bus;

    bus.dispatch(TestEvent{.value = ndde::u64(1)}, test_record(1));

    EXPECT_EQ(bus.subscriber_type_count(), 0u);
}

TEST(EventTypes, HotSimulationEventsAreTriviallyCopyable) {
    static_assert(std::is_trivially_copyable_v<ndde::simulation::events::ScenarioStarted>);
    static_assert(std::is_trivially_copyable_v<ndde::simulation::events::ScenarioStopped>);
    static_assert(std::is_trivially_copyable_v<ndde::simulation::events::AgentSpawned>);
    static_assert(std::is_trivially_copyable_v<ndde::simulation::events::AgentCaptured>);
    static_assert(std::is_trivially_copyable_v<ndde::simulation::events::PerturbationFired>);
    static_assert(std::is_trivially_copyable_v<ndde::simulation::events::FieldAdded>);
    static_assert(std::is_trivially_copyable_v<ndde::simulation::events::FieldRemoved>);

    SUCCEED();
}

TEST(EngineServices, OwnsEventBusServiceAndPassesItToSimulationHost) {
    ndde::EngineServices services;
    services.events().init();

    ndde::SimulationHost host = services.simulation_host();
    ndde::u64 calls = ndde::u64(0);
    auto sub = host.events().subscribe<TestEvent>(
        ndde::EventChannelId::Simulation,
        [&](const TestEvent&) { ++calls; });

    services.events().publish(ndde::EventChannelId::Simulation, TestEvent{}, test_record(1));

    EXPECT_EQ(calls, 1u);
}
