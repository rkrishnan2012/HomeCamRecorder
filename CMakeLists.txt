set(CMAKE_FIND_DEBUG_MODE TRUE)

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


if(NOT AVFORMAT_LIBRARY)
    message(FATAL_ERROR "AVFORMAT_LIBRARY not found")
endif()
if(NOT AVUTIL_INCLUDE_DIR)
    message(FATAL_ERROR "AVUTIL_INCLUDE_DIR not found")
endif()

add_executable(HomeCamRecorder main.cpp)
target_include_directories(HomeCamRecorder PRIVATE ${AVCODEC_INCLUDE_DIR} ${AVFORMAT_INCLUDE_DIR} ${AVUTIL_INCLUDE_DIR} ${AVDEVICE_INCLUDE_DIR})
target_link_libraries(HomeCamRecorder PRIVATE ${AVCODEC_LIBRARY} ${AVFORMAT_LIBRARY} ${AVUTIL_LIBRARY} ${AVDEVICE_LIBRARY})