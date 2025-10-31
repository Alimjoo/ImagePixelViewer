cmake_minimum_required(VERSION 3.14)
project(ImagePixelViewer LANGUAGES CXX)

# Variables
set(BUILD_WITHOUT_CONSOLE false)

# Third-party lib Paths
set(IMGUI_DIR "C:/Users/PI/Documents/imgui-1.92.4")
set(GLFW_DIR "C:/Users/PI/Documents/glfw-3.4.bin.WIN64")
set(OpenCV_DIR "$ENV{USERPROFILE}/Documents/opencv/build/")
set(GLEW_DIR "C:/Users/PI/Documents/glew-2.1.0")

# -------------------------------------------------
# 1. Dear ImGui (static library)
# -------------------------------------------------
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    # ---- backends (GLFW + OpenGL3) ----
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

# Tell ImGui which OpenGL loader to use.
# ImGui ships a tiny glad copy in examples/; we use that.
target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_LOADER_GLAD)

# -------------------------------------------------
# 2. GLFW (static library, built from source)
# -------------------------------------------------
target_include_directories(imgui PUBLIC
    ${GLFW_DIR}/include
)

# -------------------------------------------------
# 3. Link ImGui with GLFW
# -------------------------------------------------
target_link_libraries(imgui PUBLIC 
    ${GLFW_DIR}/lib-vc2022/glfw3_mt.lib
    opengl32
)

#OpenCV
find_package(OpenCV REQUIRED)

# -------------------------------------------------
# 4. Your executable
# -------------------------------------------------
file(GLOB ALL_FILES "src/*.h" "src/*.cpp" main.cpp main.h)
add_executable(${PROJECT_NAME} ${ALL_FILES} )
target_include_directories(${PROJECT_NAME} PUBLIC 
    ./
    ${GLEW_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(${PROJECT_NAME} PRIVATE 
    imgui
    ${GLEW_DIR}/lib/Release/x64/glew32s.lib
    ${OpenCV_LIBS}
)

# -------------------------------------------------
# 5. C++ standard
# -------------------------------------------------
set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Build without console window
if(BUILD_WITHOUT_CONSOLE AND WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
    target_compile_definitions(${PROJECT_NAME} PRIVATE BUILD_WITHOUT_CONSOLE)
endif()


