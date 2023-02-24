cmake_minimum_required(VERSION 3.24)
project(decode_encode)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "-D__STDC_CONSTANT_MACROS")


include_directories(../ffmpeg-4.1/build/include)

link_directories(../ffmpeg-4.1/build/lib)

add_executable(video_player main.cpp)

target_link_libraries(video_player avformat avcodec avutil swscale SDL2)