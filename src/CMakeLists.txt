# src/CMakeLists.txt

# ── ndde_numeric ─────────────────────────────────────────────────────────────
# Zero external dependencies. Safe to link from Python bindings.
# All headers are in src/numeric/*.hpp (header-only; the .cpp stub exists to
# give CMake a translation unit and to hold future non-inline implementations).
add_library(ndde_numeric INTERFACE)
target_include_directories(ndde_numeric INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ndde_numeric INTERFACE glm::glm)

# Optional: flip to std:: / GLM oracle for validation.
# cmake -DNDDE_USE_BUILTIN_MATH=ON ..
option(NDDE_USE_BUILTIN_MATH "Use std/GLM oracle backend instead of ndde::numeric" OFF)
if(NDDE_USE_BUILTIN_MATH)
    target_compile_definitions(ndde_numeric INTERFACE NDDE_USE_BUILTIN_MATH=1)
endif()

option(NDDE_USE_TAYLOR_SIN "Route ndde::numeric sine through the Taylor approximation" OFF)
if(NDDE_USE_TAYLOR_SIN)
    target_compile_definitions(ndde_numeric INTERFACE NDDE_USE_TAYLOR_SIN=1)
endif()

# ── ndde_math ─────────────────────────────────────────────────────────────────
add_library(ndde_math STATIC
    math/Conics.cpp
    math/Surfaces.cpp
    math/Axes.cpp
    math/ExtremumTable.cpp
    math/integration/Integration1D.cpp
    math/integration/Integration2D.cpp
)
target_include_directories(ndde_math PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ndde_math PUBLIC ndde_numeric glm::glm)

# ── ndde_units ────────────────────────────────────────────────────────────────
# Thin project vocabulary over mp-units. Keep this as the only common place
# that application code needs to depend on the external units library.
add_library(ndde_units INTERFACE)
target_include_directories(ndde_units INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ndde_units INTERFACE mp-units::mp-units ndde_math)

# ── ndde_memory ───────────────────────────────────────────────────────────────
add_library(ndde_memory STATIC
    memory/BufferManager.cpp
    memory/MemoryService.cpp
)
target_include_directories(ndde_memory PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ndde_memory PUBLIC ndde_math volk)

# ── ndde_platform ─────────────────────────────────────────────────────────────
add_library(ndde_platform STATIC
    platform/GlfwContext.cpp
    platform/VulkanContext.cpp
)
target_include_directories(ndde_platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ndde_platform PUBLIC ndde_math volk vk-bootstrap::vk-bootstrap glfw)

# ── ndde_renderer ─────────────────────────────────────────────────────────────
add_library(ndde_renderer STATIC
    renderer/Swapchain.cpp
    renderer/Pipeline.cpp
    renderer/ImGuiLayer.cpp
    renderer/Renderer.cpp
    renderer/SecondWindow.cpp
)
target_include_directories(ndde_renderer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ndde_renderer PUBLIC
    ndde_platform
    ndde_memory
    ndde::imgui
    Vulkan::Vulkan
)

# ── ndde_telemetry ────────────────────────────────────────────────────────────
# No Vulkan dependency -- only uses ndde_math (for scalar types) and std I/O.
add_library(ndde_telemetry STATIC
    telemetry/TelemetryService.cpp
    telemetry/TelemetryWriter.cpp
)
target_include_directories(ndde_telemetry PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ndde_telemetry PUBLIC ndde_math)

# ── ndde_simulation ───────────────────────────────────────────────────────────
# Pure simulation logic. No ImGui or Vulkan. App/runtime code adapts concrete
# particle objects into compact simulation-facing views before evaluating rules.
add_library(ndde_simulation STATIC
    simulation/events/EventBus.cpp
    simulation/events/EventLog.cpp
    simulation/events/AlertRule.cpp
)
target_include_directories(ndde_simulation PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ndde_simulation PUBLIC ndde_math)

# ── ndde_engine ───────────────────────────────────────────────────────────────
add_library(ndde_engine STATIC
    engine/capture/CaptureService.cpp
    engine/diagnostics/DiagnosticsService.cpp
    engine/events/EventBusService.cpp
    engine/logging/LoggerService.cpp
    engine/metadata/SimMetadataService.cpp
    engine/metricservice/MetricsService.cpp
    engine/resources/ResourceManagerService.cpp
    engine/text/TextOverlayService.cpp
    engine/threading/ThreadManagementService.cpp
    engine/AppConfig.cpp
    engine/Engine.cpp
    engine/SimulationRuntime.cpp
)
target_include_directories(ndde_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ndde_engine PUBLIC
    ndde_renderer
    ndde_memory
    ndde_platform
    ndde_math
    ndde_telemetry
    ndde_simulation
    nlohmann_json::nlohmann_json
    ndde::freetype
)

# ── nurbs_dde executable ──────────────────────────────────────────────────────
# Old simulations have been moved to src/old/ and are not compiled.
# Only SimulationWavePredatorPrey is active.
add_executable(nurbs_dde
    app/main.cpp
    app/AnimatedCurve.cpp
    app/IntegrationLabAnalyticsPackets.cpp
    app/IntegrationLabRenderPackets.cpp
    app/IntegrationWorkbenchState.cpp
    app/SimulationIntegrationDerivativeLab.cpp
    app/SimulationLabPicker.cpp
    app/ParticleSystem.cpp
    app/SceneFactories.cpp
    app/SimulationTaylorExpansionLab.cpp
    app/SimulationWavePredatorPrey.cpp
    app/SurfaceMeshCache.cpp
    simulation/fields/IField.cpp
    simulation/scenario/ScenarioBuilder.cpp
)
target_include_directories(nurbs_dde PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(nurbs_dde PRIVATE ndde_engine ndde::imgui)

# ── Compiler flags ────────────────────────────────────────────────────────────
set(NDDE_TARGETS ndde_math ndde_memory ndde_platform ndde_renderer ndde_telemetry ndde_simulation ndde_engine nurbs_dde)

foreach(tgt IN LISTS NDDE_TARGETS)
    if(MSVC)
        target_compile_options(${tgt} PRIVATE
            /W4 /WX- /wd4100 /permissive- /utf-8
            $<$<CONFIG:Debug>:/Od /Zi /RTC1>
            $<$<CONFIG:Tidy>:/Od /Zi /RTC1>
            $<$<CONFIG:Release>:/O2 /DNDEBUG>
        )
    else()
        target_compile_options(${tgt} PRIVATE
            -Wall -Wextra -Wpedantic -Wno-unused-parameter
            $<$<CONFIG:Debug>:-g -O0>
            $<$<CONFIG:Tidy>:-g -O0>
            $<$<CONFIG:Release>:-O3 -march=native>
        )
    endif()
endforeach()

if(NDDE_CLANG_TIDY_ENABLED)
    set_property(TARGET ${NDDE_TARGETS} PROPERTY CXX_CLANG_TIDY "${NDDE_CLANG_TIDY_COMMAND}")
endif()

# ── Shaders ───────────────────────────────────────────────────────────────────
set(SHADER_SOURCE_DIR "${CMAKE_SOURCE_DIR}/shaders")
compile_shaders(
    TARGET     nurbs_dde
    OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders"
    SHADERS
        "${SHADER_SOURCE_DIR}/curve.vert"
        "${SHADER_SOURCE_DIR}/curve.frag"
)

set(ASSETS_OUTPUT_DIR "${CMAKE_BINARY_DIR}/assets")

# ── Assets copy ───────────────────────────────────────────────────────────────
add_custom_target(nurbs_dde_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets" "${ASSETS_OUTPUT_DIR}"
    COMMENT "Copying assets to build directory"
)
add_dependencies(nurbs_dde nurbs_dde_assets)

add_custom_command(TARGET nurbs_dde POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${ASSETS_OUTPUT_DIR}" "$<TARGET_FILE_DIR:nurbs_dde>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_BINARY_DIR}/shaders" "$<TARGET_FILE_DIR:nurbs_dde>/shaders"
    COMMENT "Copying runtime assets and shaders beside executable"
)

# ── engine_config.json copy ───────────────────────────────────────────────────
add_custom_target(nurbs_dde_config ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/engine_config.json"
        "${CMAKE_BINARY_DIR}/engine_config.json"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/engine_config.json"
        "$<TARGET_FILE_DIR:nurbs_dde>/engine_config.json"
    COMMENT "Copying engine_config.json to build directory"
)
add_dependencies(nurbs_dde nurbs_dde_config)
