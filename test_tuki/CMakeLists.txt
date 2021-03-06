cmake_minimum_required(VERSION 2.8)

set(PROJ_NAME "test_tuki")
project(${PROJ_NAME})

# try to use C++ 11
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
if(COMPILER_SUPPORTS_CXX11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif(COMPILER_SUPPORTS_CXX0X)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
else()
        message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++0x support. Please use a different C++ compiler.")
endif()

add_executable("test1"
	"test1.cpp"
)

set("exec_targets"
	"test1"
)

foreach(exec_target ${exec_targets})
	target_link_libraries(${exec_target} "tuki_lib")
endforeach(exec_target)

# ----------------------------------------------------
# Copy assets to the build directory
# ----------------------------------------------------

set("asset_dirs"
	"shaders"
	"textures"
	"material_templates"
	"materials"
	"config"
	"mesh"
)

foreach(dir ${asset_dirs})
add_custom_target("copy_${dir}" ALL
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	${PROJECT_SOURCE_DIR}/${dir}
	${CMAKE_BINARY_DIR}/${PROJ_NAME}/${dir}
)
endforeach(dir)

foreach(exec_target ${exec_targets})
	foreach(asset_dir ${asset_dirs})
		add_dependencies("${exec_target}" "copy_${asset_dir}")
	endforeach(asset_dir)
endforeach(exec_target)
