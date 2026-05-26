// simulation/events/EventBus.cpp
// make_record<E> specialisations — convert typed event structs to EventRecord.
// All specialisations are allocation-free.

#include "simulation/events/EventBus.hpp"
#include "simulation/events/SimEventTypes.hpp"
#include "simulation/events/EngineEventTypes.hpp"
#include "engine/events/AppEvent.hpp"   // AppStarted, AppStopping

namespace ndde::events {

// ── Engine-scoped events ──────────────────────────────────────────────────────

template <>
EventRecord EventBus::make_record(const AppStarted& /*e*/) {
    EventRecord r;
    r.kind = EventKind::AppStarted; r.severity = EventSeverity::Info;
    r.set_label("AppStarted");
    return r;
}

template <>
EventRecord EventBus::make_record(const AppStopping& /*e*/) {
    EventRecord r;
    r.kind = EventKind::AppStopping; r.severity = EventSeverity::Info;
    r.set_label("AppStopping");
    return r;
}

template <>
EventRecord EventBus::make_record(const SimSwitched& e) {
    EventRecord r;
    r.kind = EventKind::SimSwitched; r.severity = EventSeverity::Info;
    r.id_a = e.sim_index;
    r.set_label("SimSwitched");
    return r;
}

} // namespace ndde::events

// ── Sim-scoped events (ndde::simulation::events namespace) ───────────────────
// These specialisations must be in ndde::events because EventBus lives there,
// but the event types are in ndde::simulation::events.

namespace ndde::events {

using namespace ndde::simulation::events;

template <>
EventRecord EventBus::make_record(const ScenarioStarted& e) {
    return make_sim_started(e.scenario.value, e.sim_time, e.tick);
}

template <>
EventRecord EventBus::make_record(const ScenarioReset& e) {
    return make_sim_reset(e.scenario.value, e.sim_time, e.tick);
}

template <>
EventRecord EventBus::make_record(const ScenarioStopped& e) {
    EventRecord r;
    r.kind = EventKind::SimStopped; r.severity = EventSeverity::Info;
    r.sim_time = e.sim_time; r.tick = e.total_ticks;
    r.id_a = e.scenario.value;
    r.set_label("ScenarioStopped");
    return r;
}

template <>
EventRecord EventBus::make_record(const AgentSpawned& e) {
    return make_agent_spawned(e.packed_id, e.recipe.value, e.u, e.v, e.sim_time, e.tick);
}

template <>
EventRecord EventBus::make_record(const AgentDespawned& e) {
    EventRecord r;
    r.kind = EventKind::AgentDespawned; r.severity = EventSeverity::Notice;
    r.sim_time = e.sim_time; r.tick = e.tick; r.id_a = e.packed_id;
    r.set_label("AgentDespawned");
    return r;
}

template <>
EventRecord EventBus::make_record(const AgentCaptured& e) {
    return make_agent_captured(e.pursuer_id, e.prey_id, e.distance,
                                e.sim_time, e.tick);
}

template <>
EventRecord EventBus::make_record(const PerturbationFired& e) {
    return make_perturbation_fired(e.u, e.v, e.amplitude, e.sim_time, e.tick);
}

template <>
EventRecord EventBus::make_record(const PerturbationDecayed& e) {
    return make_perturbation_decayed(e.u, e.v, e.sim_time, e.tick);
}

template <>
EventRecord EventBus::make_record(const FieldAdded& e) {
    return make_field_added(e.field.value, e.sim_time, e.tick);
}

template <>
EventRecord EventBus::make_record(const FieldRemoved& e) {
    return make_field_removed(e.field.value, e.sim_time, e.tick);
}

} // namespace ndde::events
