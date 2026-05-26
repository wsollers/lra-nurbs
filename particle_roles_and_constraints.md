# Particle Path Functions, Roles, and Constraint System

> **Pedagogical note:** Every particle in this simulation is fundamentally a *curve* on a surface — a map $\gamma: [0, T] \to \mathcal{M}$ where $\mathcal{M}$ is your surface manifold. Changing "the function a particle uses" means changing the *vector field* that drives $\dot{\gamma}(t)$. The roles (leader/chaser) and constraints are then *boundary conditions* and *coupling terms* imposed on top of those individual dynamics. Think of it as: dynamics come first, then coupling, then constraints. This document follows that order.

---

## Table of Contents

1. [The Path Function Architecture](#1-the-path-function-architecture)
2. [Swapping a Particle's Path Function](#2-swapping-a-particles-path-function)
3. [Leader and Chaser Roles](#3-leader-and-chaser-roles)
4. [Dynamic Particle Addition via Hotkeys](#4-dynamic-particle-addition-via-hotkeys)
5. [The Constraint System](#5-the-constraint-system)
6. [Constraint Catalogue](#6-constraint-catalogue)
7. [Wiring Constraints to the Solver](#7-wiring-constraints-to-the-solver)
8. [Config and Serialization](#8-config-and-serialization)
9. [Migration Checklist](#9-migration-checklist)

---

## 1. The Path Function Architecture

### Geometric Intuition

A particle moving on a surface traces a curve $\gamma(t)$. At each moment, the surface's geometry tells the particle *how* its velocity must be projected back onto the tangent plane — this is the **exponential map** and **parallel transport** machinery you will eventually formalize. For now, the particle holds a 2D parameter position $(u, v) \in D$ and a *velocity function* that says: given the current state, what is $(\dot{u}, \dot{v})$?

That velocity function is what we are making swappable.

### The `PathFunction` Interface

```cpp
// src/particles/PathFunction.hpp
#pragma once
#include <glm/glm.hpp>
#include <string>

struct ParticleState {
    glm::vec2 uv;           // parameter-space position (u, v)
    glm::vec2 uv_vel;       // parameter-space velocity
    float     time;         // current simulation time
    uint32_t  id;           // unique particle ID
};

// Abstract base — every path function implements this
class PathFunction {
public:
    virtual ~PathFunction() = default;

    // Core: given state, return (du/dt, dv/dt)
    // The solver calls this every tick.
    [[nodiscard]] virtual glm::vec2 velocity(const ParticleState& state) const = 0;

    // Human-readable name (for UI / config serialization)
    [[nodiscard]] virtual std::string name() const = 0;

    // Parameter domain clamping hint (optional override)
    virtual bool wraps_u() const { return false; }
    virtual bool wraps_v() const { return false; }
};
```

### The `PathFunctionRegistry`

```cpp
// src/particles/PathFunctionRegistry.hpp
#pragma once
#include "PathFunction.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <functional>

class PathFunctionRegistry {
public:
    using Factory = std::function<std::unique_ptr<PathFunction>()>;

    void registerFunction(const std::string& name, Factory factory);

    [[nodiscard]] std::unique_ptr<PathFunction>
    create(const std::string& name) const;

    [[nodiscard]] std::vector<std::string> allNames() const;

    static PathFunctionRegistry& global(); // singleton accessor
};
```

```cpp
// src/particles/PathFunctionRegistry.cpp
#include "PathFunctionRegistry.hpp"

// Register all built-in path functions at startup:
// (call this from main() or your App::init())
void registerBuiltinPathFunctions(PathFunctionRegistry& reg) {
    reg.registerFunction("BrownianMotion",   []{ return std::make_unique<BrownianMotionPath>(); });
    reg.registerFunction("GeodesicStraight", []{ return std::make_unique<GeodesicPath>(); });
    reg.registerFunction("SinusoidalOrbit",  []{ return std::make_unique<SinusoidalOrbitPath>(); });
    reg.registerFunction("Pursuit",          []{ return std::make_unique<PursuitPath>(); });
    reg.registerFunction("DelayPursuit",     []{ return std::make_unique<DelayPursuitPath>(); });
    // Add your own here as you build them
}
```

---

## 2. Swapping a Particle's Path Function

### The `Particle` Struct

```cpp
// src/particles/Particle.hpp
#pragma once
#include "PathFunction.hpp"
#include "Role.hpp"
#include <memory>

struct Particle {
    uint32_t                        id;
    ParticleState                   state;
    std::unique_ptr<PathFunction>   pathFunction;
    Role                            role;          // see Section 3
    glm::vec4                       color;
    bool                            active = true;

    // Swap the path function at runtime — safe to call mid-simulation
    void setPathFunction(std::unique_ptr<PathFunction> fn) {
        pathFunction = std::move(fn);
    }
};
```

### Swapping at Runtime

```cpp
// Anywhere in your simulation loop or UI handler:
auto& particle = sim.getParticle(id);

// Swap to Brownian motion
particle.setPathFunction(
    PathFunctionRegistry::global().create("BrownianMotion")
);

// Or swap to delay-pursuit
particle.setPathFunction(
    PathFunctionRegistry::global().create("DelayPursuit")
);
```

The old function is destroyed immediately (RAII). The new function takes effect on the next solver tick — no restart required.

### Provided Path Functions

#### `BrownianMotionPath`

$$(\dot{u}, \dot{v}) = \sigma \cdot \xi(t), \quad \xi \sim \mathcal{N}(0, I_2)$$

```cpp
class BrownianMotionPath : public PathFunction {
    float sigma = 0.5f; // diffusion coefficient
public:
    glm::vec2 velocity(const ParticleState&) const override {
        // Uses mymath::sqrt, feeds into your custom math layer
        return sigma * glm::vec2(rng.gaussian(), rng.gaussian());
    }
    std::string name() const override { return "BrownianMotion"; }
};
```

#### `GeodesicPath`

Moves along a fixed heading in parameter space. On the torus, straight lines in $(u,v)$ space are geodesics (the torus metric is flat in parameter space):

$$(\dot{u}, \dot{v}) = (\cos\theta, \sin\theta)$$

```cpp
class GeodesicPath : public PathFunction {
    float theta = 0.0f; // heading angle
public:
    glm::vec2 velocity(const ParticleState&) const override {
        return glm::vec2(mymath::cos(theta), mymath::sin(theta));
    }
    std::string name() const override { return "GeodesicStraight"; }
};
```

#### `PursuitPath`

Requires access to a target position — see Section 3 for how target injection works.

#### `DelayPursuitPath`

Like pursuit, but chases where the target *was* $\tau$ seconds ago. Requires a history buffer — see Section 3.

---

## 3. Leader and Chaser Roles

### Geometric Intuition

The delay-pursuit problem on a manifold is a *differential delay equation* (DDE) on the surface. The chaser's velocity field depends not on the current state of the leader but on a *past* state — the leader's position at time $t - \tau$. The "memory" of the system is geometric: it lives on the manifold, and distances are measured in the surface metric, not Euclidean space.

### The `Role` Type

```cpp
// src/particles/Role.hpp
#pragma once
#include <cstdint>

enum class RoleType : uint8_t {
    Free,       // No role — follows own PathFunction only
    Leader,     // Drives its own path; other particles may reference it
    Chaser,     // Modifies its path based on one or more leaders
};

struct Role {
    RoleType    type        = RoleType::Free;
    uint32_t    target_id   = UINT32_MAX;   // ID of the particle this chaser follows
    float       delay_tau   = 0.0f;         // DDE delay (seconds); 0 = no delay
};
```

### The Leader's History Buffer

A leader must record its trajectory so chasers can query its past position:

```cpp
// src/particles/TrajectoryBuffer.hpp
#pragma once
#include <glm/glm.hpp>
#include <deque>

struct TimestampedPosition {
    float     time;
    glm::vec2 uv;
};

class TrajectoryBuffer {
    std::deque<TimestampedPosition> history;
    float                           max_duration; // keep this many seconds

public:
    explicit TrajectoryBuffer(float max_duration = 10.0f)
        : max_duration(max_duration) {}

    void push(float t, glm::vec2 uv) {
        history.push_back({t, uv});
        // Prune old entries
        while (!history.empty() &&
               t - history.front().time > max_duration) {
            history.pop_front();
        }
    }

    // Return position at time t_query (linear interpolation)
    // Returns nullopt if t_query is before the buffer starts
    [[nodiscard]] std::optional<glm::vec2>
    positionAt(float t_query) const;
};
```

```cpp
// Linear interpolation implementation
std::optional<glm::vec2>
TrajectoryBuffer::positionAt(float t_query) const {
    if (history.size() < 2) return std::nullopt;
    if (t_query < history.front().time) return std::nullopt;

    for (size_t i = 1; i < history.size(); ++i) {
        if (history[i].time >= t_query) {
            float dt  = history[i].time - history[i-1].time;
            float alpha = (t_query - history[i-1].time) / dt;
            return glm::mix(history[i-1].uv, history[i].uv, alpha);
        }
    }
    return history.back().uv; // clamp to most recent
}
```

### The `DelayPursuitPath` Implementation

```cpp
class DelayPursuitPath : public PathFunction {
    const TrajectoryBuffer* leaderHistory = nullptr; // injected externally
    float tau   = 1.0f;   // pursuit delay (seconds)
    float speed = 1.0f;   // pursuit speed

public:
    void setLeaderHistory(const TrajectoryBuffer* buf, float delay) {
        leaderHistory = buf;
        tau = delay;
    }

    glm::vec2 velocity(const ParticleState& state) const override {
        if (!leaderHistory) return glm::vec2(0.0f);

        // Query where the leader was at (t - tau)
        auto target = leaderHistory->positionAt(state.time - tau);
        if (!target) return glm::vec2(0.0f); // buffer not yet full

        // Direction from current position toward delayed target
        glm::vec2 delta = *target - state.uv;

        // On a torus: wrap the delta to the shortest path
        // (parameter space is periodic in both u and v)
        delta = wrapTorusDelta(delta);

        float dist = glm::length(delta);
        if (dist < 1e-6f) return glm::vec2(0.0f);

        return speed * (delta / dist); // unit direction * speed
    }

    std::string name() const override { return "DelayPursuit"; }
    bool wraps_u() const override { return true; }
    bool wraps_v() const override { return true; }

private:
    static glm::vec2 wrapTorusDelta(glm::vec2 d) {
        // Shortest path on torus — wrap each component into [-pi, pi]
        constexpr float TWO_PI = 6.28318530718f;
        constexpr float PI     = 3.14159265359f;
        if (d.x >  PI) d.x -= TWO_PI;
        if (d.x < -PI) d.x += TWO_PI;
        if (d.y >  PI) d.y -= TWO_PI;
        if (d.y < -PI) d.y += TWO_PI;
        return d;
    }
};
```

### Wiring Roles in the Simulation

```cpp
// In SimulationManager::update(float dt):
for (auto& particle : particles) {
    if (particle.role.type == RoleType::Leader) {
        // Record position into history buffer
        leaderBuffers[particle.id].push(
            particle.state.time,
            particle.state.uv
        );
    }
}

for (auto& particle : particles) {
    if (particle.role.type == RoleType::Chaser) {
        uint32_t target = particle.role.target_id;
        // Inject leader history into the chaser's path function
        if (auto* path = dynamic_cast<DelayPursuitPath*>(
                particle.pathFunction.get())) {
            path->setLeaderHistory(
                &leaderBuffers[target],
                particle.role.delay_tau
            );
        }
    }
}
```

---

## 4. Dynamic Particle Addition via Hotkeys

### Design Principle

When you add a particle via hotkey, the system:
1. Assigns it a fresh unique ID.
2. Spawns it at a configurable initial position (random, near an existing particle, or at cursor position).
3. Assigns it a default `PathFunction` and `Role`.
4. Optionally attaches a set of `Constraint`s (see Section 5).

### `HotkeyConfig`

```cpp
// src/input/HotkeyConfig.hpp
#pragma once
#include "particles/Role.hpp"
#include "constraints/Constraint.hpp"
#include <vector>
#include <string>

struct SpawnConfig {
    std::string                             pathFunctionName = "BrownianMotion";
    Role                                    role;
    std::vector<std::shared_ptr<Constraint>> constraints;

    // Spawn position strategy
    enum class SpawnStrategy { Random, NearLeader, NearCursor, Fixed };
    SpawnStrategy   spawnStrategy = SpawnStrategy::Random;
    glm::vec2       fixedUV       = {0.0f, 0.0f}; // used if Fixed
};
```

### Hotkey Handler

```cpp
// src/input/InputHandler.cpp

void InputHandler::onKeyPress(int key) {
    switch (key) {
    case KEY_L: {
        // Spawn a new Leader
        SpawnConfig cfg;
        cfg.pathFunctionName = "GeodesicStraight";
        cfg.role.type        = RoleType::Leader;
        sim.spawnParticle(cfg);
        break;
    }
    case KEY_C: {
        // Spawn a new Chaser targeting the most recently added Leader
        SpawnConfig cfg;
        cfg.pathFunctionName    = "DelayPursuit";
        cfg.role.type           = RoleType::Chaser;
        cfg.role.target_id      = sim.mostRecentLeaderId();
        cfg.role.delay_tau      = 1.5f; // 1.5 second delay

        // Attach constraints — see Section 6
        cfg.constraints.push_back(
            std::make_shared<MaxDistanceToLeaderConstraint>(3.14159f)
        );
        sim.spawnParticle(cfg);
        break;
    }
    case KEY_F: {
        // Spawn a free Brownian particle with no role or constraints
        SpawnConfig cfg;
        sim.spawnParticle(cfg);
        break;
    }
    case KEY_DELETE: {
        sim.removeLastParticle();
        break;
    }
    }
}
```

### `SimulationManager::spawnParticle`

```cpp
uint32_t SimulationManager::spawnParticle(const SpawnConfig& cfg) {
    Particle p;
    p.id           = nextId++;
    p.role         = cfg.role;
    p.pathFunction = PathFunctionRegistry::global().create(cfg.pathFunctionName);
    p.color        = roleColor(cfg.role.type);
    p.state.uv     = chooseSpawnPosition(cfg);
    p.state.time   = currentTime;

    // Attach constraints
    for (auto& c : cfg.constraints) {
        constraintSystem.attach(p.id, c);
    }

    particles.push_back(std::move(p));
    return p.id;
}
```

---

## 5. The Constraint System

### Geometric Intuition

A constraint is a *relationship* between particles. Mathematically, a pairwise constraint is a condition of the form:

$$C(p_i, p_j) \leq 0 \quad \text{or} \quad C(p_i, p_j) = 0$$

where $C$ measures something geometric — distance on the surface, angle, arc length, etc. When the constraint is violated, a *correction velocity* is applied to push the particles back into the feasible region. This is the projection step: project the unconstrained dynamics onto the constraint manifold.

All distances below are measured in **parameter space** with torus wrapping. You can upgrade these to geodesic distances on the surface once your Riemannian metric machinery is in place.

### The `Constraint` Interface

```cpp
// src/constraints/Constraint.hpp
#pragma once
#include "particles/Particle.hpp"
#include <vector>
#include <string>

// A constraint acts on a named set of particles and returns
// a correction velocity for each involved particle.
class Constraint {
public:
    virtual ~Constraint() = default;

    // IDs of the particles this constraint involves
    [[nodiscard]] virtual std::vector<uint32_t>
    involvedParticles() const = 0;

    // Evaluate constraint violation and return correction velocities
    // corrections[i] corresponds to involvedParticles()[i]
    virtual void apply(
        std::vector<Particle*>      involved,
        float                       dt,
        std::vector<glm::vec2>&     corrections  // output: delta_uv per particle
    ) const = 0;

    [[nodiscard]] virtual std::string name() const = 0;
    [[nodiscard]] virtual bool        isSatisfied(
        const std::vector<Particle*>& involved
    ) const = 0;
};
```

### The `ConstraintSystem`

```cpp
// src/constraints/ConstraintSystem.hpp
#pragma once
#include "Constraint.hpp"
#include <unordered_map>
#include <vector>
#include <memory>

class ConstraintSystem {
    // Map from particle ID → list of constraints that involve it
    std::unordered_map<uint32_t, std::vector<std::shared_ptr<Constraint>>> registry;

public:
    void attach(uint32_t particleId, std::shared_ptr<Constraint> c);
    void detach(uint32_t particleId, const std::string& constraintName);
    void detachAll(uint32_t particleId);

    // Apply all constraints — call AFTER the unconstrained velocity step
    void applyAll(std::vector<Particle>& particles, float dt);
};
```

```cpp
// src/constraints/ConstraintSystem.cpp
void ConstraintSystem::applyAll(std::vector<Particle>& particles, float dt) {
    // Build ID → pointer map for fast lookup
    std::unordered_map<uint32_t, Particle*> pmap;
    for (auto& p : particles) pmap[p.id] = &p;

    // Collect unique constraints (avoid double-applying pairwise ones)
    std::set<Constraint*> visited;

    for (auto& [id, constraints] : registry) {
        for (auto& c : constraints) {
            if (visited.count(c.get())) continue;
            visited.insert(c.get());

            // Gather involved particles
            auto ids = c->involvedParticles();
            std::vector<Particle*> involved;
            for (auto pid : ids) {
                if (pmap.count(pid)) involved.push_back(pmap[pid]);
            }
            if (involved.size() != ids.size()) continue; // particle was removed

            // Get corrections
            std::vector<glm::vec2> corrections(involved.size(), glm::vec2(0.0f));
            c->apply(involved, dt, corrections);

            // Apply corrections to particle positions
            for (size_t i = 0; i < involved.size(); ++i) {
                involved[i]->state.uv += corrections[i];
            }
        }
    }
}
```

---

## 6. Constraint Catalogue

### 6.1 Chaser Follows Leader's Historical Path

**Intent:** The chaser is "attracted" to the trail left by the leader. This is encoded in the `DelayPursuitPath` itself (Section 3) — it is a *path function*, not a separate constraint, because it drives the velocity. No additional constraint object needed.

If you want a *hard* constraint that snaps the chaser to the leader's trail at every step, implement:

```cpp
class SnapToTrailConstraint : public Constraint {
    const TrajectoryBuffer* leaderHistory;
    float tau;
    float snapStrength; // 0 = soft attraction, 1 = hard snap

public:
    void apply(std::vector<Particle*> involved, float dt,
               std::vector<glm::vec2>& corrections) const override {
        auto* chaser = involved[0];
        auto target = leaderHistory->positionAt(chaser->state.time - tau);
        if (!target) return;

        glm::vec2 delta = wrapTorusDelta(*target - chaser->state.uv);
        corrections[0] += snapStrength * delta;
    }
    std::string name() const override { return "SnapToTrail"; }
};
```

---

### 6.2 Chasers Must Stay Within Distance `d` of Each Other

**Formal predicate:**

$$\forall i, j \in \text{Chasers},\; i \neq j : d_{\mathcal{M}}(p_i, p_j) \leq d_{\max}$$

```cpp
class ChaserCohesionConstraint : public Constraint {
    std::vector<uint32_t> chaserIds;
    float d_max;            // maximum allowed separation
    float stiffness = 0.5f; // correction strength in [0, 1]

public:
    ChaserCohesionConstraint(std::vector<uint32_t> ids, float max_dist)
        : chaserIds(std::move(ids)), d_max(max_dist) {}

    void apply(std::vector<Particle*> involved, float dt,
               std::vector<glm::vec2>& corrections) const override {
        // For each pair (i, j), if distance > d_max, push them together
        for (size_t i = 0; i < involved.size(); ++i) {
            for (size_t j = i + 1; j < involved.size(); ++j) {
                glm::vec2 delta = wrapTorusDelta(
                    involved[j]->state.uv - involved[i]->state.uv
                );
                float dist = glm::length(delta);
                if (dist > d_max && dist > 1e-6f) {
                    float violation = dist - d_max;
                    glm::vec2 push = stiffness * violation * (delta / dist);
                    corrections[i] += push;       // i moves toward j
                    corrections[j] -= push;       // j moves toward i
                }
            }
        }
    }

    bool isSatisfied(const std::vector<Particle*>& involved) const override {
        for (size_t i = 0; i < involved.size(); ++i)
            for (size_t j = i+1; j < involved.size(); ++j) {
                float d = glm::length(wrapTorusDelta(
                    involved[j]->state.uv - involved[i]->state.uv));
                if (d > d_max) return false;
            }
        return true;
    }

    std::string name() const override { return "ChaserCohesion"; }
    std::vector<uint32_t> involvedParticles() const override { return chaserIds; }
};
```

---

### 6.3 Leaders Must Stay No Closer Than `d_min` (Mutual Exclusion)

**Formal predicate:**

$$\forall i, j \in \text{Leaders},\; i \neq j : d_{\mathcal{M}}(p_i, p_j) \geq d_{\min}$$

```cpp
class LeaderSeparationConstraint : public Constraint {
    std::vector<uint32_t> leaderIds;
    float d_min;
    float stiffness = 0.8f;

public:
    void apply(std::vector<Particle*> involved, float dt,
               std::vector<glm::vec2>& corrections) const override {
        for (size_t i = 0; i < involved.size(); ++i) {
            for (size_t j = i + 1; j < involved.size(); ++j) {
                glm::vec2 delta = wrapTorusDelta(
                    involved[j]->state.uv - involved[i]->state.uv
                );
                float dist = glm::length(delta);
                if (dist < d_min && dist > 1e-6f) {
                    float violation = d_min - dist;
                    glm::vec2 push = stiffness * violation * (delta / dist);
                    corrections[i] -= push; // push i away from j
                    corrections[j] += push; // push j away from i
                }
            }
        }
    }

    std::string name() const override { return "LeaderSeparation"; }
    std::vector<uint32_t> involvedParticles() const override { return leaderIds; }
};
```

---

### 6.4 Leaders Must Stay Closer Than `d_max` (Attraction / Flocking)

**Formal predicate:**

$$\forall i, j \in \text{Leaders},\; i \neq j : d_{\mathcal{M}}(p_i, p_j) \leq d_{\max}$$

This is the same structure as `ChaserCohesionConstraint` applied to leaders. You can use a single `MaxPairwiseDistanceConstraint` parameterized by a set of IDs:

```cpp
class MaxPairwiseDistanceConstraint : public Constraint {
    std::vector<uint32_t> ids;
    float d_max;
    float stiffness;
    std::string label;
public:
    MaxPairwiseDistanceConstraint(
        std::vector<uint32_t> ids,
        float max_dist,
        float stiffness = 0.5f,
        std::string label = "MaxPairwiseDist")
        : ids(std::move(ids)), d_max(max_dist),
          stiffness(stiffness), label(std::move(label)) {}

    // ... apply() identical to ChaserCohesionConstraint above ...
    std::string name() const override { return label; }
    std::vector<uint32_t> involvedParticles() const override { return ids; }
};
```

Usage:
```cpp
// Leaders must stay within pi of each other (half the torus circumference)
constraintSystem.attach(leader1_id,
    std::make_shared<MaxPairwiseDistanceConstraint>(
        std::vector<uint32_t>{leader1_id, leader2_id},
        3.14159f, 0.6f, "LeaderAttraction"
    )
);
```

---

### 6.5 Chaser Must Stay Within `d` of Its Specific Leader

**Formal predicate:**

$$\forall c \in \text{Chasers} : d_{\mathcal{M}}(c, \text{leader}(c)) \leq d_{\max}$$

```cpp
class MaxDistanceToLeaderConstraint : public Constraint {
    uint32_t chaser_id;
    uint32_t leader_id;
    float    d_max;
    float    stiffness = 0.7f;

public:
    MaxDistanceToLeaderConstraint(uint32_t chaser, uint32_t leader, float max_dist)
        : chaser_id(chaser), leader_id(leader), d_max(max_dist) {}

    void apply(std::vector<Particle*> involved, float dt,
               std::vector<glm::vec2>& corrections) const override {
        // involved[0] = chaser, involved[1] = leader (by convention of involvedParticles)
        glm::vec2 delta = wrapTorusDelta(
            involved[1]->state.uv - involved[0]->state.uv
        );
        float dist = glm::length(delta);
        if (dist > d_max && dist > 1e-6f) {
            float violation = dist - d_max;
            glm::vec2 push = stiffness * violation * (delta / dist);
            corrections[0] += push; // pull chaser toward leader
            // leader is NOT corrected — leader is autonomous
        }
    }

    std::string name() const override { return "MaxDistToLeader"; }
    std::vector<uint32_t> involvedParticles() const override {
        return {chaser_id, leader_id};
    }
};
```

---

### 6.6 Constraint Parameter Summary

| Constraint | Key Parameter | Effect |
|---|---|---|
| `SnapToTrailConstraint` | `tau`, `snapStrength` | Hard/soft attraction to leader's past trail |
| `ChaserCohesionConstraint` | `d_max` | Chasers flock — can't stray apart |
| `LeaderSeparationConstraint` | `d_min` | Leaders repel — maintain personal space |
| `MaxPairwiseDistanceConstraint` | `d_max` | Any group must stay within range |
| `MaxDistanceToLeaderConstraint` | `d_max` | Individual chaser leashed to leader |

---

## 7. Wiring Constraints to the Solver

The constraint solve happens **after** the unconstrained Euler/RK4 step:

```cpp
// SimulationManager::update(float dt)
void SimulationManager::update(float dt) {
    // --- Step 1: Unconstrained velocity integration ---
    for (auto& p : particles) {
        if (!p.active) continue;
        glm::vec2 vel = p.pathFunction->velocity(p.state);
        p.state.uv  += vel * dt;         // Euler step (replace with RK4 later)
        p.state.time += dt;
        wrapTorus(p.state.uv);           // enforce periodic boundary
    }

    // --- Step 2: Record leader history ---
    for (auto& p : particles) {
        if (p.role.type == RoleType::Leader) {
            leaderBuffers[p.id].push(p.state.time, p.state.uv);
        }
    }

    // --- Step 3: Apply constraints (projection step) ---
    constraintSystem.applyAll(particles, dt);

    // --- Step 4: Re-wrap after constraint corrections ---
    for (auto& p : particles) wrapTorus(p.state.uv);
}
```

> **Note on multiple constraint iterations:** A single pass may not fully satisfy all constraints when they conflict (e.g., cohesion vs. leash). Add an outer loop:
>
> ```cpp
> for (int iter = 0; iter < MAX_CONSTRAINT_ITERS; ++iter) {
>     constraintSystem.applyAll(particles, dt);
>     if (constraintSystem.allSatisfied(particles)) break;
> }
> ```
> This is the foundation of Position-Based Dynamics (PBD), the same technique used in game physics engines.

---

## 8. Config and Serialization

So you can save and restore simulation states to your git repository:

```json
{
  "particles": [
    {
      "id": 0,
      "role": "Leader",
      "pathFunction": "GeodesicStraight",
      "spawnUV": [1.5708, 0.0],
      "color": [1.0, 0.4, 0.1, 1.0],
      "constraints": []
    },
    {
      "id": 1,
      "role": "Chaser",
      "target_id": 0,
      "delay_tau": 1.5,
      "pathFunction": "DelayPursuit",
      "spawnUV": [0.0, 0.0],
      "color": [0.2, 0.6, 1.0, 1.0],
      "constraints": [
        { "type": "MaxDistToLeader", "d_max": 3.14159, "stiffness": 0.7 }
      ]
    }
  ],
  "surface": "Torus",
  "dt": 0.016
}
```

---

## 9. Migration Checklist

| Step | Task | Done? |
|---|---|---|
| 1 | Create `PathFunction` base class and `PathFunctionRegistry` | ☐ |
| 2 | Refactor existing particle to hold `unique_ptr<PathFunction>` | ☐ |
| 3 | Implement `BrownianMotionPath`, `GeodesicPath` | ☐ |
| 4 | Implement `TrajectoryBuffer` with interpolation | ☐ |
| 5 | Add `Role` struct to `Particle` | ☐ |
| 6 | Implement `DelayPursuitPath` with leader history injection | ☐ |
| 7 | Create `Constraint` base class and `ConstraintSystem` | ☐ |
| 8 | Implement `ChaserCohesionConstraint` | ☐ |
| 9 | Implement `LeaderSeparationConstraint` | ☐ |
| 10 | Implement `MaxDistanceToLeaderConstraint` | ☐ |
| 11 | Wire constraint solve into `SimulationManager::update` | ☐ |
| 12 | Add hotkey handler with `SpawnConfig` | ☐ |
| 13 | Add JSON serialization for particle configs | ☐ |
| 14 | Test: spawn leader + chaser via hotkey, verify DDE delay visually | ☐ |
