//
// Created by JackOu on 2021/7/23.
//

#include "AudioChannel.h"

AudioChannel::AudioChannel(int stream_index, AVCodecContext *avCodecContext)
        : BaseChannel(stream_index, avCodecContext) {

}

AudioChannel::~AudioChannel() {

}

void AudioChannel::start() {

}

void AudioChannel::stop() {

}
