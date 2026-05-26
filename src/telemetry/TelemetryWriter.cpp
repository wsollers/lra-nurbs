// telemetry/TelemetryWriter.cpp

#include "telemetry/TelemetryWriter.hpp"
#include <cassert>
#include <chrono>
#include <format>
#include <iostream>

namespace ndde::telemetry {

// ── Helpers ───────────────────────────────────────────────────────────────────

std::string TelemetryWriter::iso8601_now() {
    const auto now = std::chrono::floor<std::chrono::seconds>(
        std::chrono::system_clock::now());
    return std::format("{:%FT%TZ}", now);
}

std::filesystem::path TelemetryWriter::make_stem(std::string_view sim_name,
                                                   u64 index) const {
    // Sanitise sim_name → lowercase_with_underscores
    std::string name{sim_name};
    for (char& c : name) {
        const auto uc = static_cast<byte>(c);
        c = std::isalnum(static_cast<int>(uc))
            ? static_cast<char>(std::tolower(static_cast<int>(uc)))
            : '_';
    }
    // Collapse runs of underscores
    name.erase(std::unique(name.begin(), name.end(),
                           [](char a, char b) { return a == '_' && b == '_'; }),
               name.end());
    if (!name.empty() && name.back() == '_') name.pop_back();
    if (name.empty()) name = "simulation";

    const auto now = std::chrono::floor<std::chrono::seconds>(
        std::chrono::system_clock::now());

    return m_output_dir / std::format("{}_{}_{:%Y%m%d_%H%M%S}", name, index, now);
}

// ── Public API ────────────────────────────────────────────────────────────────

std::filesystem::path TelemetryWriter::open(const events::SimStarted& event) {
    assert(!m_csv.is_open() && "TelemetryWriter::open called while already open");

    std::filesystem::create_directories(m_output_dir);

    const std::filesystem::path stem = make_stem(event.sim_name, event.sim_index);
    m_meta.csv_path     = std::filesystem::path(stem.string() + ".csv");
    m_meta.ext_csv_path = std::filesystem::path(stem.string() + "_ext.csv");
    m_meta.sim_name     = std::string(event.sim_name);
    m_meta.sim_index    = event.sim_index;
    m_meta.started_at   = iso8601_now();
    m_meta.total_ticks    = u64(0);
    m_meta.total_records  = u64(0);
    m_meta.total_ext      = u64(0);
    m_meta.dropped        = u64(0);

    m_csv.open(m_meta.csv_path, std::ios::out | std::ios::trunc);
    if (!m_csv.is_open()) {
        std::cerr << std::format("[TelemetryWriter] Failed to open: {}\n",
                                  m_meta.csv_path.string());
        return {};
    }
    m_csv << csv_header;

    m_ext_csv.open(m_meta.ext_csv_path, std::ios::out | std::ios::trunc);
    if (m_ext_csv.is_open())
        m_ext_csv << ext_csv_header;

    return m_meta.csv_path;
}

void TelemetryWriter::write_batch(std::span<const TelemetryRecord> records) {
    if (!m_csv.is_open() || records.empty()) return;

    for (const TelemetryRecord& r : records) {
        // One CSV row per record. Column order matches csv_header exactly.
        m_csv
            << r.tick          << ','
            << r.sim_time      << ','
            << r.wall_ms       << ','
            << r.packed_id     << ','
            << r.noise_sigma   << ','
            << r.speed         << ','
            << r.u             << ','
            << r.v             << ','
            << r.x             << ','
            << r.y             << ','
            << r.z             << ','
            << r.angle         << ','
            << r.geodesic_k    << ','
            << r.metric_factor << '\n';
        ++m_meta.total_records;
    }
}

void TelemetryWriter::write_ext_batch(std::span<const TelemetryExtRecord> records) {
    if (!m_ext_csv.is_open() || records.empty()) return;

    for (const TelemetryExtRecord& r : records) {
        m_ext_csv
            << r.tick              << ','
            << r.pursuer_packed_id << ','
            << r.target_packed_id  << ','
            << r.delay_u           << ','
            << r.delay_v           << '\n';
        ++m_meta.total_ext;
    }
}

void TelemetryWriter::close(const events::SimStopped& event, u64 dropped_count) {
    m_meta.stopped_at      = iso8601_now();
    m_meta.total_ticks     = event.total_ticks;
    m_meta.total_sim_time  = event.total_sim_time;
    m_meta.dropped         = dropped_count;

    close_streams();

    // Write markdown stub alongside the CSV.
    const std::filesystem::path md_path =
        std::filesystem::path(m_meta.csv_path.string().substr(
            0, m_meta.csv_path.string().size() - 4) + ".md");
    write_markdown_stub(m_meta);
    (void)md_path;
}

void TelemetryWriter::close_streams() {
    if (m_csv.is_open())     { m_csv.flush();     m_csv.close(); }
    if (m_ext_csv.is_open()) { m_ext_csv.flush(); m_ext_csv.close(); }
}

void TelemetryWriter::write_markdown_stub(const RunMetadata& meta) {
    if (meta.csv_path.empty()) return;

    const std::filesystem::path md_path =
        std::filesystem::path(meta.csv_path.string().substr(
            0, meta.csv_path.string().size() - 4) + ".md");

    std::ofstream md(md_path, std::ios::out | std::ios::trunc);
    if (!md.is_open()) {
        std::cerr << std::format("[TelemetryWriter] Failed to write metadata: {}\n",
                                  md_path.string());
        return;
    }

    md << std::format("# Telemetry Run: {}\n\n", meta.sim_name);
    md << "| Field | Value |\n";
    md << "|---|---|\n";
    md << std::format("| Simulation | {} |\n", meta.sim_name);
    md << std::format("| Index | {} |\n", meta.sim_index);
    md << std::format("| Started | {} |\n", meta.started_at);
    md << std::format("| Stopped | {} |\n", meta.stopped_at);
    md << std::format("| Total ticks | {} |\n", meta.total_ticks);
    md << std::format("| Total records | {} |\n", meta.total_records);
    md << std::format("| Ext records | {} |\n", meta.total_ext);
    md << std::format("| Dropped records | {} |\n", meta.dropped);
    md << std::format("| Sim time (s) | {:.3f} |\n", meta.total_sim_time);
    md << std::format("| CSV | {} |\n", meta.csv_path.filename().string());
    md << std::format("| Ext CSV | {} |\n", meta.ext_csv_path.filename().string());

    md << R"(
## Primary Record Schema (`TelemetryRecord`, 64 bytes)

| Offset | Column | Type | Description |
|---|---|---|---|
| 0 | `tick` | `u64` | Frame counter since sim start |
| 8 | `sim_time` | `f32` | Simulated seconds since sim start |
| 12 | `wall_ms` | `f32` | Wall-clock ms since sim start |
| 16 | `packed_id` | `u64` | `role(4b) \| raw_id(60b)` — decode: `role = packed_id >> 60`, `raw_id = packed_id & 0x0FFFFFFFFFFFFFFF` |
| 24 | `noise_sigma` | `f32` | Diffusion coefficient magnitude σ — 0 for deterministic particles |
| 28 | `speed` | `f32` | Parameter-space speed ‖(du/dt, dv/dt)‖ |
| 32 | `u` | `f32` | Surface parameter u |
| 36 | `v` | `f32` | Surface parameter v |
| 40 | `x` | `f32` | World position x |
| 44 | `y` | `f32` | World position y |
| 48 | `z` | `f32` | World position z |
| 52 | `angle` | `f32` | Heading in parameter space (radians) — `ParticleState::angle` |
| 56 | `geodesic_k` | `f32` | Geodesic curvature κ_g |
| 60 | `metric_factor` | `f32` | Reserved (1.0) — Phase 2: local conformal factor (1 + εh) |

## Extension Record Schema (`TelemetryExtRecord`, 32 bytes)

Sparse — emitted only for Chaser/Avoider particles when a target is active.
Join to primary on `tick` and `pursuer_packed_id == packed_id`.

| Offset | Column | Type | Description |
|---|---|---|---|
| 0 | `tick` | `u64` | Matches primary record tick |
| 8 | `pursuer_packed_id` | `u64` | packed_id of the chasing particle |
| 16 | `target_packed_id` | `u64` | packed_id of the target particle |
| 24 | `delay_u` | `f32` | Target's delayed u — what the pursuer reacted to |
| 28 | `delay_v` | `f32` | Target's delayed v |

## Python Quick-Start

```python
import pandas as pd

df  = pd.read_csv(")" << meta.csv_path.filename().string() << R"(")
ext = pd.read_csv(")" << meta.ext_csv_path.filename().string() << R"(")

# Decode role and raw id from packed_id
df['role']   = (df['packed_id'] >> 60).astype('uint8')
df['raw_id'] = df['packed_id'] & 0x0FFFFFFFFFFFFFFF

# Role names (extend if ParticleRole enum grows)
role_names = {0: 'Neutral', 1: 'Leader', 2: 'Chaser', 3: 'Avoider'}
df['role_name'] = df['role'].map(role_names)

# Heading vector in parameter space
import numpy as np
df['du'] = df['speed'] * np.cos(df['angle'])
df['dv'] = df['speed'] * np.sin(df['angle'])

# Angular velocity (heading rate) — finite difference per particle
df = df.sort_values(['raw_id', 'tick'])
df['d_angle'] = df.groupby('raw_id')['angle'].diff() / df.groupby('raw_id')['sim_time'].diff()
```
)";
}

} // namespace ndde::telemetry
