cmake_minimum_required(VERSION 3.4.1)

set(FFMPEG ${CMAKE_SOURCE_DIR}/ffmpeg)  #定义FFMPEG变量
set(RTMP ${CMAKE_SOURCE_DIR}/rtmp) #定义RTMP变量

include_directories(${FFMPEG}/include) #添加FFMPEG头文件

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L${FFMPEG}/libs/${CMAKE_ANDROID_ARCH_ABI}") #指定ffmpeg静态库
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L${RTMP}/libs/${CMAKE_ANDROID_ARCH_ABI}")

file(GLOB src_files *.cpp *.c)

add_library(
     native-lib
     SHARED
     ${src_files} )

find_library(
      log-lib
      log )

target_link_libraries(
            native-lib  # 链接总库libnative-lib.so

            # 忽略导入顺序的方式
            -Wl,--start-group
            avcodec avfilter avformat avutil swresample swscale
            -Wl,--end-group
            z # libz.so库，是FFmpeg需要用ndk的z库，FFMpeg需要额外支持libz.so
            rtmp
            android # android ANativeWindow 用来渲染画面的
            OpenSLES # OpenSLES 用来播放声音的
            ${log-lib} )