# tests/CMakeLists.txt
enable_testing()

add_executable(ndde_tests
    test_all_simulations.cpp
    test_alternate_view_builders.cpp
    test_app_config.cpp
    test_conics.cpp
    test_capture_service.cpp
    test_axes.cpp
    test_numeric.cpp
    test_particle_system.cpp
    test_surface_mesh_cache.cpp
    test_diagnostics_service.cpp
    test_event_bus_service.cpp
    test_engine_services.cpp
    test_gravitational_systems.cpp
    test_history_buffer.cpp
    test_integration2d_core.cpp
    test_integration_lab_core.cpp
    test_integration_lab_analytics_packets.cpp
    test_integration_lab_render_packets.cpp
    test_integration_workbench_state.cpp
    test_logger_service.cpp
    test_metrics_service.cpp
    test_milstein_integrator.cpp
    test_mp_units_integration.cpp
    test_resource_manager_service.cpp
    test_sim_metadata_service.cpp
    test_simulation_context.cpp
    test_simulation_runtime.cpp
    test_surface_metadata.cpp
    test_text_overlay_service.cpp
    test_thread_management_service.cpp
    # Differential / DDE tests reference old simulations — archived, excluded.
    # Re-enable when those sims are ported to the new simulation/ layer.
    # test_differential_system.cpp
    # test_simulation_surface_gaussian.cpp
    ${CMAKE_SOURCE_DIR}/src/app/AnimatedCurve.cpp
    ${CMAKE_SOURCE_DIR}/src/app/IntegrationLabAnalyticsPackets.cpp
    ${CMAKE_SOURCE_DIR}/src/app/IntegrationLabRenderPackets.cpp
    ${CMAKE_SOURCE_DIR}/src/app/IntegrationWorkbenchState.cpp
    ${CMAKE_SOURCE_DIR}/src/app/SimulationIntegrationDerivativeLab.cpp
    ${CMAKE_SOURCE_DIR}/src/app/SimulationLabPicker.cpp
    ${CMAKE_SOURCE_DIR}/src/app/ParticleSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/app/SceneFactories.cpp
    ${CMAKE_SOURCE_DIR}/src/app/SimulationTaylorExpansionLab.cpp
    ${CMAKE_SOURCE_DIR}/src/app/SimulationWavePredatorPrey.cpp
    ${CMAKE_SOURCE_DIR}/src/app/SurfaceMeshCache.cpp
    ${CMAKE_SOURCE_DIR}/src/simulation/fields/IField.cpp
    ${CMAKE_SOURCE_DIR}/src/simulation/scenario/ScenarioBuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/AppConfig.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/SimulationRuntime.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/capture/CaptureService.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/diagnostics/DiagnosticsService.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/events/EventBusService.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/logging/LoggerService.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/metadata/SimMetadataService.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/metricservice/MetricsService.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/resources/ResourceManagerService.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/text/TextOverlayService.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/threading/ThreadManagementService.cpp
)

target_include_directories(ndde_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ndde_tests PRIVATE
    ndde_memory
    ndde_math
    ndde_numeric
    ndde_units
    ndde_telemetry
    ndde_simulation
    volk
    ndde::imgui
    ndde::freetype
    nlohmann_json::nlohmann_json
    GTest::gtest_main
    glm::glm
)

if(MSVC)
    target_compile_options(ndde_tests PRIVATE
        /W4 /WX- /wd4100 /permissive- /utf-8
        $<$<CONFIG:Debug>:/Od /Zi /RTC1>
        $<$<CONFIG:Tidy>:/Od /Zi /RTC1>
        $<$<CONFIG:Release>:/O2>
    )
else()
    target_compile_options(ndde_tests PRIVATE
        -Wall -Wextra -Wpedantic -Wno-unused-parameter
        $<$<CONFIG:Debug>:-g -O0>
        $<$<CONFIG:Tidy>:-g -O0>
        $<$<CONFIG:Release>:-O3>
        $<$<AND:$<CONFIG:Debug>,$<BOOL:${ENABLE_SANITIZERS}>>:-fsanitize=address,undefined>
    )
    target_link_options(ndde_tests PRIVATE
        $<$<AND:$<CONFIG:Debug>,$<BOOL:${ENABLE_SANITIZERS}>>:-fsanitize=address,undefined>
    )
endif()

include(GoogleTest)
gtest_discover_tests(ndde_tests)

add_test(
    NAME allocation_policy
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/check_allocation_policy.py
)

add_test(
    NAME hot_path_container_policy
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/check_hot_path_container_policy.py
)
