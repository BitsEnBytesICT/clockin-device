cmake_minimum_required(VERSION 3.16)
project(clockin_device VERSION 2.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CLOCKIN_BUILD_APP "Build the STM32MP157 A7 kiosk application" ON)
option(CLOCKIN_BUILD_DIAGNOSTIC "Build the board hardware diagnostic" ON)
option(CLOCKIN_BUILD_TESTS "Build host-only unit tests" OFF)
option(CLOCKIN_ENABLE_SANITIZERS "Enable ASan and UBSan for unit tests" OFF)

set(CLOCKIN_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui")
set(GLFW_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw")

function(clockin_enable_warnings target)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
endfunction()

set(CLOCKIN_DOMAIN_SOURCES
    src/app/Navigation.cpp
    src/app/WorkflowCoordinator.cpp
    src/app/WorkflowSession.cpp
    src/domain/ApiUrl.cpp
    src/domain/AttendanceDate.cpp
    src/domain/CardPresenceTracker.cpp
    src/domain/RfidUid.cpp
    src/services/api/TinyJson.cpp
    src/services/hardware/buzzer/BuzzerSequencer.cpp
    src/services/hardware/rfid/Mfrc522.cpp
    src/services/network/NetworkParsing.cpp
    src/services/network/NetworkTypes.cpp
    src/ui/SignaturePad.cpp
)

if(CLOCKIN_BUILD_APP OR CLOCKIN_BUILD_DIAGNOSTIC)
    find_package(Threads REQUIRED)
    find_library(GPIOD_LIBRARY NAMES gpiod REQUIRED)
endif()

if(CLOCKIN_BUILD_APP)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_WAYLAND ON CACHE BOOL "" FORCE)
    set(GLFW_BUILD_X11 OFF CACHE BOOL "" FORCE)
    add_subdirectory("${GLFW_DIR}" "${CMAKE_BINARY_DIR}/glfw" EXCLUDE_FROM_ALL)

    find_path(CURL_INCLUDE_DIR NAMES curl/curl.h REQUIRED)
    find_library(CURL_LIBRARY NAMES curl REQUIRED)
    find_library(GLES2_LIBRARY NAMES GLESv2 REQUIRED)
    find_library(EGL_LIBRARY NAMES EGL REQUIRED)

    add_executable(clockin-device
        src/main.cpp
        src/app/AppConfiguration.cpp
        src/app/Application.cpp
        src/app/ApplicationController.cpp
        src/app/ApplicationControllerAdmin.cpp
        src/app/InputRouter.cpp
        ${CLOCKIN_DOMAIN_SOURCES}
        src/pages/AdminPage.cpp
        src/pages/AdminPinPage.cpp
        src/pages/admin/AdminApiPanel.cpp
        src/pages/admin/AdminNetworkPanel.cpp
        src/pages/admin/AdminNetworkPanelDrawing.cpp
        src/pages/admin/AdminPinPanel.cpp
        src/pages/admin/AdminRfidPanel.cpp
        src/pages/admin/OnScreenKeyboard.cpp
        src/pages/AttendancePage.cpp
        src/pages/ErrorPage.cpp
        src/pages/ProcessingPage.cpp
        src/pages/SignaturePage.cpp
        src/pages/SuccessPage.cpp
        src/pages/WaitingPage.cpp
        src/platform/linux/DisplayWindow.cpp
        src/platform/linux/LinuxDeviceDiscovery.cpp
        src/platform/linux/LinuxGpioOutput.cpp
        src/platform/linux/LinuxPwm.cpp
        src/platform/linux/LinuxPwmBuzzerOutput.cpp
        src/platform/linux/LinuxSpiDevice.cpp
        src/services/api/ApiClient.cpp
        src/services/api/ApiWorker.cpp
        src/services/display/BacklightController.cpp
        src/services/display/PerformanceMetrics.cpp
        src/services/hardware/HardwareService.cpp
        src/services/input/TouchInput.cpp
        src/services/config/AdminPinStore.cpp
        src/services/config/ApiUrlStore.cpp
        src/services/config/SecureFile.cpp
        src/services/network/NetworkConnection.cpp
        src/services/network/NetworkService.cpp
        src/services/network/WpaControlSocket.cpp
        src/ui/assets/UiAssets.cpp
        src/ui/components/ButtonFeedback.cpp
        src/ui/components/PageDrawing.cpp
        src/ui/components/PageTransition.cpp
        src/ui/components/PinKeypad.cpp
        src/ui/components/StatusAnimation.cpp
        src/ui/components/VectorIcons.cpp
        third_party/imgui/imgui.cpp
        third_party/imgui/imgui_draw.cpp
        third_party/imgui/imgui_tables.cpp
        third_party/imgui/imgui_widgets.cpp
        third_party/imgui/backends/imgui_impl_glfw.cpp
        third_party/imgui/backends/imgui_impl_opengl3.cpp
    )
    target_include_directories(clockin-device PRIVATE
        "${CLOCKIN_SOURCE_DIR}"
        "${IMGUI_DIR}"
        "${IMGUI_DIR}/backends"
        "${GLFW_DIR}/include"
        "${CURL_INCLUDE_DIR}"
    )
    target_compile_definitions(clockin-device PRIVATE
        GLFW_INCLUDE_ES2
        GLFW_EXPOSE_NATIVE_WAYLAND
        IMGUI_IMPL_OPENGL_ES2
    )
    target_link_libraries(clockin-device PRIVATE
        glfw
        "${GLES2_LIBRARY}"
        "${EGL_LIBRARY}"
        "${CURL_LIBRARY}"
        "${GPIOD_LIBRARY}"
        Threads::Threads
        ${CMAKE_DL_LIBS}
        m
    )
    clockin_enable_warnings(clockin-device)
endif()

if(CLOCKIN_BUILD_DIAGNOSTIC)
    add_executable(clockin-hardware-diagnostic
        src/platform/linux/HardwareDiagnostic.cpp
        src/platform/linux/LinuxDeviceDiscovery.cpp
        src/platform/linux/LinuxGpioOutput.cpp
        src/platform/linux/LinuxPwm.cpp
        src/platform/linux/LinuxPwmBuzzerOutput.cpp
        src/platform/linux/LinuxSpiDevice.cpp
        src/services/hardware/buzzer/BuzzerSequencer.cpp
        src/services/hardware/rfid/Mfrc522.cpp
    )
    target_include_directories(clockin-hardware-diagnostic PRIVATE "${CLOCKIN_SOURCE_DIR}")
    target_link_libraries(clockin-hardware-diagnostic PRIVATE
        "${GPIOD_LIBRARY}"
        Threads::Threads
    )
    clockin_enable_warnings(clockin-hardware-diagnostic)
endif()

if(CLOCKIN_BUILD_TESTS)
    enable_testing()
    add_executable(clockin-unit-tests
        tests/unit/TestMain.cpp
        tests/unit/BuzzerTests.cpp
        tests/unit/ConfigurationTests.cpp
        tests/unit/DomainTests.cpp
        tests/unit/JsonTests.cpp
        tests/unit/NetworkTests.cpp
        tests/unit/PageTests.cpp
        tests/unit/RfidTests.cpp
        tests/unit/SignatureTests.cpp
        tests/unit/TouchTests.cpp
        tests/unit/UiAnimationTests.cpp
        tests/unit/WorkflowTests.cpp
        src/app/AppConfiguration.cpp
        src/services/config/AdminPinStore.cpp
        src/services/config/ApiUrlStore.cpp
        src/services/config/SecureFile.cpp
        ${CLOCKIN_DOMAIN_SOURCES}
        src/pages/AdminPage.cpp
        src/pages/AdminPinPage.cpp
        src/pages/admin/AdminApiPanel.cpp
        src/pages/admin/AdminNetworkPanel.cpp
        src/pages/admin/AdminNetworkPanelDrawing.cpp
        src/pages/admin/AdminPinPanel.cpp
        src/pages/admin/AdminRfidPanel.cpp
        src/pages/admin/OnScreenKeyboard.cpp
        src/pages/AttendancePage.cpp
        src/pages/ErrorPage.cpp
        src/pages/ProcessingPage.cpp
        src/pages/SignaturePage.cpp
        src/pages/SuccessPage.cpp
        src/pages/WaitingPage.cpp
        src/services/input/TouchInput.cpp
        src/ui/components/ButtonFeedback.cpp
        src/ui/components/PageDrawing.cpp
        src/ui/components/PageTransition.cpp
        src/ui/components/PinKeypad.cpp
        src/ui/components/StatusAnimation.cpp
        src/ui/components/VectorIcons.cpp
        third_party/imgui/imgui.cpp
        third_party/imgui/imgui_draw.cpp
        third_party/imgui/imgui_tables.cpp
        third_party/imgui/imgui_widgets.cpp
    )
    target_include_directories(clockin-unit-tests PRIVATE
        "${CLOCKIN_SOURCE_DIR}"
        "${IMGUI_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
    )
    target_link_libraries(clockin-unit-tests PRIVATE pthread m)
    clockin_enable_warnings(clockin-unit-tests)
    if(CLOCKIN_ENABLE_SANITIZERS)
        target_compile_options(clockin-unit-tests PRIVATE
            -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(clockin-unit-tests PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME clockin-unit-tests COMMAND clockin-unit-tests)
endif()
