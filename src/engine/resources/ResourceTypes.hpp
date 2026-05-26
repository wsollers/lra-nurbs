#pragma once
// engine/resources/ResourceTypes.hpp
// Addressable runtime resource descriptors and payload metadata.

#include "engine/RuntimeIds.hpp"
#include "engine/diagnostics/DiagnosticsTypes.hpp"
#include "memory/MemoryService.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace ndde {

struct ResourceKey {
    std::string_view value{};

    friend constexpr bool operator==(ResourceKey, ResourceKey) noexcept = default;
};

enum class ResourceKind : u8 {
    Unknown,
    CpuMesh,
    RenderSnapshot,
    TextureImage,
    ShaderModule,
    Material,
    ColorMap,
    Font,
    ScenarioAsset,
    GpuUpload,
    CaptureArtifact,
    FileArtifact,
    TelemetryArtifact,
    SolverCache,
    WorkerResult
};

enum class ResourceOrigin : u8 {
    Builtin,
    ConstexprRegistered,
    FilePath,
    Generated,
    Capture,
    External
};

enum class ResourceOwner : u8 {
    Engine,
    Simulation,
    Renderer,
    Capture,
    Telemetry,
    Logger,
    Worker,
    External
};

enum class ResourceLifetime : u8 {
    Frame,
    View,
    Simulation,
    Cache,
    History,
    Persistent,
    ExternalFile
};

enum class ResourceState : u8 {
    Reserved,
    Pending,
    Ready,
    Adopted,
    Released,
    Failed
};

enum class ResourceLoadErrorCode : u8 {
    Unknown,
    InvalidHandle,
    NoPathRegistered,
    FileNotFound,
    AlreadyReleased
};

struct ResourceLoadError {
    ResourceLoadErrorCode code = ResourceLoadErrorCode::Unknown;
    ResourceHandle handle = {};
    std::filesystem::path path;
};

struct ResourceRegistration {
    ResourceHandle handle = {};
    ResourceKey key = {};
    ResourceKind kind = ResourceKind::Unknown;
    ResourceOrigin origin = ResourceOrigin::Generated;
    ResourceOwner owner = ResourceOwner::Engine;
    ResourceLifetime lifetime = ResourceLifetime::Persistent;
    ComponentId producer = ids::unknown_component;
};

struct ResourceDescriptor {
    ResourceId id = {};
    ResourceHandle handle = {};
    ResourceKind kind = ResourceKind::Unknown;
    ResourceOrigin origin = ResourceOrigin::Generated;
    ResourceOwner owner = ResourceOwner::Engine;
    ResourceLifetime lifetime = ResourceLifetime::Cache;
    ResourceState state = ResourceState::Reserved;
    ComponentId producer = ids::unknown_component;
    RuntimeNodeId source_node = {};
    u64 byte_count = u64(0);
    ResourceGeneration generation = {};
    std::optional<DiagnosticId> diagnostic;
};

struct CpuMeshResource {
    ResourceId id = {};
    u64 vertex_count = u64(0);
    u64 index_count = u64(0);
    memory::MemoryLifetime storage = memory::MemoryLifetime::Cache;
};

struct RenderSnapshotResource {
    ResourceId id = {};
    u64 packet_count = u64(0);
    u64 vertex_count = u64(0);
};

struct FileArtifactResource {
    ResourceId id = {};
    std::filesystem::path path;
    u64 byte_count = u64(0);
};

struct PathBackedResource {
    ResourceId id = {};
    ResourceHandle handle = {};
    std::filesystem::path path;
    ResourceKind expected_kind = ResourceKind::Unknown;
    u64 byte_count = u64(0);
};

struct FontResource {
    ResourceId id = {};
    ResourceHandle handle = {};
    std::filesystem::path path;
    u64 byte_count = u64(0);
    std::vector<byte> bytes;
};

struct GpuUploadResource {
    ResourceId id = {};
    u64 byte_count = u64(0);
    ResourceState state = ResourceState::Pending;
};

using ResourcePayload = std::variant<
    CpuMeshResource,
    RenderSnapshotResource,
    FileArtifactResource,
    PathBackedResource,
    FontResource,
    GpuUploadResource
>;

struct ResourceManagerConfig {
    u64 max_resources = u64(4096);
    u64 max_handles = u64(4096);
};

} // namespace ndde
