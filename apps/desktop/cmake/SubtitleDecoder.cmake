# Use headers matching the already-reviewed runtime, never system FFmpeg headers
# against a different bundled ABI. No additional codec runtime is shipped.
if(APPLE)
    include(FetchContent)
    FetchContent_Declare(neden_ffmpeg_headers
        URL https://ffmpeg.org/releases/ffmpeg-6.0.tar.xz
        URL_HASH SHA256=57be87c22d9b49c112b6d24bc67d42508660e6b718b3db89c44e47e289137082
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(neden_ffmpeg_headers)
    if(NOT EXISTS "${neden_ffmpeg_headers_SOURCE_DIR}/libavutil/avconfig.h")
        execute_process(COMMAND sh ./configure --disable-everything --disable-autodetect --disable-x86asm
            --disable-programs --disable-doc
            WORKING_DIRECTORY "${neden_ffmpeg_headers_SOURCE_DIR}"
            OUTPUT_QUIET COMMAND_ERROR_IS_FATAL ANY)
    endif()
    # A source-root VERSION file shadows C++ <version> on case-insensitive disks.
    set(NEDEN_AV_INCLUDE "${CMAKE_CURRENT_BINARY_DIR}/subtitle-decoder-headers")
    file(COPY "${neden_ffmpeg_headers_SOURCE_DIR}/libavformat"
        "${neden_ffmpeg_headers_SOURCE_DIR}/libavcodec" "${neden_ffmpeg_headers_SOURCE_DIR}/libavutil"
        DESTINATION "${NEDEN_AV_INCLUDE}" FILES_MATCHING PATTERN "*.h")
    set(NEDEN_AV_LIBRARIES "${NEDEN_MPV_FRAMEWORKS}/Avformat.framework/Avformat"
        "${NEDEN_MPV_FRAMEWORKS}/Avcodec.framework/Avcodec" "${NEDEN_MPV_FRAMEWORKS}/Avutil.framework/Avutil")
else()
    find_path(NEDEN_AV_INCLUDE libavformat/avformat.h PATHS "${AUDITED_MPV_PREFIX}/include" NO_DEFAULT_PATH REQUIRED)
    foreach(component avformat avcodec avutil)
        find_library(NEDEN_${component}_LIBRARY NAMES ${component} PATHS "${AUDITED_MPV_PREFIX}/lib" NO_DEFAULT_PATH REQUIRED)
        list(APPEND NEDEN_AV_LIBRARIES "${NEDEN_${component}_LIBRARY}")
    endforeach()
endif()
add_library(neden_subtitle_decoder STATIC subtitles/embedded_subtitles.cpp)
target_include_directories(neden_subtitle_decoder SYSTEM PRIVATE "${NEDEN_AV_INCLUDE}")
target_link_libraries(neden_subtitle_decoder PUBLIC Qt6::Qml "${NEDEN_AV_LIBRARIES}")
