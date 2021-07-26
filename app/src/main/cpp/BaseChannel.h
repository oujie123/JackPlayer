//
// Created by JackOu on 2021/7/24.
//

#ifndef JACKPLAYER_BASECHANNEL_H
#define JACKPLAYER_BASECHANNEL_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/time.h>
};

#include "safe_queue.h"
#include "log4c.h"
#include "JNICallbackHelper.h"

#define SOURCE_QUEUE_THRESHOLD 100
#define PRODUCER_WAITING_TIME 10 * 1000

class BaseChannel {

public:
    int stream_index; // 音频或者视频下标
    SafeQueue<AVPacket * > packets; // 压缩的 数据包
    SafeQueue<AVFrame *> frames; // 原始的 数据包
    bool isPlaying; // 音频 和 视频 都会有的标记 是否播放
    AVCodecContext * avCodecContext = 0;

    AVRational time_base;  // 音视频同步参数

    JNICallbackHelper *jniCallbackHelper = 0;

    void setJNICallbackHelper(JNICallbackHelper *jniCallbackHelper){
        this->jniCallbackHelper = jniCallbackHelper;
    }

    BaseChannel(int stream_index,AVCodecContext *avCodecContext1, AVRational time_base);

    ~BaseChannel();

    // 声明成静态，子类可以随意拿到
    static void releaseAVPacket(AVPacket **pPacket);

    // 声明成静态，子类可以随意拿到
    static void releaseAVFrame(AVFrame **pFrame);
};

#endif //JACKPLAYER_BASECHANNEL_H
