cmake_minimum_required(VERSION 3.27)
project(nurbs_dde VERSION 0.2.0 LANGUAGES CXX)
enable_testing()

# ── C++23 ─────────────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Compiler guard ────────────────────────────────────────────────────────────
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(WARNING "This project targets Clang. Current: ${CMAKE_CXX_COMPILER_ID}")
endif()

# ── Build type default ────────────────────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel Tidy)
set(CMAKE_MAP_IMPORTED_CONFIG_TIDY Debug)

set(NDDE_TIDY_BUILD_TYPE_REQUESTED OFF)
if(CMAKE_BUILD_TYPE STREQUAL "Tidy")
    set(NDDE_TIDY_BUILD_TYPE_REQUESTED ON)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

if(CMAKE_CONFIGURATION_TYPES)
    list(APPEND CMAKE_CONFIGURATION_TYPES Tidy)
    list(REMOVE_DUPLICATES CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES}" CACHE STRING "Available build configurations" FORCE)
endif()

# ── Options ───────────────────────────────────────────────────────────────────
option(ENABLE_SANITIZERS "Build with ASan + UBSan (Debug only)" ON)
option(NDDE_ENABLE_CLANG_TIDY "Run clang-tidy for production targets" OFF)

set(NDDE_CLANG_TIDY_ENABLED OFF)
if(NDDE_ENABLE_CLANG_TIDY OR NDDE_TIDY_BUILD_TYPE_REQUESTED)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy clang-tidy.exe)

    if(NOT CLANG_TIDY_EXE)
        message(FATAL_ERROR
            "clang-tidy was requested but clang-tidy.exe was not found. "
            "Install Visual Studio 'C++ Clang tools for Windows' or LLVM, "
            "then ensure clang-tidy.exe is on PATH.")
    endif()

    set(NDDE_CLANG_TIDY_ENABLED ON)
    set(NDDE_CLANG_TIDY_COMMAND
        "${CLANG_TIDY_EXE}"
        "--extra-arg=/EHsc"
        "-checks=-*,cppcoreguidelines-special-member-functions,cppcoreguidelines-slicing"
        "-warnings-as-errors=cppcoreguidelines-special-member-functions")
    message(STATUS "clang-tidy enabled: ${CLANG_TIDY_EXE}")
endif()

# ── Module path ───────────────────────────────────────────────────────────────
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ── Dependencies ──────────────────────────────────────────────────────────────
include(FetchContent)

# vk-bootstrap
FetchContent_Declare(vk_bootstrap
    GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git
    GIT_TAG        v1.3.295)
FetchContent_MakeAvailable(vk_bootstrap)

# Volk
FetchContent_Declare(volk
    GIT_REPOSITORY https://github.com/zeux/volk.git
    GIT_TAG        1.3.295)
FetchContent_MakeAvailable(volk)

# GLFW
FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# GLM (header-only)
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1)
FetchContent_MakeAvailable(glm)

# nlohmann/json (header-only, for engine_config.json loading)
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)

# mp-units (header-only). Use the library's src CMake project; the top-level
# project is intended for mp-units development and pulls extra examples/tests.
set(MP_UNITS_BUILD_INSTALL OFF CACHE BOOL "" FORCE)
set(MP_UNITS_BUILD_CXX_MODULES OFF CACHE BOOL "" FORCE)
set(MP_UNITS_API_CONTRACTS NONE CACHE STRING "" FORCE)
FetchContent_Declare(mp_units
    GIT_REPOSITORY https://github.com/mpusz/mp-units.git
    # mp-units 2.5.0, pinned by commit for reproducible Windows builds.
    GIT_TAG        6e06eddf205deaf6c2f2f63500c8c70ec220a99f
    SOURCE_SUBDIR  src)
FetchContent_MakeAvailable(mp_units)

# Google Test
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Dear ImGui (docking branch, no CMakeLists — we define the target manually)
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        docking
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(imgui)

# FreeType
# Used by the renderer-side text service to rasterize TTF/OTF font resources
# that are loaded and cached through ResourceManagerService.
set(FT_DISABLE_BROTLI  ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2   ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG     ON CACHE BOOL "" FORCE)
set(FT_DISABLE_ZLIB    ON CACHE BOOL "" FORCE)
FetchContent_Declare(freetype
    GIT_REPOSITORY https://github.com/freetype/freetype.git
    GIT_TAG        VER-2-13-3
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(freetype)
add_library(ndde::freetype ALIAS freetype)

find_package(Vulkan REQUIRED)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

add_library(ndde_imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)
add_library(ndde::imgui ALIAS ndde_imgui)
target_include_directories(ndde_imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_compile_definitions(ndde_imgui PUBLIC IMGUI_IMPL_VULKAN_USE_VOLK)
target_link_libraries(ndde_imgui PUBLIC Vulkan::Vulkan volk glfw)

# ── Global Vulkan define ──────────────────────────────────────────────────────
# volk loads all Vulkan function pointers at runtime.
# VK_NO_PROTOTYPES tells every Vulkan header not to declare the standard
# extern symbols, preventing duplicate-symbol linker errors.
add_compile_definitions(VK_NO_PROTOTYPES)

# ── Shader compilation ────────────────────────────────────────────────────────
include(CompileShaders)

# ── Subdirectories ────────────────────────────────────────────────────────────
add_subdirectory(src)
add_subdirectory(tests)
