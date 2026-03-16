include(FetchContent)

set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.8
)

FetchContent_MakeAvailable(glfw imgui)

set(IMGUI_DIR ${imgui_SOURCE_DIR})

add_library(imgui_static STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui_static
    PUBLIC
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
)

target_link_libraries(imgui_static PUBLIC glfw)
target_compile_features(imgui_static PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(imgui_static PRIVATE /W4 /permissive-)
else()
    target_compile_options(imgui_static PRIVATE -Wall -Wextra -Wpedantic)
endif()

find_package(OpenGL REQUIRED)


add_executable(aipackager-gui
    src/main.cpp
)

# WIN32 yapılandırması: terminal açılmasın
if(WIN32)
    # Uygulamayı GUI olarak işaretle (Terminali kapatır)
    set_target_properties(aipackager-gui PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

target_link_libraries(aipackager-gui
    PRIVATE
        aipackager_core
        imgui_static
        glfw
        OpenGL::GL
)

target_compile_features(aipackager-gui PRIVATE cxx_std_20)

target_include_directories(aipackager-gui
    PRIVATE
        ${CMAKE_SOURCE_DIR}/core/include
)

if(MSVC)
    target_compile_options(aipackager-gui PRIVATE /W4 /permissive-)
else()
    target_compile_options(aipackager-gui PRIVATE -Wall -Wextra -Wpedantic)
endif()
