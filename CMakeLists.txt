cmake_minimum_required(VERSION 3.21 FATAL_ERROR)
project(tuki)

set (CMAKE_CXX_STANDARD 20)

add_subdirectory(libs/tk)

find_program(GLSLC glslc REQUIRED)

set(COMMON_LIBS
	tk
)

# -- SHADERS --
set(glsl_files
    #vertColor.vert vertColor.frag
    #textured.vert textured.frag
)
foreach(fileNoExt ${glsl_files})
    set(glsl_file ${CMAKE_SOURCE_DIR}/shaders/${fileNoExt}.glsl)
    set(spirv_file ${CMAKE_SOURCE_DIR}/shaders/${fileNoExt}.spirv)
    #message(${spirv_file})
    add_custom_command(
        DEPENDS ${glsl_file}
        OUTPUT ${spirv_file}
        COMMAND ${GLSLC} ${glsl_file} -o ${spirv_file}
    )
    list(APPEND spirv_files ${spirv_file})
endforeach()
add_custom_target(shaders_target DEPENDS ${spirv_files})

# -- SAMPLES --

function(addTukiExecutable sampleName sources)
    add_executable(${sampleName} ${sources})
    add_dependencies(${sampleName} shaders_target)
    source_group("" FILES ${sources})
    target_link_libraries(${sampleName} ${COMMON_LIBS})
    set_target_properties(
        ${sampleName} PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endfunction()

addTukiExecutable(tuki_editor src/editor.cpp)

set_property(DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT tuki_editor)