#include "engine/SimulationHost.hpp"
#include "engine/logging/LoggerService.hpp"
#include "engine/resources/ResourceManagerService.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

namespace {

using namespace ndde;

[[nodiscard]] std::filesystem::path resource_test_dir() {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "ndde_resource_manager_tests";
    std::filesystem::create_directories(dir);
    return dir;
}

TEST(ResourceManagerService, RegistersConstexprBuiltinHandleAndFindsByKey) {
    ResourceManagerService resources;
    resources.init();

    const ResourceHandle handle = resources.register_builtin(ResourceRegistration{
        .handle = resource_handles::colormap_viridis,
        .key = ResourceKey{"builtin.colormap.viridis"},
        .kind = ResourceKind::ColorMap,
        .origin = ResourceOrigin::ConstexprRegistered,
        .owner = ResourceOwner::Engine,
        .lifetime = ResourceLifetime::Persistent
    });

    EXPECT_EQ(handle, resource_handles::colormap_viridis);
    ASSERT_TRUE(resources.find(ResourceKey{"builtin.colormap.viridis"}).has_value());
    EXPECT_EQ(*resources.find(ResourceKey{"builtin.colormap.viridis"}), handle);
    EXPECT_FALSE(resources.current(handle).has_value());
}

TEST(ResourceManagerService, RejectsDuplicateHandlesKeysAndPaths) {
    ResourceManagerService resources;
    resources.init();
    const std::filesystem::path path = resource_test_dir() / "duplicate_asset.bin";

    ASSERT_NE(resources.register_path(ResourceRegistration{
        .handle = ResourceHandle{u64(0x0001'0000'0000'1000)},
        .key = ResourceKey{"asset.duplicate"},
        .kind = ResourceKind::FileArtifact,
        .owner = ResourceOwner::Engine,
        .lifetime = ResourceLifetime::Persistent
    }, path).value, u64(0));

    EXPECT_EQ(resources.register_handle(ResourceRegistration{
        .handle = ResourceHandle{u64(0x0001'0000'0000'1000)},
        .key = ResourceKey{"asset.other"},
        .kind = ResourceKind::FileArtifact
    }).value, u64(0));

    EXPECT_EQ(resources.register_handle(ResourceRegistration{
        .handle = ResourceHandle{u64(0x0001'0000'0000'1001)},
        .key = ResourceKey{"asset.duplicate"},
        .kind = ResourceKind::FileArtifact
    }).value, u64(0));

    EXPECT_EQ(resources.register_path(ResourceRegistration{
        .handle = ResourceHandle{u64(0x0001'0000'0000'1002)},
        .key = ResourceKey{"asset.path.duplicate"},
        .kind = ResourceKind::FileArtifact
    }, path).value, u64(0));
}

TEST(ResourceManagerService, RuntimeHandlePoolAllocatesNonzeroHandles) {
    ResourceManagerService resources;
    resources.init();

    const ResourceHandle first = resources.register_handle(ResourceRegistration{
        .key = ResourceKey{"runtime.mesh.one"},
        .kind = ResourceKind::CpuMesh,
        .owner = ResourceOwner::Worker,
        .lifetime = ResourceLifetime::Cache
    });
    const ResourceHandle second = resources.register_handle(ResourceRegistration{
        .key = ResourceKey{"runtime.mesh.two"},
        .kind = ResourceKind::CpuMesh,
        .owner = ResourceOwner::Worker,
        .lifetime = ResourceLifetime::Cache
    });

    EXPECT_NE(first.value, u64(0));
    EXPECT_NE(second.value, u64(0));
    EXPECT_NE(first, second);
}

TEST(ResourceManagerService, ReservesPublishesAndQueriesResource) {
    ResourceManagerService resources;
    resources.init();

    const ResourceId id = resources.reserve(ResourceKind::CpuMesh,
                                            ResourceOwner::Worker,
                                            ResourceLifetime::Cache);
    ASSERT_NE(id.value, u64(0));

    EXPECT_TRUE(resources.publish(id, CpuMeshResource{
        .vertex_count = u64(12),
        .index_count = u64(6)
    }));
    EXPECT_FALSE(resources.publish(id, CpuMeshResource{
        .vertex_count = u64(1)
    }));
    EXPECT_TRUE(resources.mark_ready(id));

    const ResourceDescriptor* descriptor = resources.descriptor(id);
    ASSERT_NE(descriptor, nullptr);
    EXPECT_EQ(descriptor->kind, ResourceKind::CpuMesh);
    EXPECT_EQ(descriptor->owner, ResourceOwner::Worker);
    EXPECT_EQ(descriptor->lifetime, ResourceLifetime::Cache);
    EXPECT_EQ(descriptor->state, ResourceState::Ready);
    EXPECT_GT(descriptor->byte_count, u64(0));

    ASSERT_NE(resources.payload(id), nullptr);
    ASSERT_TRUE(std::holds_alternative<CpuMeshResource>(*resources.payload(id)));
    EXPECT_EQ(std::get<CpuMeshResource>(*resources.payload(id)).vertex_count, u64(12));

    EXPECT_EQ(resources.resources_by_owner(ResourceOwner::Worker).size(), 1u);
    EXPECT_EQ(resources.resources_by_kind(ResourceKind::CpuMesh).size(), 1u);
    EXPECT_EQ(resources.resources_by_lifetime(ResourceLifetime::Cache).size(), 1u);
}

TEST(ResourceManagerService, RegistersPathAndLoadsFileAsCurrentResource) {
    ResourceManagerService resources;
    resources.init();

    const std::filesystem::path path = resource_test_dir() / "path_asset.bin";
    {
        std::ofstream out(path, std::ios::binary);
        out << "resource bytes";
    }

    const ResourceHandle handle = resources.register_path(ResourceRegistration{
        .key = ResourceKey{"asset.path"},
        .kind = ResourceKind::FileArtifact,
        .owner = ResourceOwner::Engine,
        .lifetime = ResourceLifetime::Persistent
    }, path);
    ASSERT_NE(handle.value, u64(0));
    ASSERT_TRUE(resources.find_by_path(path).has_value());
    EXPECT_EQ(*resources.find_by_path(path), handle);

    const auto loaded = resources.load(handle);
    ASSERT_TRUE(loaded.has_value());
    ASSERT_TRUE(resources.current(handle).has_value());
    EXPECT_EQ(*resources.current(handle), *loaded);

    const ResourceDescriptor* descriptor = resources.descriptor(handle);
    ASSERT_NE(descriptor, nullptr);
    EXPECT_EQ(descriptor->id, *loaded);
    EXPECT_EQ(descriptor->state, ResourceState::Ready);
    EXPECT_EQ(descriptor->origin, ResourceOrigin::FilePath);
    EXPECT_GT(descriptor->byte_count, u64(0));

    const ResourcePayload* payload = resources.payload(*loaded);
    ASSERT_NE(payload, nullptr);
    ASSERT_TRUE(std::holds_alternative<PathBackedResource>(*payload));
    EXPECT_EQ(std::get<PathBackedResource>(*payload).path, path);
}

TEST(ResourceManagerService, LoadsFontResourceBytesFromPath) {
    ResourceManagerService resources;
    resources.init();

    const std::filesystem::path path = resource_test_dir() / "font_asset.ttf";
    {
        std::ofstream out(path, std::ios::binary);
        out << "font bytes";
    }

    const ResourceHandle handle = resources.register_path(ResourceRegistration{
        .key = ResourceKey{"font.test"},
        .kind = ResourceKind::Font,
        .owner = ResourceOwner::Renderer,
        .lifetime = ResourceLifetime::Persistent
    }, path);
    ASSERT_NE(handle.value, u64(0));

    const auto loaded = resources.load(handle);
    ASSERT_TRUE(loaded.has_value());

    const ResourceDescriptor* descriptor = resources.descriptor(*loaded);
    ASSERT_NE(descriptor, nullptr);
    EXPECT_EQ(descriptor->kind, ResourceKind::Font);
    EXPECT_EQ(descriptor->state, ResourceState::Ready);
    EXPECT_EQ(descriptor->byte_count, u64(10));

    const ResourcePayload* payload = resources.payload(*loaded);
    ASSERT_NE(payload, nullptr);
    ASSERT_TRUE(std::holds_alternative<FontResource>(*payload));
    const FontResource& font = std::get<FontResource>(*payload);
    EXPECT_EQ(font.handle, handle);
    EXPECT_EQ(font.path, path);
    ASSERT_EQ(font.bytes.size(), 10u);
    EXPECT_EQ(static_cast<char>(font.bytes[0]), 'f');
}

TEST(ResourceManagerService, LogsSuccessfulPathLoadsWithResourceReference) {
    LoggerService logger;
    logger.init();
    ResourceManagerService resources;
    resources.init();
    resources.set_logger_service(&logger);

    const std::filesystem::path path = resource_test_dir() / "logged_asset.bin";
    {
        std::ofstream out(path, std::ios::binary);
        out << "logged bytes";
    }

    const ResourceHandle handle = resources.register_path(ResourceRegistration{
        .key = ResourceKey{"asset.logged"},
        .kind = ResourceKind::FileArtifact,
        .owner = ResourceOwner::Engine,
        .lifetime = ResourceLifetime::Persistent
    }, path);
    ASSERT_NE(handle.value, u64(0));

    const auto loaded = resources.load(handle);
    ASSERT_TRUE(loaded.has_value());

    const auto records = logger.snapshot();
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records.front().record.severity, LogSeverity::Info);
    EXPECT_EQ(records.front().record.category, LogCategory::Resource);
    ASSERT_TRUE(records.front().record.resource.has_value());
    EXPECT_EQ(*records.front().record.resource, *loaded);
    EXPECT_NE(records.front().message.find("loaded resource"), std::string::npos);
    EXPECT_NE(records.front().message.find(path.string()), std::string::npos);
}

TEST(ResourceManagerService, LogsMissingPathLoadFailures) {
    LoggerService logger;
    logger.init();
    ResourceManagerService resources;
    resources.init();
    resources.set_logger_service(&logger);

    const std::filesystem::path path = resource_test_dir() / "logged_missing_asset.bin";
    std::filesystem::remove(path);
    const ResourceHandle handle = resources.register_path(ResourceRegistration{
        .key = ResourceKey{"asset.logged.missing"},
        .kind = ResourceKind::FileArtifact
    }, path);
    ASSERT_NE(handle.value, u64(0));

    const auto loaded = resources.load(handle);
    ASSERT_FALSE(loaded.has_value());

    const auto records = logger.snapshot();
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records.front().record.severity, LogSeverity::Error);
    EXPECT_EQ(records.front().record.category, LogCategory::Resource);
    EXPECT_FALSE(records.front().record.resource.has_value());
    EXPECT_NE(records.front().message.find("file not found"), std::string::npos);
    EXPECT_NE(records.front().message.find(path.string()), std::string::npos);
}

TEST(ResourceManagerService, LoadReportsMissingFile) {
    ResourceManagerService resources;
    resources.init();

    const std::filesystem::path path = resource_test_dir() / "missing_asset.bin";
    std::filesystem::remove(path);
    const ResourceHandle handle = resources.register_path(ResourceRegistration{
        .key = ResourceKey{"asset.missing"},
        .kind = ResourceKind::FileArtifact
    }, path);
    ASSERT_NE(handle.value, u64(0));

    const auto loaded = resources.load(handle);
    ASSERT_FALSE(loaded.has_value());
    EXPECT_EQ(loaded.error().code, ResourceLoadErrorCode::FileNotFound);
}

TEST(ResourceManagerService, ReleaseAndClearLifetimeInvalidateCurrentResources) {
    ResourceManagerService resources;
    resources.init();

    const ResourceHandle handle = resources.register_handle(ResourceRegistration{
        .key = ResourceKey{"cache.mesh"},
        .kind = ResourceKind::CpuMesh,
        .owner = ResourceOwner::Worker,
        .lifetime = ResourceLifetime::Cache
    });
    ASSERT_NE(handle.value, u64(0));

    const ResourceId id = resources.reserve(handle);
    ASSERT_NE(id.value, u64(0));
    ASSERT_TRUE(resources.publish(id, CpuMeshResource{.vertex_count = u64(4)}));
    ASSERT_TRUE(resources.mark_ready(id));
    ASSERT_TRUE(resources.current(handle).has_value());

    resources.clear_lifetime(ResourceLifetime::Cache);

    const ResourceDescriptor* descriptor = resources.descriptor(id);
    ASSERT_NE(descriptor, nullptr);
    EXPECT_EQ(descriptor->state, ResourceState::Released);
    EXPECT_FALSE(resources.current(handle).has_value());
    EXPECT_TRUE(resources.resources_by_lifetime(ResourceLifetime::Cache).empty());
}

TEST(EngineServices, OwnsResourceManagerServiceAndPassesItToSimulationHost) {
    EngineServices services;
    SimulationHost host = services.simulation_host();

    const ResourceHandle handle = host.resources().register_builtin(ResourceRegistration{
        .handle = resource_handles::renderer_line_shader,
        .key = ResourceKey{"builtin.shader.line"},
        .kind = ResourceKind::ShaderModule,
        .origin = ResourceOrigin::ConstexprRegistered,
        .owner = ResourceOwner::Renderer,
        .lifetime = ResourceLifetime::Persistent
    });

    EXPECT_EQ(handle, resource_handles::renderer_line_shader);
    ASSERT_TRUE(services.resources().find(ResourceKey{"builtin.shader.line"}).has_value());
    EXPECT_EQ(*services.resources().find(ResourceKey{"builtin.shader.line"}), handle);
}

} // namespace
