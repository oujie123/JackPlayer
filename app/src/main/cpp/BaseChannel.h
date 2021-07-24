//
// Created by JackOu on 2021/7/24.
//

#ifndef JACKPLAYER_BASECHANNEL_H
#define JACKPLAYER_BASECHANNEL_H

extern "C" {
#include <libavcodec/avcodec.h>
};

#include "safe_queue.h"
#include "log4c.h"

class BaseChannel {

public:
    int stream_index; // 音频或者视频下标
    SafeQueue<AVPacket * > packets; // 压缩的 数据包
    SafeQueue<AVFrame *> frames; // 原始的 数据包
    bool isPlaying; // 音频 和 视频 都会有的标记 是否播放
    AVCodecContext * avCodecContext = 0;

    BaseChannel(int stream_index,AVCodecContext *avCodecContext1);

    ~BaseChannel();

    // 声明成静态，子类可以随意拿到
    static void releaseAVPacket(AVPacket **pPacket);

    // 声明成静态，子类可以随意拿到
    static void releaseAVFrame(AVFrame **pFrame);
};

#endif //JACKPLAYER_BASECHANNEL_H
