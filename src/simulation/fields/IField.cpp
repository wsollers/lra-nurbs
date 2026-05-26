// simulation/fields/IField.cpp
#include "simulation/fields/IField.hpp"
#include <algorithm>

namespace ndde::simulation {

void FieldCompositor::add(std::shared_ptr<IField> field) {
    if (field) m_fields.push_back(std::move(field));
}

void FieldCompositor::remove(std::string_view name) {
    m_fields.erase(
        std::remove_if(m_fields.begin(), m_fields.end(),
            [name](const std::shared_ptr<IField>& f){
                return f && f->name() == name;
            }),
        m_fields.end());
}

void FieldCompositor::clear() { m_fields.clear(); }

glm::vec2 FieldCompositor::total_drift(const sim::ParticleState& state,
                                        const math::ISurface&     surface,
                                        f32                       t) const {
    glm::vec2 acc{f32(0), f32(0)};
    for (const auto& f : m_fields)
        if (f && f->active(t))
            acc += f->drift_contribution(state, surface, t);
    return acc;
}

f32 FieldCompositor::metric_factor(f32 u, f32 v, f32 t) const {
    f32 factor = f32(1);
    for (const auto& f : m_fields)
        if (f && f->active(t))
            factor *= f->metric_factor(u, v, t);
    return factor;
}

glm::vec2 FieldCompositor::diffusion_factor(const sim::ParticleState& state,
                                            const math::ISurface& surface,
                                            f32 t) const {
    glm::vec2 factor{f32(1), f32(1)};
    for (const auto& f : m_fields)
        if (f && f->active(t))
            factor *= f->diffusion_contribution(state, surface, t);
    return factor;
}

f32 FieldCompositor::surface_displacement(f32 u, f32 v, f32 t) const {
    f32 displacement = f32(0);
    for (const auto& f : m_fields)
        if (f && f->active(t))
            displacement += f->surface_displacement(u, v, t);
    return displacement;
}

std::vector<std::string> FieldCompositor::sweep_decayed(f32 t) {
    std::vector<std::string> removed;
    auto it = m_fields.begin();
    while (it != m_fields.end()) {
        if (*it && !(*it)->active(t)) {
            removed.emplace_back((*it)->name());
            it = m_fields.erase(it);
        } else {
            ++it;
        }
    }
    return removed;
}

} // namespace ndde::simulation
