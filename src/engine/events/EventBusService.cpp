// engine/events/EventBusService.cpp

#include "engine/events/EventBusService.hpp"

#include <stdexcept>
#include <utility>

namespace ndde {

EventBusConfig EventBusConfig::defaults() {
    return EventBusConfig{{
        EventChannelConfig{
            .channel = EventChannelId::App,
            .ring_capacity_records = u64(2048),
            .max_display_records = u64(512),
            .record_to_log = true
        },
        EventChannelConfig{
            .channel = EventChannelId::Scenario,
            .ring_capacity_records = u64(2048),
            .max_display_records = u64(512),
            .record_to_log = true
        },
        EventChannelConfig{
            .channel = EventChannelId::Simulation,
            .ring_capacity_records = u64(4096),
            .max_display_records = u64(1024),
            .record_to_log = true
        },
        EventChannelConfig{
            .channel = EventChannelId::Ui,
            .ring_capacity_records = u64(1024),
            .max_display_records = u64(256),
            .record_to_log = true
        },
        EventChannelConfig{
            .channel = EventChannelId::Worker,
            .ring_capacity_records = u64(512),
            .max_display_records = u64(256),
            .record_to_log = true
        }
    }};
}

void EventBusService::init(EventBusConfig config) {
    shutdown();

    for (const EventChannelConfig& channel_config : config.channels) {
        Channel& ch = channel_for(channel_config.channel);
        ch.record_to_log = channel_config.record_to_log;
        ch.configured = true;
        ch.next_sequence = u64(1);
        m_mailbox_capacities[index_of(channel_config.channel)] = channel_config.ring_capacity_records;
        if (ch.record_to_log) {
            ch.log.init(channel_config.ring_capacity_records,
                        channel_config.max_display_records);
            ch.bus.attach_ring(&ch.log.ring());
        }
    }

    m_initialised = true;
}

void EventBusService::set_owner_guard(std::function<bool(std::string_view)> guard) {
    m_owner_guard = std::move(guard);
}

void EventBusService::shutdown() noexcept {
    for (Channel& channel : m_channels) {
        channel.bus.clear_all_subscribers();
        channel.bus.detach_ring();
        channel.log.destroy();
        channel.next_sequence = u64(1);
        channel.record_to_log = true;
        channel.configured = false;
    }
    {
        std::scoped_lock lock{m_mailbox_mutex};
        for (auto& mailbox : m_mailboxes) {
            mailbox.clear();
        }
        m_mailbox_capacities.fill(u64(512));
        m_mailbox_dropped.fill(u64(0));
    }
    m_initialised = false;
}

bool EventBusService::enqueue_worker_record(events::EventRecord record) {
    return enqueue_record(EventChannelId::Worker, record);
}

bool EventBusService::enqueue_record(EventChannelId channel, events::EventRecord record) {
    std::scoped_lock lock{m_mailbox_mutex};
    const std::size_t index = index_of(channel);
    std::vector<events::EventRecord>& mailbox = m_mailboxes[index];
    if (mailbox.size() >= static_cast<std::size_t>(mailbox_capacity(channel))) {
        ++m_mailbox_dropped[index];
        return false;
    }
    mailbox.push_back(record);
    return true;
}

u64 EventBusService::drain_worker_mailbox() {
    return drain_mailbox(EventChannelId::Worker);
}

u64 EventBusService::drain_mailbox(EventChannelId channel) {
    std::vector<events::EventRecord> pending;
    {
        std::scoped_lock lock{m_mailbox_mutex};
        pending.swap(m_mailboxes[index_of(channel)]);
    }

    Channel& target = channel_for(channel);
    for (events::EventRecord& record : pending) {
        record.sequence = next_sequence(target);
        if (target.record_to_log && target.configured) {
            (void)target.log.ring().push(record);
        }
    }
    return static_cast<u64>(pending.size());
}

void EventBusService::drain(EventChannelId channel, f32 sim_time, u64 tick) {
    if (!require_owner_thread("EventBusService::drain")) return;
    (void)drain_mailbox(channel);
    Channel& ch = channel_for(channel);
    if (ch.record_to_log)
        ch.log.drain(sim_time, tick);
}

void EventBusService::drain_all(f32 sim_time, u64 tick) {
    if (!require_owner_thread("EventBusService::drain_all")) return;
    for (std::size_t i = 0; i < m_channels.size(); ++i) {
        Channel& ch = m_channels[i];
        const auto channel = static_cast<EventChannelId>(i);
        (void)drain_mailbox(channel);
        if (ch.configured && ch.record_to_log)
            ch.log.drain(sim_time, tick);
    }
}

void EventBusService::reset_channel(EventChannelId channel) {
    if (!require_owner_thread("EventBusService::reset_channel")) return;
    Channel& ch = m_channels[index_of(channel)];
    ch.bus.clear_all_subscribers();
    ch.log.reset();
    ch.next_sequence = u64(1);
    std::scoped_lock lock{m_mailbox_mutex};
    m_mailboxes[index_of(channel)].clear();
    m_mailbox_dropped[index_of(channel)] = u64(0);
}

void EventBusService::clear_scenario_channels() {
    if (!require_owner_thread("EventBusService::clear_scenario_channels")) return;
    reset_channel(EventChannelId::Scenario);
    reset_channel(EventChannelId::Simulation);
}

events::EventLog& EventBusService::log(EventChannelId channel) {
    return channel_for(channel).log;
}

const events::EventLog& EventBusService::log(EventChannelId channel) const {
    return channel_for(channel).log;
}

events::EventBus& EventBusService::bus(EventChannelId channel) {
    return channel_for(channel).bus;
}

const events::EventBus& EventBusService::bus(EventChannelId channel) const {
    return channel_for(channel).bus;
}

u64 EventBusService::next_sequence_value(EventChannelId channel) const {
    return channel_for(channel).next_sequence;
}

u64 EventBusService::worker_mailbox_size() const {
    return mailbox_size(EventChannelId::Worker);
}

u64 EventBusService::mailbox_size(EventChannelId channel) const {
    std::scoped_lock lock{m_mailbox_mutex};
    return static_cast<u64>(m_mailboxes[index_of(channel)].size());
}

u64 EventBusService::mailbox_dropped(EventChannelId channel) const noexcept {
    return m_mailbox_dropped[index_of(channel)];
}

EventBusService::Channel& EventBusService::channel_for(EventChannelId channel) {
    const std::size_t index = index_of(channel);
    if (index >= m_channels.size())
        throw std::out_of_range("[EventBusService] Invalid event channel");
    return m_channels[index];
}

const EventBusService::Channel& EventBusService::channel_for(EventChannelId channel) const {
    const std::size_t index = index_of(channel);
    if (index >= m_channels.size())
        throw std::out_of_range("[EventBusService] Invalid event channel");
    return m_channels[index];
}

u64 EventBusService::mailbox_capacity(EventChannelId channel) const noexcept {
    const u64 capacity = m_mailbox_capacities[index_of(channel)];
    return capacity == u64(0) ? u64(512) : capacity;
}

bool EventBusService::require_owner_thread(std::string_view api_name) const {
    return !m_owner_guard || m_owner_guard(api_name);
}

} // namespace ndde
