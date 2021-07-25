//
// Created by JackOu on 2021/7/24.
//

#include "BaseChannel.h"

BaseChannel::BaseChannel(int stream_index, AVCodecContext *avCodecContext1, AVRational timeBase)
        : stream_index(stream_index), avCodecContext(avCodecContext1), time_base(timeBase) {
    packets.setReleaseCallback(releaseAVPacket);
    frames.setReleaseCallback(releaseAVFrame);
}

BaseChannel::~BaseChannel() {
    packets.clear();
    frames.clear();
}

/**
 * free AVPacket
 *
 * @param p
 */
void BaseChannel::releaseAVPacket(AVPacket **p) {
    if (p) {
        av_packet_free(p);
        *p = 0;
    }
}

/**
 * free AVFrame
 *
 * @param p
 */
void BaseChannel::releaseAVFrame(AVFrame **p) {
    if (p) {
        av_frame_free(p);
        *p = 0;
    }
}

