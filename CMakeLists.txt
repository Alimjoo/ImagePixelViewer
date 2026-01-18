cmake_minimum_required(VERSION 3.14)
project(ImagePixelViewer LANGUAGES C CXX)

option(BUILD_WITHOUT_CONSOLE "Build without console window on Windows." OFF)
option(GLEW_USE_STATIC_LIBS "Use static GLEW libraries." OFF)

# Third-party locations (use -D...=path when configuring)
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/imgui-1.92.5" CACHE PATH "Path to Dear ImGui source tree")
set(GLFW_DIR "" CACHE PATH "Path to GLFW install (optional if find_package works)")
set(GLEW_DIR "" CACHE PATH "Path to GLEW install (optional if find_package works)")
set(OpenCV_DIR "" CACHE PATH "Path to OpenCV config directory")

if(GLFW_DIR)
    list(APPEND CMAKE_PREFIX_PATH "${GLFW_DIR}")
endif()
if(GLEW_DIR)
    list(APPEND CMAKE_PREFIX_PATH "${GLEW_DIR}")
endif()
if(OpenCV_DIR)
    list(APPEND CMAKE_PREFIX_PATH "${OpenCV_DIR}")
endif()

if(NOT EXISTS "${IMGUI_DIR}/imgui.h")
    message(FATAL_ERROR "IMGUI_DIR must point to Dear ImGui sources (missing imgui.h).")
endif()

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
target_compile_definitions(imgui PUBLIC
    IMGUI_IMPL_OPENGL_LOADER_GLEW 
)

# -------------------------------------------------
# 2. GLFW (static library, built from source)
# -------------------------------------------------
target_include_directories(imgui PUBLIC
)

# -------------------------------------------------
# 3. Link ImGui with GLFW
# -------------------------------------------------
find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(GLEW REQUIRED)
find_package(OpenCV REQUIRED)

if(TARGET glfw)
    set(GLFW_TARGET glfw)
elseif(TARGET glfw3)
    set(GLFW_TARGET glfw3)
else()
    message(FATAL_ERROR "glfw target not found after find_package(glfw3).")
endif()

target_link_libraries(imgui PUBLIC
    ${GLFW_TARGET}
    OpenGL::GL
)

if(APPLE)
    target_compile_definitions(imgui PUBLIC GL_SILENCE_DEPRECATION)
endif()

# -------------------------------------------------
# 4. Your executable
# -------------------------------------------------
file(GLOB ALL_FILES "src/*.h" "src/*.cpp" "src/*.c" main.cpp main.h)
add_executable(${PROJECT_NAME} ${ALL_FILES} )
target_include_directories(${PROJECT_NAME} PUBLIC 
    ./
    ./src/
    ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(${PROJECT_NAME} PRIVATE 
    imgui
    GLEW::GLEW
    ${OpenCV_LIBS}
)

if(GLEW_USE_STATIC_LIBS)
    target_compile_definitions(imgui PUBLIC GLEW_STATIC)
    target_compile_definitions(${PROJECT_NAME} PRIVATE GLEW_STATIC)
endif()


# -------------------------------------------------
# 5. C++ standard
# -------------------------------------------------
set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Build without console window
if(BUILD_WITHOUT_CONSOLE AND WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE BUILD_WITHOUT_CONSOLE)
endif()


# copy assets to build folder
add_custom_command(
  TARGET ${PROJECT_NAME} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
  COMMENT "Copying assets directory to output folder"
  VERBATIM
)
