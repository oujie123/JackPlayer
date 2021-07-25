//
// Created by JackOu on 2021/7/21.
//

#ifndef JACKPLAYER_VIDEOCHANNEL_H
#define JACKPLAYER_VIDEOCHANNEL_H

#include "BaseChannel.h"
#include "AudioChannel.h"

extern "C" {
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
};

typedef void(*RenderCallback)(uint8_t *, int, int, int);

class VideoChannel : public BaseChannel{

private:
    pthread_t pid_video_decode;
    pthread_t pid_video_play;
    RenderCallback renderCallback;

    int fps; // 一秒多少帧
    AudioChannel *audioChannel = nullptr;

public:
    VideoChannel(int stream_index,AVCodecContext *avCodecContext1, AVRational timeBase, int fps);

    virtual ~VideoChannel();

    void start();

    void stop();

    void videoDecode();

    void videoPlay();

    void setRenderCallback(RenderCallback renderCallback);

    void setAudioChannel(AudioChannel *pChannel);
};

#endif //JACKPLAYER_VIDEOCHANNEL_H
