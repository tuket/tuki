project(wyhash LANGUAGES C)

set(SRCS wyhash.h)
add_library(wyhash INTERFACE ${SRCS})

target_include_directories(wyhash INTERFACE ${PROJECT_SOURCE_DIR})
source_group("" FILES ${SRCS})