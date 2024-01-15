project(tk)

set (CMAKE_CXX_STANDARD 20)

find_package(Vulkan REQUIRED COMPONENTS shaderc_combined)

add_subdirectory(libs/glfw)
add_subdirectory(libs/glm)
add_subdirectory(libs/stb)
add_subdirectory(libs/cgltf)
add_subdirectory(libs/wyhash)
add_subdirectory(libs/imgui)

set(SRCS_TOP
	src/vma.h
	src/delegate.hpp
	src/utils.hpp src/utils.cpp
	src/shader_compiler.hpp src/shader_compiler.cpp
	src/tvk.hpp src/tvk.cpp
	src/tg.hpp src/tg.cpp
	src/pbr.hpp src/pbr.cpp
	src/tk.hpp src/tk.cpp
)
set(SRCS

)
add_library(tk ${SRCS_TOP} ${SRCS})
source_group("" FILES ${SRCS_TOP})
source_group("tk" FILES ${SRCS})
target_include_directories(tk PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(tk PUBLIC Vulkan::Vulkan Vulkan::shaderc_combined glfw glm stb cgltf wyhash imgui)