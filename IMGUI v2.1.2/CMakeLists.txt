cmake_minimum_required(VERSION 3.16)
project(rfid-attendance VERSION 2.1.2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ====================================================
# Dependencies via pkg-config (from SDK sysroot)
# ====================================================
find_package(PkgConfig REQUIRED)

pkg_check_modules(GLES2   REQUIRED IMPORTED_TARGET glesv2)
pkg_check_modules(EGL     REQUIRED IMPORTED_TARGET egl)
pkg_check_modules(CURL    REQUIRED IMPORTED_TARGET libcurl)
pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)
pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)
pkg_check_modules(WAYLAND_EGL    REQUIRED IMPORTED_TARGET wayland-egl)
pkg_check_modules(WAYLAND_CURSOR REQUIRED IMPORTED_TARGET wayland-cursor)
pkg_check_modules(XKBCOMMON      REQUIRED IMPORTED_TARGET xkbcommon)
pkg_check_modules(LIBDRM         REQUIRED IMPORTED_TARGET libdrm)

# ====================================================
# GLFW - cross-compile from source (not in SDK sysroot)
# ====================================================
include(FetchContent)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "")
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "")
set(GLFW_BUILD_WAYLAND ON CACHE BOOL "")
set(GLFW_BUILD_X11 OFF CACHE BOOL "")
FetchContent_MakeAvailable(glfw)

# ====================================================
# ImGui - cross-compile from source
# ====================================================
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.9
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

# ====================================================
# Application
# ====================================================
add_executable(${PROJECT_NAME}
    main.cpp
    ${IMGUI_SOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    IMGUI_IMPL_OPENGL_ES3
    GLFW_INCLUDE_ES3
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    glfw
    PkgConfig::GLES2
    PkgConfig::EGL
    PkgConfig::CURL
    PkgConfig::FREETYPE
    PkgConfig::WAYLAND_CLIENT
    PkgConfig::WAYLAND_EGL
    PkgConfig::WAYLAND_CURSOR
    PkgConfig::XKBCOMMON
    PkgConfig::LIBDRM
    pthread
    dl
    m
)

# Strip debug symbols for smaller binary
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_STRIP} $<TARGET_FILE:${PROJECT_NAME}>
    COMMENT "Stripping ${PROJECT_NAME}"
)

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
