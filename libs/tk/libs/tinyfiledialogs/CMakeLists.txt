project(tinyfiledialogs)

add_library(tinyfiledialogs STATIC tinyfiledialogs.h tinyfiledialogs.c)

target_include_directories(tinyfiledialogs PUBLIC ${PROJECT_SOURCE_DIR})