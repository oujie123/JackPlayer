//
// Created by JackOu on 2021/7/21.
//

#ifndef JACKPLAYER_AUDIOCHANNEL_H
#define JACKPLAYER_AUDIOCHANNEL_H

#include "BaseChannel.h"

class AudioChannel : public BaseChannel{

public:
    AudioChannel(int stream_index, AVCodecContext * avCodecContext);

    virtual ~AudioChannel();

    void start();

    void stop();
};

#endif //JACKPLAYER_AUDIOCHANNEL_H
