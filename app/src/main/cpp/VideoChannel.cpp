//
// Created by JackOu on 2021/7/23.
//

#include "VideoChannel.h"

VideoChannel::VideoChannel(int stream_index, AVCodecContext *avCodecContext1) : BaseChannel(
        stream_index, avCodecContext1) {

}

VideoChannel::~VideoChannel() {

}

void *task_video_decode(void *args) {
    VideoChannel *channel = static_cast<VideoChannel *>(args);
    channel->videoDecode();
    return nullptr;
}

void *task_video_play(void *args) {
    VideoChannel *channel = static_cast<VideoChannel *>(args);
    channel->videoPlay();
    return nullptr;
}

/**
 * 解码和播放视频
 */
void VideoChannel::start() {
    isPlaying = true;

    // 队列开始工作了
    packets.setWork(1);
    frames.setWork(1);

    // 第一个线程： 视频：取出队列的压缩包 进行解码，解码后的原始包 再push队列中去
    pthread_create(&pid_video_decode, nullptr, task_video_decode, this);

    // 第二线线程：视频：从队列取出原始包，播放
    pthread_create(&pid_video_play, nullptr, task_video_play, this);
}

void VideoChannel::stop() {

}

/**
 * TODO 优化视频帧队列大小
 *
 * 将原始视频包取出I帧，然后放入frame队列
 */
void VideoChannel::videoDecode() {
    AVPacket *packet = nullptr;
    while (isPlaying) {
        // TODO 2.1 控制在队列的视频帧数量
        if (frames.size() > SOURCE_QUEUE_THRESHOLD) {
            av_usleep(PRODUCER_WAITING_TIME);
            continue;
        }

        int ret = packets.getQueueAndDel(packet);
        if (!isPlaying) {
            // 阻塞方法，可能拿到packet已经不播放了，直接跳出循环，释放资源
            break;
        }
        if (!ret) {
            // 没有那成功就继续去拿
            continue;
        }

        // 1.发送压缩包到缓冲区
        ret = avcodec_send_packet(avCodecContext, packet);

        // 发送完成之后ffmpeg会缓存一份，可以放心释放
        // TODO 优化 2.3: 释放时机应该是当frame队列之后，然后还要释放所指堆空间的内存,应该将该逻辑放在后面
        // releaseAVPacket(&packet);

        if (ret) {
            // 发送失败释放资源
            break;
        }

        // 从缓冲区获取原始包(AVFrame)
        AVFrame *frame = av_frame_alloc();
        ret = avcodec_receive_frame(avCodecContext, frame);
        if (ret == AVERROR(EAGAIN)) {
            // 当前真不可用，可能是B帧参考前面成功  B帧参考后面失败   可能是P帧没有出来，再拿一次就行了
            continue;
        } else if (ret != 0) {
            // TODO 优化2.2 :获取帧出错，应该全部释放frame和packet
            if (frame) {
                releaseAVFrame(&frame);
                frame = nullptr;
            }
            break;
        }

        // 成功拿到原始帧了，加入到Frames队列
        frames.insertToQueue(frame);

        // TODO 优化 3.1: 释放时机应该是当frame队列之后，然后还要释放所指堆空间的内存
        // 先释放引用，再释放堆
        av_packet_unref(packet);  // 成员所指的堆空间
        releaseAVPacket(&packet);
    }
    // TODO 优化 3.2: 流程出错跳出循环内存释放点
    av_packet_unref(packet);
    releaseAVPacket(&packet);
}

/**
 * 从frame队列中取出帧转化成RGBA数据显示在屏幕中
 */
void VideoChannel::videoPlay() {
    // sws_getContext中flags参数代表视屏转换算法
    // SWS_FAST_BILINEAR代表转化效率高，但是可能会模糊
    // SWS_BILINEAR 转化速度适中

    // 原始包（YUV） -> 转化[libswscale] -> Android 屏幕（RGBA数据）

    AVFrame *frame = 0;
    uint8_t *dst_data[4]; // RGBA每一行四路对应的色彩值
    int dst_linesize[4];  // RGBA每一行有色彩值的大小

    /**
     *   此函数的功能是按照指定的宽、高、像素格式来分析图像内存
     *   pointers[4]：保存图像通道的地址。如果是RGB，则前三个指针分别指向R,G,B的内存地址。第四个指针保留不用
     *   linesizes[4]：保存图像每个通道的内存对齐的步长，即一行的对齐内存的宽度，此值大小等于图像宽度。
     *   w:            要申请内存的图像宽度。
     *   h:            要申请内存的图像高度。
     *   pix_fmt:      要申请内存的图像的像素格式。
     *   align:        用于内存对齐的值。
     *   返回值：所申请的内存空间的总大小。如果是负值，表示申请失败。
     */
    av_image_alloc(dst_data, dst_linesize,
                   avCodecContext->width, avCodecContext->height, AV_PIX_FMT_RGBA, 1);
    SwsContext *swsContext = sws_getContext(
            // 输入环节
            avCodecContext->width,
            avCodecContext->height,
            avCodecContext->pix_fmt,   //AV_PIX_FMT_YUV420P

            // 输出环节
            avCodecContext->width,
            avCodecContext->height,
            AV_PIX_FMT_RGBA,
            SWS_BILINEAR, NULL, NULL, NULL);

    while (isPlaying) {
        int ret = frames.getQueueAndDel(frame);
        if (!isPlaying) {
            break; // 如果关闭了播放，跳出循环，releaseAVPacket(&pkt);
        }
        if (!ret) { // ret == 0
            continue; // 哪怕是没有成功，也要继续（假设：你生产太慢(原始包加入队列)，我消费就等一下你）
        }

        // 格式转换  yuv -> rgba
        sws_scale(swsContext,
                // 输入
                  frame->data,
                  frame->linesize,
                  0, avCodecContext->height,

                // 输出
                  dst_data,
                  dst_linesize);

        // 如何渲染一帧图像？
        // 答：宽，高，数据  ----> 函数指针的声明
        // 由于此方法拿不到Surface，只能回调给 native-lib.cpp

        // 数组被传递会退化成指针，默认就是去1元素
        renderCallback(dst_data[0], avCodecContext->width, avCodecContext->height, dst_linesize[0]);

        // releaseAVFrame(&frame); // 显示完成，数据立马释放

        // TODO 优化5.1: Frame 释放
        av_frame_unref(frame);
        releaseAVFrame(&frame);
    } // end while
    // TODO 优化5.2: 出错时，Frame 释放
    av_frame_unref(frame);
    releaseAVFrame(&frame);
    isPlaying = 0;
    av_free(&dst_data[0]);
    sws_freeContext(swsContext);
}

void VideoChannel::setRenderCallback(RenderCallback renderCallback) {
    this->renderCallback = renderCallback;
}
