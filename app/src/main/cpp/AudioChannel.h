//
// Created by JackOu on 2021/7/21.
//

#ifndef JACKPLAYER_AUDIOCHANNEL_H
#define JACKPLAYER_AUDIOCHANNEL_H

#include "BaseChannel.h"
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

extern "C" {
#include <libswresample/swresample.h>
};

class AudioChannel : public BaseChannel{

private:
    pthread_t pid_audio_decode;
    pthread_t pid_audio_play;

public:
    int out_channels;
    int out_sample_size;
    int out_sample_rate;
    size_t out_buffer_size;
    uint8_t *out_buffer = 0;
    SwrContext *swr_context = 0;

public:
    SLObjectItf engineObject = 0;  // 引擎对象
    SLEngineItf engineItf = 0; // 引擎接口
    SLObjectItf outputMixObject = 0; // 混音器
    SLEnvironmentalReverbItf outputMixEnvironmentalReverb = 0; // 混响器
    SLObjectItf bqPlayerObject = 0; // 播放器
    SLPlayItf bqPlayerItf = 0; // 播放接口
    SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue = 0; // 播放器队列接口

public:
    AudioChannel(int stream_index, AVCodecContext * avCodecContext);

    virtual ~AudioChannel();

    void start();

    void stop();

    void audioDecode();

    void audioPlay();

    int getPcm();
};

#endif //JACKPLAYER_AUDIOCHANNEL_H
