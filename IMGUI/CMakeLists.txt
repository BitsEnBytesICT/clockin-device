cmake_minimum_required(VERSION 3.16)
project(imgui_stm32 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# OFF is the production ARM/OpenSTLinux build. ON is the WSLg simulator.
option(DESKTOP_SIM "Build the WSL desktop hardware simulator" OFF)

set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/imgui")
set(GLFW_DIR "${CMAKE_CURRENT_SOURCE_DIR}/glfw")

set(IMGUI_SOURCES
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
)

# Build the bundled GLFW source for the selected architecture. This avoids
# committing an SDK-specific prebuilt ARM archive to the repository.
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_WAYLAND ON CACHE BOOL "" FORCE)
if(DESKTOP_SIM)
    set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
else()
    set(GLFW_BUILD_X11 OFF CACHE BOOL "" FORCE)
endif()
add_subdirectory("${GLFW_DIR}" "${CMAKE_BINARY_DIR}/glfw-native" EXCLUDE_FROM_ALL)

add_executable(imgui_app
    main.cpp
    ${IMGUI_SOURCES}
)

target_include_directories(imgui_app PRIVATE
    "${IMGUI_DIR}"
    "${IMGUI_DIR}/backends"
    "${GLFW_DIR}/include"
)

target_compile_definitions(imgui_app PRIVATE
    GLFW_INCLUDE_ES2
    GLFW_EXPOSE_NATIVE_WAYLAND
    IMGUI_IMPL_OPENGL_ES2
)

if(DESKTOP_SIM)
    # run-desktop-sim.sh extracts matching curl headers locally without sudo.
    set(LOCAL_CURL_ROOT "${CMAKE_BINARY_DIR}/curl-dev/root")
    find_path(HOST_CURL_INCLUDE_DIR
        NAMES curl/curl.h
        PATHS
            "${LOCAL_CURL_ROOT}/usr/include/x86_64-linux-gnu"
            "${LOCAL_CURL_ROOT}/usr/include"
        NO_DEFAULT_PATH
    )
    find_file(HOST_CURL_LIBRARY
        NAMES libcurl.so.4
        PATHS /lib/x86_64-linux-gnu /usr/lib/x86_64-linux-gnu
        NO_DEFAULT_PATH
    )
    if(NOT HOST_CURL_INCLUDE_DIR OR NOT HOST_CURL_LIBRARY)
        message(FATAL_ERROR "Desktop curl headers are missing. Run ./run-desktop-sim.sh.")
    endif()

    target_compile_definitions(imgui_app PRIVATE DESKTOP_SIM=1)
    target_include_directories(imgui_app PRIVATE "${HOST_CURL_INCLUDE_DIR}")
    target_link_libraries(imgui_app PRIVATE
        glfw
        "${HOST_CURL_LIBRARY}"
        GLESv2
        EGL
        dl
        pthread
        m
    )
else()
    target_compile_definitions(imgui_app PRIVATE STM32_DISPLAY_ROTATE_180=1)
    target_link_libraries(imgui_app PRIVATE
        glfw
        GLESv2
        EGL
        dl
        pthread
        m
        curl
    )
endif()
