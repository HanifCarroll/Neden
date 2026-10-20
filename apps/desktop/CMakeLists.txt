cmake_minimum_required(VERSION 3.21)
project(NedenQt LANGUAGES CXX)
if(APPLE)
    enable_language(OBJCXX)
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
    add_compile_options(/W4 /permissive-)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wold-style-cast -Woverloaded-virtual -Werror=return-type)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)
option(NEDEN_CORE_ONLY "Build portable learning logic tests without playback or a WebView" OFF)
find_package(Qt6 REQUIRED COMPONENTS Test Qml Network Gui)
enable_testing()
set(NEDEN_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/app" "${CMAKE_CURRENT_SOURCE_DIR}/study"
    "${CMAKE_CURRENT_SOURCE_DIR}/subtitles" "${CMAKE_CURRENT_SOURCE_DIR}/playback"
    "${CMAKE_CURRENT_SOURCE_DIR}/platform")
include_directories(${NEDEN_INCLUDE_DIRS})
set(NEDEN_STUDY_SOURCES app/profile_store.cpp app/profile_store.h app/app_settings.cpp app/app_settings.h study/ai_client.cpp study/ai_client.h
    study/flashcard_deck.cpp study/flashcard_deck.h study/explanation_session.cpp study/explanation_session.h subtitles/subtitle_service.cpp subtitles/subtitle_service.h
    app/study_services.h study/word_lookup.cpp study/word_lookup.h study/conversation.cpp study/conversation.h study/study_target.h)
set(NEDEN_SESSION_SOURCES playback/media_queue.cpp playback/media_queue.h playback/player_preferences.cpp playback/player_preferences.h app/learning_session.cpp app/learning_session.h study/study_interaction.cpp study/study_interaction.h
    subtitles/subtitle_timeline.cpp subtitles/subtitle_timeline.h app/recent_videos.cpp app/recent_videos.h playback/playback_engine.h subtitles/cues.cpp subtitles/cues.h)

qt_add_executable(conversation_tests tests/conversation_tests.cpp study/conversation.cpp study/conversation.h)
target_link_libraries(conversation_tests PRIVATE Qt6::Test)
add_test(NAME conversation_tests COMMAND conversation_tests)
qt_add_executable(cue_tests tests/cue_tests.cpp subtitles/cues.cpp subtitles/cues.h)
target_link_libraries(cue_tests PRIVATE Qt6::Test Qt6::Qml)
add_test(NAME cue_tests COMMAND cue_tests)
qt_add_executable(session_tests tests/session_tests.cpp ${NEDEN_STUDY_SOURCES} ${NEDEN_SESSION_SOURCES})
target_link_libraries(session_tests PRIVATE Qt6::Test Qt6::Qml Qt6::Network Qt6::Gui)
add_test(NAME session_tests COMMAND session_tests)
qt_add_executable(recent_videos_tests tests/recent_videos_tests.cpp app/recent_videos.cpp app/recent_videos.h)
target_link_libraries(recent_videos_tests PRIVATE Qt6::Test Qt6::Qml)
add_test(NAME recent_videos_tests COMMAND recent_videos_tests)
qt_add_executable(study_tests tests/study_tests.cpp ${NEDEN_STUDY_SOURCES} app/subtitle_appearance.cpp app/subtitle_appearance.h subtitles/cues.cpp subtitles/cues.h)
target_link_libraries(study_tests PRIVATE Qt6::Test Qt6::Qml Qt6::Network)
add_test(NAME study_tests COMMAND study_tests)
qt_add_executable(video_geometry_tests tests/video_geometry_tests.cpp)
target_link_libraries(video_geometry_tests PRIVATE Qt6::Test)
add_test(NAME video_geometry_tests COMMAND video_geometry_tests)
qt_add_executable(video_render_lifetime_tests tests/video_render_lifetime_tests.cpp)
target_link_libraries(video_render_lifetime_tests PRIVATE Qt6::Test)
add_test(NAME video_render_lifetime_tests COMMAND video_render_lifetime_tests)
if(NEDEN_CORE_ONLY)
    return()
endif()
find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 OpenGL WebView Svg)
qt_add_executable(explanation_tests tests/explanation_tests.cpp study/explanation_format.cpp study/explanation_format.h)
target_link_libraries(explanation_tests PRIVATE Qt6::Test Qt6::Gui Qt6::Qml)
add_test(NAME explanation_tests COMMAND explanation_tests)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(AUDITED_MPV_PREFIX "" CACHE PATH "Explicit source-built LGPL runtime on Linux")
find_path(MPV_INCLUDE_DIR mpv/client.h HINTS "${AUDITED_MPV_PREFIX}/include" /opt/homebrew/include REQUIRED)
if(APPLE)
    set(PLATFORM_SOURCES platform/fullscreen_controller.mm platform/credentials_mac.mm)
    set(MEDIA_PLATFORM_SOURCE platform/platform_media_session_mac.mm)
else()
    set(PLATFORM_SOURCES platform/fullscreen_controller.cpp platform/credentials_unavailable.cpp)
    if(WIN32)
        set(MEDIA_PLATFORM_SOURCE platform/platform_media_session_windows.cpp)
    else()
        find_package(Qt6 REQUIRED COMPONENTS DBus)
        set(MEDIA_PLATFORM_SOURCE platform/platform_media_session_linux.cpp)
    endif()
endif()
qt_standard_project_setup(REQUIRES 6.9)
qt_add_executable(NedenQt MACOSX_BUNDLE app/main.cpp app/desktop_application.cpp app/desktop_application.h platform/platform_media_session.h ${MEDIA_PLATFORM_SOURCE})
if(APPLE)
    target_sources(NedenQt PRIVATE macos/Neden.icns)
    set_source_files_properties(macos/Neden.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
endif()
set_source_files_properties(ui/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set(NEDEN_UI_SOURCES playback/thumbnail_service.cpp playback/thumbnail_service.h ${NEDEN_STUDY_SOURCES} ${NEDEN_SESSION_SOURCES} app/subtitle_appearance.cpp app/subtitle_appearance.h
    app/media_path.h study/explanation_format.cpp study/explanation_format.h playback/mpv_playback.cpp playback/mpv_playback.h playback/video_render_lifetime.h
    playback/video.cpp playback/video.h playback/video_geometry.h platform/fullscreen_controller.h ${PLATFORM_SOURCES} platform/credentials.h)
set(NEDEN_QML_FILES ui/SegmentBar.qml ui/OptionsDialog.qml ui/SettingsGroup.qml ui/SettingsRow.qml ui/TimingControl.qml ui/PlaybackPreferences.qml ui/MediaDetails.qml ui/PlayerOptions.qml ui/PlayerShortcuts.qml ui/RecentVideosPane.qml ui/Main.qml ui/Theme.qml ui/ActionButton.qml ui/SearchField.qml
        ui/VideoStage.qml ui/PlaybackControls.qml ui/SubtitleText.qml ui/InspectorTab.qml ui/IconButton.qml ui/MeaningPane.qml ui/ChoiceBox.qml ui/ControlSlider.qml ui/SegmentButton.qml ui/TranscriptPane.qml ui/SubtitleTrackPicker.qml
        ui/StudyPane.qml ui/SettingsPane.qml ui/DictionaryPane.qml)
qt_add_qml_module(NedenQt URI Neden VERSION 1.0
    SOURCES ${NEDEN_UI_SOURCES}
    QML_FILES ${NEDEN_QML_FILES})
qt_add_resources(NedenQt fonts PREFIX "/fonts" BASE "../../shared/fonts"
    FILES ../../shared/fonts/Inter-Regular.otf ../../shared/fonts/Inter-Medium.otf
        ../../shared/fonts/Inter-SemiBold.otf ../../shared/fonts/LICENSE.txt)
file(GLOB NEDEN_ICON_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../../shared/icons/lucide/*.svg")
qt_add_resources(NedenQt icons PREFIX "/icons" BASE "../../shared/icons/lucide" FILES ${NEDEN_ICON_FILES})
qt_add_resources(NedenQt branding PREFIX "/branding" BASE "../../shared/branding" FILES ../../shared/branding/neden.png)
target_include_directories(NedenQt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(NedenQt PRIVATE Qt6::Network Qt6::Quick Qt6::QuickControls2 Qt6::OpenGL Qt6::WebView)
if(APPLE)
    set_source_files_properties(platform/fullscreen_controller.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    set_source_files_properties(platform/platform_media_session_mac.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    set_source_files_properties(platform/credentials_mac.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    target_link_libraries(NedenQt PRIVATE "-framework MediaPlayer" "-framework IOKit" "-framework CoreAudio" "-framework AppKit" "-framework Security" "-framework Foundation" "-framework LocalAuthentication")
endif()
if(WIN32)
    target_link_libraries(NedenQt PRIVATE windowsapp)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(NedenQt PRIVATE Qt6::DBus)
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
qt_add_executable(mpv_playback_tests tests/mpv_playback_tests.cpp playback/mpv_playback.cpp playback/mpv_playback.h playback/playback_engine.h)
target_include_directories(mpv_playback_tests PRIVATE ${MPV_INCLUDE_DIR})
target_link_libraries(mpv_playback_tests PRIVATE Qt6::Test Qt6::Qml Qt6::Gui "${REVIEWED_MPV_LIBRARY}")
get_target_property(REVIEWED_MPV_RPATH NedenQt BUILD_RPATH)
set_target_properties(mpv_playback_tests PROPERTIES BUILD_RPATH "${REVIEWED_MPV_RPATH}")
add_test(NAME mpv_playback_tests COMMAND mpv_playback_tests)
qt_add_executable(thumbnail_tests tests/thumbnail_tests.cpp playback/thumbnail_service.cpp playback/thumbnail_service.h)
target_include_directories(thumbnail_tests PRIVATE ${MPV_INCLUDE_DIR})
target_link_libraries(thumbnail_tests PRIVATE Qt6::Test Qt6::Qml Qt6::Gui "${REVIEWED_MPV_LIBRARY}")
set_target_properties(thumbnail_tests PROPERTIES BUILD_RPATH "${REVIEWED_MPV_RPATH}")
add_test(NAME thumbnail_tests COMMAND thumbnail_tests)

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
    qt_add_executable(NedenNativeChecks tests/native_ui_checks.cpp)
    if(APPLE)
        target_sources(NedenNativeChecks PRIVATE tests/native_menu_checks.mm)
    endif()
    qt_add_qml_module(NedenNativeChecks URI Neden VERSION 1.0
        OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/native-checks/Neden"
        SOURCES ${NEDEN_UI_SOURCES}
        QML_FILES ${NEDEN_QML_FILES})
    qt_add_resources(NedenNativeChecks check_fonts PREFIX "/fonts" BASE "../../shared/fonts"
        FILES ../../shared/fonts/Inter-Regular.otf ../../shared/fonts/Inter-Medium.otf
            ../../shared/fonts/Inter-SemiBold.otf ../../shared/fonts/LICENSE.txt)
    qt_add_resources(NedenNativeChecks check_icons PREFIX "/icons" BASE "../../shared/icons/lucide" FILES ${NEDEN_ICON_FILES})
    qt_add_resources(NedenNativeChecks check_branding PREFIX "/branding" BASE "../../shared/branding" FILES ../../shared/branding/neden.png)
    target_include_directories(NedenNativeChecks PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${MPV_INCLUDE_DIR})
    target_link_libraries(NedenNativeChecks PRIVATE Qt6::Test Qt6::Network Qt6::Quick
        Qt6::QuickControls2 Qt6::OpenGL Qt6::WebView "${REVIEWED_MPV_LIBRARY}")
    set_target_properties(NedenNativeChecks PROPERTIES BUILD_RPATH "${REVIEWED_MPV_RPATH}")
    if(APPLE)
        target_link_libraries(NedenNativeChecks PRIVATE "-framework AppKit" "-framework Security"
            "-framework Foundation" "-framework LocalAuthentication")
    endif()
endif()
