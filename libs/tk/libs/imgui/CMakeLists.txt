project(imgui)

add_library(imgui STATIC
    imconfig.h
    imgui.cpp
    imgui.h
    imgui_demo.cpp
    imgui_draw.cpp
    imgui_impl_glfw.cpp
    imgui_impl_glfw.h
    imgui_impl_vulkan.cpp
    imgui_impl_vulkan.h
    imgui_internal.h
    imgui_tables.cpp
    imgui_widgets.cpp
    imstb_rectpack.h
    imstb_textedit.h
    imstb_truetype.h
)

target_include_directories(imgui PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(imgui
	Vulkan::Vulkan
	glfw
    glm
)
