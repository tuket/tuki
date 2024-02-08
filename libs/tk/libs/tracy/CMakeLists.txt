project(tracy)

set(SRCS
	TracyEnable.h
	TracyClient.cpp
	Tracy.hpp
	TracyC.h
	TracyOpenGL.hpp
	TracyVulkan.hpp
)

add_library(tracy ${SRCS})
target_include_directories(tracy PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
source_group("" FILES ${SRCS})