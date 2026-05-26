// telemetry/TelemetryService.cpp

#include "telemetry/TelemetryService.hpp"
#include <cassert>
#include <format>
#include <iostream>
#include <utility>

namespace ndde::telemetry {

void TelemetryService::set_owner_guard(std::function<bool(std::string_view)> guard) {
    m_owner_guard = std::move(guard);
}

void TelemetryService::init(u64 buffer_records,
                             std::filesystem::path output_dir) {
    if (!require_owner_thread("TelemetryService::init")) return;

    // Allocate slab — aligned to cache line for the ring's atomic indices.
    const u64 slab_bytes = buffer_records * static_cast<u64>(sizeof(TelemetryRecord));
    m_slab.resize(static_cast<std::size_t>(slab_bytes));
    m_ring.attach(m_slab.data(), slab_bytes);

    // Pre-size the flush scratch buffer to ring capacity to avoid re-allocation
    // during a flush call.
    m_flush_scratch.resize(static_cast<std::size_t>(buffer_records));
    m_ext_scratch.reserve(1024u);

    m_writer.set_output_dir(std::move(output_dir));

    std::cout << std::format(
        "[Telemetry] Initialised -- {} records capacity ({} MB slab)\n",
        buffer_records,
        slab_bytes / (u64(1024) * u64(1024)));
}

void TelemetryService::destroy() noexcept {
    std::scoped_lock lock(m_ext_mutex);
    m_ring.detach();
    m_slab.clear();
    m_slab.shrink_to_fit();
    m_flush_scratch.clear();
    m_flush_scratch.shrink_to_fit();
    m_ext_scratch.clear();
    m_ext_scratch.shrink_to_fit();
}

void TelemetryService::on_app_started(const events::AppStarted& /*e*/) {
    if (!require_owner_thread("TelemetryService::on_app_started")) return;
    std::cout << "[Telemetry] App started.\n";
}

void TelemetryService::on_app_stopping(const events::AppStopping& /*e*/) {
    if (!require_owner_thread("TelemetryService::on_app_stopping")) return;
    // If a sim is still active (abnormal exit path) ensure we flush and close.
    if (m_writer.is_open()) {
        flush();
        events::SimStopped fake{
            .sim_name        = "unknown",
            .sim_index       = u64(0),
            .total_sim_time  = f32(0),
            .total_ticks     = total_ticks(),
            .total_records   = total_records(),
            .dropped_records = dropped(),
            .wall_time       = std::chrono::system_clock::now()
        };
        m_writer.close(fake, dropped());
    }
    std::cout << "[Telemetry] App stopping -- writer closed.\n";
}

void TelemetryService::on_sim_started(const events::SimStarted& e) {
    if (!require_owner_thread("TelemetryService::on_sim_started")) return;
    m_ring.reset();
    m_total_records.store(u64(0), std::memory_order_relaxed);
    m_total_ticks.store(u64(0), std::memory_order_relaxed);
    std::scoped_lock lock(m_ext_mutex);
    m_ext_scratch.clear();

    const auto csv_path = m_writer.open(e);
    std::cout << std::format("[Telemetry] Recording '{}' -> {}\n",
        e.sim_name, csv_path.string());
}

void TelemetryService::on_sim_stopped(const events::SimStopped& e) {
    if (!require_owner_thread("TelemetryService::on_sim_stopped")) return;
    flush();

    // Flush ext records that accumulated since last flush.
    std::vector<TelemetryExtRecord> ext_records;
    {
        std::scoped_lock lock(m_ext_mutex);
        ext_records.swap(m_ext_scratch);
    }
    if (!ext_records.empty()) {
        m_writer.write_ext_batch(ext_records);
    }

    m_writer.close(e, dropped());
    std::cout << std::format(
        "[Telemetry] Run complete -- {} ticks  {} records  {} dropped\n",
        total_ticks(), total_records(), dropped());
}

u64 TelemetryService::flush() {
    if (!require_owner_thread("TelemetryService::flush")) return u64(0);
    if (!m_writer.is_open() || m_ring.empty()) return u64(0);

    const u64 n = m_ring.consume_all(m_flush_scratch);
    if (n > u64(0)) {
        m_writer.write_batch(
            std::span<const TelemetryRecord>{m_flush_scratch.data(),
                                              static_cast<std::size_t>(n)});
    }

    // Flush accumulated ext records while we're here.
    std::vector<TelemetryExtRecord> ext_records;
    {
        std::scoped_lock lock(m_ext_mutex);
        ext_records.swap(m_ext_scratch);
    }
    if (!ext_records.empty()) {
        m_writer.write_ext_batch(ext_records);
    }

    return n;
}

bool TelemetryService::require_owner_thread(std::string_view api_name) const {
    return !m_owner_guard || m_owner_guard(api_name);
}

} // namespace ndde::telemetry
