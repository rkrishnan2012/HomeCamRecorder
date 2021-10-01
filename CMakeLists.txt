cmake_minimum_required(VERSION 3.20)
project(HomeCamRecorder)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "-D__STDC_CONSTANT_MACROS")

find_path(AVCODEC_INCLUDE_DIR libavcodec/avcodec.h)
find_library(AVCODEC_LIBRARY avcodec)

find_path(AVFORMAT_INCLUDE_DIR libavformat/avformat.h)
find_library(AVFORMAT_LIBRARY avformat)

find_path(AVUTIL_INCLUDE_DIR libavutil/avutil.h)
find_library(AVUTIL_LIBRARY avutil)

find_path(AVDEVICE_INCLUDE_DIR libavdevice/avdevice.h)
find_library(AVDEVICE_LIBRARY avdevice)

find_path(SWRESAMPLE_INCLUDE_DIR libswresample/swresample.h)
find_library(SWRESAMPLE_LIBRARY swresample)

find_library(MATH_LIBRARY m)
find_library(Z_LIBRARY z)
find_library(P_THREAD_LIBRARY pthread)
find_library(X264_LIBRARY x264)
find_library(X265_LIBRARY x265)
find_library(RTMP_LIBRARY rtmp)
find_library(DRM_LIBRARY drm)

add_executable(HomeCamRecorder main.cpp RotatingFileMuxer.cpp FLVMuxer.cpp Muxer.h MotionDetector.cpp MotionDetector.h)
target_include_directories(HomeCamRecorder PRIVATE ${AVCODEC_INCLUDE_DIR}
        ${AVFORMAT_INCLUDE_DIR} ${AVUTIL_INCLUDE_DIR} ${AVDEVICE_INCLUDE_DIR} ${SWRESAMPLE_INCLUDE_DIR})

if(NOT DRM_LIBRARY)
    target_link_libraries(HomeCamRecorder PRIVATE
            ${AVFORMAT_LIBRARY} ${AVCODEC_LIBRARY}
            ${AVUTIL_LIBRARY} ${AVDEVICE_LIBRARY} ${SWRESAMPLE_LIBRARY}
            ${MATH_LIBRARY} ${Z_LIBRARY} ${P_THREAD_LIBRARY} ${X264_LIBRARY}
            ${X265_LIBRARY})
else()
    target_link_libraries(HomeCamRecorder PRIVATE
            ${AVFORMAT_LIBRARY} ${AVCODEC_LIBRARY}
            ${AVUTIL_LIBRARY} ${AVDEVICE_LIBRARY} ${SWRESAMPLE_LIBRARY}
            ${MATH_LIBRARY} ${Z_LIBRARY} ${P_THREAD_LIBRARY} ${X264_LIBRARY}
            ${X265_LIBRARY} ${DRM_LIBRARY} ${RTMP_LIBRARY})
endif()
