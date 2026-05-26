// engine/AppConfig.cpp
// JSON is isolated here — nothing else in the engine sees nlohmann.
#include "engine/AppConfig.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace ndde {

using json = nlohmann::json;

template<typename T>
static T jget(const json& j, const std::string& key, T def) {
    if (j.contains(key) && !j[key].is_null()) return j[key].get<T>();
    return def;
}

static Vec3 jvec3(const json& j, const std::string& key, Vec3 def) {
    if (j.contains(key) && j[key].is_array() && j[key].size() >= 3)
        return { j[key][0].get<f32>(), j[key][1].get<f32>(), j[key][2].get<f32>() };
    return def;
}

AppConfig AppConfig::load_or_default(const std::string& path) {
    AppConfig cfg{};

    std::ifstream file(path);
    if (!file.is_open()) {
        std::cout << "[Config] " << path << " not found — using defaults.\n";
        return cfg;
    }

    json j;
    try { file >> j; }
    catch (const json::parse_error& e) {
        throw std::runtime_error("[Config] JSON parse error in " + path + ": " + e.what());
    }

    std::cout << "[Config] Loaded " << path << "\n";

    if (j.contains("window")) {
        auto& w = j["window"];
        cfg.window.width  = jget<u32>(w, "width",  cfg.window.width);
        cfg.window.height = jget<u32>(w, "height", cfg.window.height);
        cfg.window.title  = jget<std::string>(w, "title", cfg.window.title);
    }
    if (j.contains("render")) {
        auto& r = j["render"];
        cfg.render.vsync                = jget<bool>(r, "vsync",      cfg.render.vsync);
        cfg.render.max_frames_in_flight = jget<u32>(r,  "max_frames_in_flight", cfg.render.max_frames_in_flight);
        cfg.render.threaded_presentation = jget<bool>(r, "threaded_presentation", cfg.render.threaded_presentation);
    }
    if (j.contains("camera")) {
        auto& c = j["camera"];
        cfg.camera.position  = jvec3(c, "position", cfg.camera.position);
        cfg.camera.target    = jvec3(c, "target",   cfg.camera.target);
        cfg.camera.fov       = jget<f32>(c, "fov",  cfg.camera.fov);
        cfg.camera.near_plane = jget<f32>(c, "near", cfg.camera.near_plane);
        cfg.camera.far_plane  = jget<f32>(c, "far",  cfg.camera.far_plane);
    }
    if (j.contains("simulation")) {
        auto& s = j["simulation"];
        cfg.simulation.tau           = jget<f32>(s, "tau",           cfg.simulation.tau);
        cfg.simulation.speed         = jget<f32>(s, "speed",         cfg.simulation.speed);
        cfg.simulation.tessellation  = jget<u32>(s, "tessellation",  cfg.simulation.tessellation);
        cfg.simulation.arena_size_mb = jget<u32>(s, "arena_size_mb", cfg.simulation.arena_size_mb);
        cfg.simulation.threaded_runtime = jget<bool>(s, "threaded_runtime", cfg.simulation.threaded_runtime);
    }
    if (j.contains("telemetry")) {
        auto& t = j["telemetry"];
        cfg.telemetry.enabled        = jget<bool>(t,        "enabled",        cfg.telemetry.enabled);
        cfg.telemetry.buffer_records = jget<u64>(t,         "buffer_records",  cfg.telemetry.buffer_records);
        cfg.telemetry.output_dir     = jget<std::string>(t, "output_dir",      cfg.telemetry.output_dir);
        cfg.telemetry.flush_periodic = jget<bool>(t,        "flush_periodic",  cfg.telemetry.flush_periodic);
        cfg.telemetry.flush_interval = jget<u64>(t,         "flush_interval",  cfg.telemetry.flush_interval);
    }
    cfg.assets_dir = jget<std::string>(j, "assets_dir", cfg.assets_dir);
    return cfg;
}

void AppConfig::save(const std::string& path) const {
    json j = {
        { "window",     { {"width",  window.width},  {"height", window.height}, {"title", window.title} }},
        { "render",     { {"vsync",  render.vsync},
                          {"max_frames_in_flight", render.max_frames_in_flight},
                          {"threaded_presentation", render.threaded_presentation} }},
        { "camera",     { {"position", {camera.position.x, camera.position.y, camera.position.z}},
                          {"target",   {camera.target.x,   camera.target.y,   camera.target.z}},
                          {"fov",  camera.fov}, {"near", camera.near_plane}, {"far", camera.far_plane} }},
        { "simulation", { {"tau", simulation.tau}, {"speed", simulation.speed},
                          {"tessellation", simulation.tessellation}, {"arena_size_mb", simulation.arena_size_mb},
                          {"threaded_runtime", simulation.threaded_runtime} }},
        { "telemetry",  { {"enabled",        telemetry.enabled},
                          {"buffer_records", telemetry.buffer_records},
                          {"output_dir",     telemetry.output_dir},
                          {"flush_periodic", telemetry.flush_periodic},
                          {"flush_interval", telemetry.flush_interval} }},
        { "assets_dir", assets_dir }
    };
    std::ofstream file(path);
    if (!file.is_open()) throw std::runtime_error("[Config] Cannot write to " + path);
    file << j.dump(4) << "\n";
    std::cout << "[Config] Saved " << path << "\n";
}

} // namespace ndde
