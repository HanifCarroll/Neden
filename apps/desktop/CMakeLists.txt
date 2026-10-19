cmake_minimum_required(VERSION 3.21)
project(NedenQt LANGUAGES CXX)
if(APPLE)
    enable_language(OBJCXX)
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
option(NEDEN_CORE_ONLY "Build portable learning logic tests without playback or a WebView" OFF)
find_package(Qt6 REQUIRED COMPONENTS Test Qml Network)
enable_testing()
qt_add_executable(cue_tests tests.cpp cues.cpp cues.h)
target_link_libraries(cue_tests PRIVATE Qt6::Test Qt6::Qml)
add_test(NAME cue_tests COMMAND cue_tests)
qt_add_executable(session_tests study_tools.cpp study_tools.h recent_videos.cpp recent_videos.h session_tests.cpp learning_session.cpp learning_session.h playback_engine.h cues.cpp cues.h)
target_link_libraries(session_tests PRIVATE Qt6::Test Qt6::Qml Qt6::Network)
add_test(NAME session_tests COMMAND session_tests)
qt_add_executable(recent_videos_tests recent_videos_tests.cpp recent_videos.cpp recent_videos.h)
target_link_libraries(recent_videos_tests PRIVATE Qt6::Test Qt6::Qml)
add_test(NAME recent_videos_tests COMMAND recent_videos_tests)
qt_add_executable(study_tests study_tests.cpp study_tools.cpp study_tools.h cues.cpp cues.h)
target_link_libraries(study_tests PRIVATE Qt6::Test Qt6::Qml Qt6::Network)
add_test(NAME study_tests COMMAND study_tests)
qt_add_executable(video_geometry_tests video_geometry_tests.cpp)
target_link_libraries(video_geometry_tests PRIVATE Qt6::Test)
add_test(NAME video_geometry_tests COMMAND video_geometry_tests)
qt_add_executable(video_render_lifetime_tests video_render_lifetime_tests.cpp)
target_link_libraries(video_render_lifetime_tests PRIVATE Qt6::Test)
add_test(NAME video_render_lifetime_tests COMMAND video_render_lifetime_tests)
if(NEDEN_CORE_ONLY)
    return()
endif()
find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 OpenGL WebView)
qt_add_executable(explanation_tests explanation_tests.cpp explanation_format.cpp explanation_format.h)
target_link_libraries(explanation_tests PRIVATE Qt6::Test Qt6::Gui Qt6::Qml)
add_test(NAME explanation_tests COMMAND explanation_tests)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(AUDITED_MPV_PREFIX "" CACHE PATH "Explicit source-built LGPL runtime on Linux")
find_path(MPV_INCLUDE_DIR mpv/client.h HINTS "${AUDITED_MPV_PREFIX}/include" /opt/homebrew/include REQUIRED)
if(APPLE)
    set(PLATFORM_SOURCES fullscreen_controller.mm credentials_mac.mm)
else()
    set(PLATFORM_SOURCES fullscreen_controller.cpp credentials_unavailable.cpp)
endif()
qt_standard_project_setup(REQUIRES 6.8)
qt_add_executable(NedenQt MACOSX_BUNDLE main.cpp desktop_application.cpp desktop_application.h)
if(APPLE)
    target_sources(NedenQt PRIVATE macos/Neden.icns)
    set_source_files_properties(macos/Neden.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
endif()
set_source_files_properties(ui/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set(NEDEN_UI_SOURCES recent_videos.cpp recent_videos.h explanation_format.cpp explanation_format.h learning_session.cpp learning_session.h playback_engine.h mpv_playback.cpp mpv_playback.h video_render_lifetime.h
        cues.cpp cues.h study_tools.cpp study_tools.h video.cpp video.h video_geometry.h
        fullscreen_controller.h ${PLATFORM_SOURCES} credentials.h)
set(NEDEN_QML_FILES ui/RecentVideosPane.qml ui/Main.qml ui/Theme.qml ui/ActionButton.qml ui/SearchField.qml ui/PlayerHeader.qml
        ui/VideoStage.qml ui/PlaybackControls.qml ui/SelectionPanel.qml ui/TranscriptPane.qml
        ui/StudyPane.qml ui/SettingsPane.qml ui/DictionaryPane.qml ui/Glyph.qml)
qt_add_qml_module(NedenQt URI Neden VERSION 1.0
    SOURCES ${NEDEN_UI_SOURCES}
    QML_FILES ${NEDEN_QML_FILES})
qt_add_resources(NedenQt fonts PREFIX "/fonts" BASE "../../shared/fonts"
    FILES ../../shared/fonts/Inter-Regular.otf ../../shared/fonts/Inter-Medium.otf
        ../../shared/fonts/Inter-SemiBold.otf ../../shared/fonts/LICENSE.txt)
target_include_directories(NedenQt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(NedenQt PRIVATE Qt6::Network Qt6::Quick Qt6::QuickControls2 Qt6::OpenGL Qt6::WebView)
if(APPLE)
    set_source_files_properties(fullscreen_controller.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    set_source_files_properties(credentials_mac.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    target_link_libraries(NedenQt PRIVATE "-framework AppKit" "-framework Security" "-framework Foundation" "-framework LocalAuthentication")
endif()
set(NEDEN_MPV_FRAMEWORKS "" CACHE PATH "Path to reviewed LGPL macOS playback frameworks")
if(APPLE AND NEDEN_MPV_FRAMEWORKS)
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../../scripts/check_playback_licenses.py"
        "${NEDEN_MPV_FRAMEWORKS}" COMMAND_ERROR_IS_FATAL ANY)
    target_include_directories(NedenQt PRIVATE ${MPV_INCLUDE_DIR})
    set(REVIEWED_MPV_LIBRARY "${NEDEN_MPV_FRAMEWORKS}/Mpv.framework/Mpv")
    set_target_properties(NedenQt PROPERTIES BUILD_RPATH "${NEDEN_MPV_FRAMEWORKS}")
elseif(UNIX AND NOT APPLE AND AUDITED_MPV_PREFIX)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env "LD_LIBRARY_PATH=${AUDITED_MPV_PREFIX}/lib"
        "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../../scripts/check_linux_playback.py"
        "${AUDITED_MPV_PREFIX}" COMMAND_ERROR_IS_FATAL ANY)
    target_include_directories(NedenQt PRIVATE ${MPV_INCLUDE_DIR})
    set(REVIEWED_MPV_LIBRARY "${AUDITED_MPV_PREFIX}/lib/libmpv.so")
    set_target_properties(NedenQt PROPERTIES BUILD_RPATH "${AUDITED_MPV_PREFIX}/lib")
else()
    message(FATAL_ERROR "Provide NEDEN_MPV_FRAMEWORKS on macOS. Other platforms need an audited LGPL runtime; do not fall back to system mpv.")
endif()
target_link_libraries(NedenQt PRIVATE "${REVIEWED_MPV_LIBRARY}")
qt_add_executable(mpv_playback_tests mpv_playback_tests.cpp mpv_playback.cpp mpv_playback.h playback_engine.h)
target_include_directories(mpv_playback_tests PRIVATE ${MPV_INCLUDE_DIR})
target_link_libraries(mpv_playback_tests PRIVATE Qt6::Test Qt6::Qml Qt6::Gui "${REVIEWED_MPV_LIBRARY}")
get_target_property(REVIEWED_MPV_RPATH NedenQt BUILD_RPATH)
set_target_properties(mpv_playback_tests PROPERTIES BUILD_RPATH "${REVIEWED_MPV_RPATH}")
add_test(NAME mpv_playback_tests COMMAND mpv_playback_tests)
set_target_properties(NedenQt PROPERTIES
    OUTPUT_NAME Neden
    MACOSX_BUNDLE_BUNDLE_NAME Neden
    MACOSX_BUNDLE_GUI_IDENTIFIER io.github.hanifcarroll.neden.qt-lab
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/macos/Info.plist.in")

if(APPLE)
    set(NEDEN_SIGNING_IDENTITY "Apple Development" CACHE STRING "Persistent Keychain identity for local macOS builds")
    if(NEDEN_SIGNING_IDENTITY STREQUAL "" OR NEDEN_SIGNING_IDENTITY STREQUAL "-")
        message(FATAL_ERROR "Choose a persistent NEDEN_SIGNING_IDENTITY; ad hoc signing invalidates Keychain approvals after rebuilds.")
    endif()
    add_custom_command(TARGET NedenQt POST_BUILD
        COMMAND /usr/bin/codesign --force --sign "${NEDEN_SIGNING_IDENTITY}"
            --identifier io.github.hanifcarroll.neden.qt-lab --timestamp=none "$<TARGET_BUNDLE_DIR:NedenQt>"
        COMMAND /usr/bin/codesign --verify --strict "$<TARGET_BUNDLE_DIR:NedenQt>"
        COMMENT "Sign Qt with the persistent development identity"
        VERBATIM)
endif()

# Optional native interaction checks run only on an explicitly selected real Mac display.
option(NEDEN_NATIVE_UI_CHECKS "Build the opt-in native interaction check executable" OFF)
if(NEDEN_NATIVE_UI_CHECKS)
    qt_add_executable(NedenNativeChecks native_ui_checks.cpp)
    qt_add_qml_module(NedenNativeChecks URI Neden VERSION 1.0
        OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/native-checks/Neden"
        SOURCES ${NEDEN_UI_SOURCES}
        QML_FILES ${NEDEN_QML_FILES})
    qt_add_resources(NedenNativeChecks check_fonts PREFIX "/fonts" BASE "../../shared/fonts"
        FILES ../../shared/fonts/Inter-Regular.otf ../../shared/fonts/Inter-Medium.otf
            ../../shared/fonts/Inter-SemiBold.otf ../../shared/fonts/LICENSE.txt)
    target_include_directories(NedenNativeChecks PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${MPV_INCLUDE_DIR})
    target_link_libraries(NedenNativeChecks PRIVATE Qt6::Test Qt6::Network Qt6::Quick
        Qt6::QuickControls2 Qt6::OpenGL Qt6::WebView "${REVIEWED_MPV_LIBRARY}")
    set_target_properties(NedenNativeChecks PROPERTIES BUILD_RPATH "${REVIEWED_MPV_RPATH}")
    if(APPLE)
        target_link_libraries(NedenNativeChecks PRIVATE "-framework AppKit" "-framework Security"
            "-framework Foundation" "-framework LocalAuthentication")
    endif()
endif()
