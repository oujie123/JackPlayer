//
// Created by JackOu on 2021/7/21.
//

#ifndef JACKPLAYER_JACKPLAYER_H
#define JACKPLAYER_JACKPLAYER_H

#include <cstring>
#include <pthread.h>
#include "AudioChannel.h"
#include "VideoChannel.h"
#include "JNICallbackHelper.h"
#include "utils.h"
#include "safe_queue.h"

extern "C" { // ffmpeg是纯c写的，必须采用c的编译方式，否则奔溃
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
};

class JackPlayer {

private:
    char *sourceData = 0; // 防止野指针
    bool isPlaying = 0;
    pthread_t pid_prepare;
    pthread_t pid_start;
    AVFormatContext *formatContext = 0;
    AudioChannel *audioChannel = 0;
    VideoChannel *videoChannel = 0;
    JNICallbackHelper *helper = 0;
    RenderCallback renderCallback; // VideoChannel.h已经定义，可以直接使用

public:
    JackPlayer(const char *sourceData, JNICallbackHelper *helper);

    ~JackPlayer();

    void prepare();   // 创建线程
    void prepare_();  //子线程线程

    void start();

    void start_();

    void setRenderCallback(RenderCallback renderCallback);
};

#endif //JACKPLAYER_JACKPLAYER_H
