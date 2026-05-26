# Allocation Policy

The current hard rule is enforced by `tools/check_allocation_policy.py`:

- raw `new`
- raw `delete`
- `malloc`
- `calloc`
- `realloc`
- `free`
- `std::make_unique`
- direct `std::unique_ptr<T>` ownership

These are allowed only inside the central memory package.

The public allocation surface is `memory::MemoryService`. Lower-level objects
such as `BufferManager`, arena resources, and future PMR resources are
implementation details behind the service. Engine/app code should request
storage by lifetime first:

- `memory.frame()`
- `memory.view()`
- `memory.simulation()`
- `memory.cache()`
- `memory.history()`
- `memory.persistent()`

`std::string` still exists in app/runtime code for labels, metadata, and user
text. Direct ownership through `std::unique_ptr` should stay inside the memory
package, where it implements `memory::Unique`. Direct `std::vector` use should
stay out of hot-path/runtime code; use the policy aliases instead.

All policy vector aliases in `memory/Containers.hpp` are now
`std::pmr::vector<T>`. Vectors should be created through the appropriate
`MemoryService` scope when they need to bind to an engine arena:

- `memory.frame().make_vector<T>()`
- `memory.view().make_vector<T>()`
- `memory.simulation().make_vector<T>()`
- `memory.cache().make_vector<T>()`
- `memory.history().make_vector<T>()`
- `memory.persistent().make_vector<T>()`

Long-lived owner types that are default-constructed before receiving a service
reference expose explicit bind methods, for example `SurfaceMeshCache::bind_memory`,
`ParticleSystem::bind_memory`, and service `set_memory_service` methods. A scope
must not be reset while objects allocated from that scope are still alive.

Cross-frame and cross-thread snapshots are not frame scratch. If a snapshot is
cached by a runtime, published through a mailbox, or may be read after
`memory.frame().begin_frame()` resets the frame scope, it must use
`memory::PersistentVector<T>`, another appropriate non-frame policy alias, or a
compact resource ID. `memory::FrameVector<T>` is only for data consumed before
the next frame reset.

Object ownership for simulation-runtime polymorphic objects should use
`memory::Unique<T>`, normally created by the appropriate scope:

- `memory.simulation().make_unique<T>()`
- `memory.history().make_unique<T>()`
- `memory.persistent().make_unique<T>()`

Unlike the earlier bridge API, scope `make_unique` now constructs the object in
the scope's PMR resource. The deleter calls the concrete destructor and returns
storage to that same resource. For monotonic arenas, individual deallocation is
cheap/no-op and bulk release still happens at scope reset. Existing migrated
owners include simulation runtimes, active simulations, surfaces, particle
behavior stacks, wrapped equations, particle constraints, pair constraints,
goals, and history buffers.

Recommended next enforcement stages:

1. Keep the raw allocation ban always on.
2. Use the policy aliases in `memory/Containers.hpp` for dynamic arrays:
   `FrameVector<T>` for per-frame scratch,
   `ViewVector<T>` for render/input view lifetime,
   `SimVector<T>` for simulation-instance lifetime,
   `CacheVector<T>` for derived surface/geometry caches,
   `HistoryVector<T>` for trails, delay buffers, replay/export history,
   and `PersistentVector<T>` for app/service/session lifetime.
3. Protect migrated hot-path files with `tools/check_hot_path_container_policy.py`.
4. Keep `std::unique_ptr` implementation detail usage isolated to
   `src/memory/Unique.hpp`.
5. Leave configuration and scalar/string metadata on ordinary STL until their
   lifetime pressure justifies migration.

Currently protected migrated areas include render packets, interaction/picking state,
view registration and mouse state, surface mesh caches, particle/trail/swarm state,
history buffers, simulation context command queues, engine panels/hotkeys, scoped
service handles, simulation runtime registry storage, and scene snapshots.
