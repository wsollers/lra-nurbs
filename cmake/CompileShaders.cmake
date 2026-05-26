# cmake/CompileShaders.cmake
# Compiles GLSL shaders to SPIR-V.
# Prefers glslc; falls back to glslangValidator with -V flag.

find_program(GLSLC_EXECUTABLE glslc)
find_program(GLSLANG_EXECUTABLE glslangValidator)

function(compile_shaders)
    cmake_parse_arguments(CS "" "TARGET;OUTPUT_DIR" "SHADERS" ${ARGN})

    if(NOT CS_TARGET)
        message(FATAL_ERROR "compile_shaders: TARGET is required")
    endif()

    if(NOT CS_OUTPUT_DIR)
        set(CS_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")
    endif()

    file(MAKE_DIRECTORY "${CS_OUTPUT_DIR}")

    # Pick compiler once, here, inside the function where the cache vars are visible
    if(GLSLC_EXECUTABLE)
        set(SHADER_COMPILER "${GLSLC_EXECUTABLE}")
        set(SHADER_COMPILER_FLAGS "")          # glslc: glslc in.vert -o out.spv
        set(SHADER_COMPILER_NAME "glslc")
    elseif(GLSLANG_EXECUTABLE)
        set(SHADER_COMPILER "${GLSLANG_EXECUTABLE}")
        set(SHADER_COMPILER_FLAGS "-V")        # glslangValidator: needs -V for SPIR-V output
        set(SHADER_COMPILER_NAME "glslangValidator")
    else()
        message(WARNING "No GLSL compiler (glslc or glslangValidator) found. Shaders will not compile.")
        return()
    endif()

    message(STATUS "Shader compiler: ${SHADER_COMPILER_NAME} (${SHADER_COMPILER})")

    set(SPIRV_FILES "")
    foreach(SHADER ${CS_SHADERS})
        get_filename_component(SHADER_NAME "${SHADER}" NAME)
        set(SPIRV "${CS_OUTPUT_DIR}/${SHADER_NAME}.spv")

        add_custom_command(
            OUTPUT  "${SPIRV}"
            COMMAND "${SHADER_COMPILER}" ${SHADER_COMPILER_FLAGS} "${SHADER}" -o "${SPIRV}"
            DEPENDS "${SHADER}"
            COMMENT "Compiling shader (${SHADER_COMPILER_NAME}): ${SHADER_NAME}"
            VERBATIM
        )

        list(APPEND SPIRV_FILES "${SPIRV}")
    endforeach()

    if(SPIRV_FILES)
        add_custom_target(${CS_TARGET}_shaders ALL DEPENDS ${SPIRV_FILES})
        add_dependencies(${CS_TARGET} ${CS_TARGET}_shaders)
    endif()

endfunction()
