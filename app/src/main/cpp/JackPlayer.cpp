//
// Created by JackOu on 2021/7/22.
//

#include "JackPlayer.h"


JackPlayer::JackPlayer(const char *sourceData, JNICallbackHelper *helper) {
    // this->sourceData = sourceData;  //此处sourceData在prepareNative中方法出栈就会被释放，如果被释放就会成为悬空指针，所以需要用深拷贝

    // 深拷贝
    // 在java: demo.mp4
    // 在C++: demo.mp4 \0  但是strlen不会计算\0,所以长度需要+1；
    this->sourceData = new char[strlen(sourceData) + 1];
    strcpy(this->sourceData, sourceData);

    this->helper = helper;
}

JackPlayer::~JackPlayer() {
    if (sourceData) {
        delete sourceData;
    }
    if (helper) {
        delete helper;
    }
}

/**
 * 预备工作线程执行方法
 *
 * @param args JackPlayer对象，可以通过该对象拿到私有方法
 * @return
 */
void *task_prepare(void *args) {
    // 此方法必须卸载prepare()上面，否者编译器找不到该方法
    // 子线程的函数，this拿不到对象
    // avformat_open_input(0, this->data_source)

    //JackPlayer *player = static_cast<JackPlayer *>(args);
    auto *player = static_cast<JackPlayer *>(args);
    player->prepare_();

    // 必须返回，坑，错误很难找
    return 0;
}

// 此函数在子线程
void JackPlayer::prepare_() {

    //FFmpeg源码是纯C的，他不像C++、Java ， 上下文的出现是为了贯彻环境，就相当于Java的this能够操作成员
    // 只是分配了一个对象，内容avformat_open_input来填写
    formatContext = avformat_alloc_context();

    // TODO 第一步：打开媒体地址（文件路径， 直播地址rtmp）
    AVDictionary *dictionary = 0;
    av_dict_set(&dictionary, "timeout", "5000000", 0);

    /**
     * 1，AVFormatContext *
     * 2，路径
     * 3，AVInputFormat *fmt  Mac、Windows 摄像头、麦克风， 目前安卓用不到
     * 4，各种设置：例如：Http 连接超时， 打开rtmp的超时  AVDictionary **options
     */
    int ret = avformat_open_input(&formatContext, sourceData, 0, &dictionary);

    av_dict_free(&dictionary);

    if (ret) {
        if (helper) {
            helper->onError(THREAD_CHILD, FFMPEG_CAN_NOT_OPEN_URL);
            // ffmpeg 根据返回码获得错误信息
            // char *error = av_err2str(ret);
        }
        return;
    }

    // TODO 第二步：查找媒体中的音视频流的信息
    ret = avformat_find_stream_info(formatContext, 0);

    if (ret < 0) {
        if (helper) {
            helper->onError(THREAD_CHILD, FFMPEG_CAN_NOT_FIND_STREAMS);
        }
        return;
    }

    // TODO 第三步: 根据流信息，流的个数，用循环来找，一般第0个代表视频流，第一个代表音频流
    for (int i = 0; i < formatContext->nb_streams; i++) {
        // TODO 第四步：获取媒体流（视频，音频）
        AVStream *stream = formatContext->streams[i];

        /**
         * TODO 第五步：从上面的流中 获取 编码解码的【参数】
         * 由于：后面的编码器 解码器 都需要参数（宽高 等等）
         */
        AVCodecParameters *parameters = stream->codecpar;

        // TODO 第六步：（根据上面的【参数】）获取编解码器
        AVCodec *codec = avcodec_find_decoder(parameters->codec_id);
        if (!codec) {
            if (helper) {
                helper->onError(THREAD_CHILD, FFMPEG_FIND_DECODER_FAIL);
            }
            return;
        }

        // TODO 第七步：编解码器 上下文
        AVCodecContext *avCodecContext = avcodec_alloc_context3(codec);

        if (!avCodecContext) {
            if (helper) {
                helper->onError(THREAD_CHILD, FFMPEG_ALLOC_CODEC_CONTEXT_FAIL);
            }
            return;
        }

        // TODO 第八步：使用parameter来填充avCodecContext
        ret = avcodec_parameters_to_context(avCodecContext, parameters);

        if (ret < 0) {
            if (helper) {
                helper->onError(THREAD_CHILD, FFMPEG_CODEC_CONTEXT_PARAMETERS_FAIL);
            }
            return;
        }

        // TODO 第九步：打开解码器
        ret = avcodec_open2(avCodecContext, codec, 0);

        if (ret) {
            if (helper) {
                helper->onError(THREAD_CHILD, FFMPEG_OPEN_DECODER_FAIL);
            }
            return;
        }

        // TODO 第十步：从编解码器参数中，获取流的类型 codec_type, 判断音频 视频
        if (parameters->codec_type == AVMediaType::AVMEDIA_TYPE_AUDIO) {
            audioChannel = new AudioChannel(i, avCodecContext);
        } else if (parameters->codec_type == AVMediaType::AVMEDIA_TYPE_VIDEO) {
            videoChannel = new VideoChannel(i, avCodecContext);
            videoChannel->setRenderCallback(renderCallback);
        }
    } // for end

    // TODO 第十一步: 如果流中 没有音频 也没有 视频 【健壮性校验】
    if (!audioChannel || !videoChannel) {
        if (helper) {
            helper->onError(THREAD_CHILD, FFMPEG_NOMEDIA);
        }
        return;
    }

    // TODO 第十二步：恭喜你，准备成功，我们的媒体文件 OK了，通知给上层
    if (helper) {
        helper->onPrepared(THREAD_CHILD);
    }
}

void JackPlayer::prepare() {
    // 当前是在MainActivity的onResume()中调用是在主线程调用
    // 而读音视频流或者直播流是耗时操作，需要放在子线程中去读

    // 创建子线程
    pthread_create(&pid_prepare, 0, task_prepare, this);
}

//===========================以下是start to play内容====================================
/**
 *  AVPacket 影视频的压缩包
 */
void JackPlayer::start_() {
    while (isPlaying) {
        // 分配AVPacket 内存
        AVPacket *pkt = av_packet_alloc();
        int ret = av_read_frame(formatContext, pkt);
        if (!ret) { // 获取到pkt就进来
            // 在该if内部区分音视频加入队列
            if (videoChannel && videoChannel->stream_index == pkt->stream_index) {
                // 将视频压缩包放入videoChannel中
                videoChannel->packets.insertToQueue(pkt);
            } else if(audioChannel && audioChannel->stream_index == pkt->stream_index) {
                // audioChannel->packets.insertToQueue(pkt);
            }
        } else if(ret == AVERROR_EOF) {
            // 文件读到末尾，并不代表播放完毕，需要处理播放问题
        } else {
            // 读到的frame有问题，结束当前循环
            break;
        }
    } // end while

    isPlaying = 0;
    videoChannel->stop();
    audioChannel->stop();
}

void *task_start(void *args) {
    JackPlayer *player = static_cast<JackPlayer *>(args);
    player->start_();
    return 0;
}

void JackPlayer::start() {
    isPlaying = 1;

    if (videoChannel) {
        videoChannel->start();
    }

    if (audioChannel) {
        audioChannel->start();
    }

    pthread_create(&pid_start, 0, task_start, this);
}

void JackPlayer::setRenderCallback(RenderCallback renderCallback) {
    this->renderCallback = renderCallback;
}
