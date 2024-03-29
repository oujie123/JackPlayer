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

    pthread_mutex_init(&seek_mutex, nullptr);
}

JackPlayer::~JackPlayer() {
    if (sourceData) {
        delete sourceData;
        sourceData = nullptr;
    }
    if (helper) {
        delete helper;
        helper = nullptr;
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
    return nullptr;
}

// 此函数在子线程
void JackPlayer::prepare_() {

    //FFmpeg源码是纯C的，他不像C++、Java ， 上下文的出现是为了贯彻环境，就相当于Java的this能够操作成员
    // 只是分配了一个对象，内容avformat_open_input来填写
    formatContext = avformat_alloc_context();

    // TODO 第一步：打开媒体地址（文件路径， 直播地址rtmp）
    AVDictionary *dictionary = nullptr;
    av_dict_set(&dictionary, "timeout", "5000000", 0);

    /**
     * 1，AVFormatContext *
     * 2，路径
     * 3，AVInputFormat *fmt  Mac、Windows 摄像头、麦克风， 目前安卓用不到
     * 4，各种设置：例如：Http 连接超时， 打开rtmp的超时  AVDictionary **options
     */
    int ret = avformat_open_input(&formatContext, sourceData, nullptr, &dictionary);

    av_dict_free(&dictionary);

    if (ret) {
        if (helper) {
            helper->onError(THREAD_CHILD, FFMPEG_CAN_NOT_OPEN_URL);
            // ffmpeg 根据返回码获得错误信息
            // char *error = av_err2str(ret);
        }
        avformat_close_input(&formatContext);
        avformat_free_context(formatContext);
        return;
    }

    // TODO 第二步：查找媒体中的音视频流的信息
    ret = avformat_find_stream_info(formatContext, nullptr);

    if (ret < 0) {
        if (helper) {
            helper->onError(THREAD_CHILD, FFMPEG_CAN_NOT_FIND_STREAMS);
        }
        avformat_close_input(&formatContext);
        avformat_free_context(formatContext);
        return;
    }

    // 只有在avformat_find_stream_info之后拿事件才是可靠的，如果是flv格式文件中在avformat_find_stream_info之前，拿不到事件
    duration = static_cast<int>(formatContext->duration / AV_TIME_BASE);

    AVCodecContext *avCodecContext = 0;

    // TODO 第三步: 根据流信息，流的个数，用循环来找，一般第0个代表视频流，第一个代表音频流
    for (int stream_index = 0; stream_index < formatContext->nb_streams; stream_index++) {
        // TODO 第四步：获取媒体流（视频，音频）
        AVStream *stream = formatContext->streams[stream_index];

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
            avformat_close_input(&formatContext);
            avformat_free_context(formatContext);
            return;
        }

        // TODO 第七步：编解码器 上下文
        avCodecContext = avcodec_alloc_context3(codec);

        if (!avCodecContext) {
            if (helper) {
                helper->onError(THREAD_CHILD, FFMPEG_ALLOC_CODEC_CONTEXT_FAIL);
            }
            avcodec_free_context(&avCodecContext);// 释放上下文，codec会自动释放
            avformat_close_input(&formatContext);
            avformat_free_context(formatContext);
            return;
        }

        // TODO 第八步：使用parameter来填充avCodecContext
        ret = avcodec_parameters_to_context(avCodecContext, parameters);

        if (ret < 0) {
            if (helper) {
                helper->onError(THREAD_CHILD, FFMPEG_CODEC_CONTEXT_PARAMETERS_FAIL);
            }
            avcodec_free_context(&avCodecContext);
            avformat_close_input(&formatContext);
            avformat_free_context(formatContext);
            return;
        }

        // TODO 第九步：打开解码器
        ret = avcodec_open2(avCodecContext, codec, nullptr);
        if (ret) {
            if (helper) {
                helper->onError(THREAD_CHILD, FFMPEG_OPEN_DECODER_FAIL);
            }
            avcodec_free_context(&avCodecContext);
            avformat_close_input(&formatContext);
            avformat_free_context(formatContext);
            return;
        }

        // 获取时间基传给父类
        AVRational time_base = stream->time_base;

        // TODO 第十步：从编解码器参数中，获取流的类型 codec_type, 判断音频 视频
        if (parameters->codec_type == AVMediaType::AVMEDIA_TYPE_AUDIO) {
            audioChannel = new AudioChannel(stream_index, avCodecContext, time_base);

            // 回调方法给进度条显示
            if (this->duration != 0) {   // 非直播才有意义
                audioChannel->setJNICallbackHelper(helper);
            }
        } else if (parameters->codec_type == AVMediaType::AVMEDIA_TYPE_VIDEO) {
            // 虽然是视频流，但是是封面格式，不用播放
            if (stream->disposition & AV_DISPOSITION_ATTACHED_PIC) {
                continue;
            }

            // 获取fps
            AVRational fps_rational = stream->avg_frame_rate;
            int fps = static_cast<int>(av_q2d(fps_rational));

            videoChannel = new VideoChannel(stream_index, avCodecContext, time_base, fps);
            videoChannel->setRenderCallback(renderCallback);

            if (this->duration != 0) {   // 非直播才有意义
                videoChannel->setJNICallbackHelper(helper);
            }
        }
    } // for end

    // TODO 第十一步: 如果流中 没有音频 也没有 视频 【健壮性校验】
    if (!audioChannel || !videoChannel) {
        if (helper) {
            helper->onError(THREAD_CHILD, FFMPEG_NOMEDIA);
        }
        if (avCodecContext) {
            avcodec_free_context(&avCodecContext);
        }
        avformat_close_input(&formatContext);
        avformat_free_context(formatContext);
        return;
    }

    // TODO 第十二步：恭喜你，准备成功，我们的媒体文件 OK了，通知给上层
    // 用户关闭之后界面之后不能回调到用户。
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
 *  TODO 控制packet队列的数据大小，防止生产太快，消费太慢
 *
 *  AVPacket 影视频的压缩包
 */
void JackPlayer::start_() {
    AVPacket *pkt = nullptr;
    while (isPlaying) {
        // TODO 优化1.1 : 控制视频包个数
        if (videoChannel && videoChannel->packets.size() > SOURCE_QUEUE_THRESHOLD) {
            av_usleep(PRODUCER_WAITING_TIME); // 单位微妙
            continue;
        }

        // TODO 优化1.2 : 控制视频包个数
        if (audioChannel && audioChannel->packets.size() > SOURCE_QUEUE_THRESHOLD) {
            av_usleep(PRODUCER_WAITING_TIME); // 单位微妙
            continue;
        }

        // 分配AVPacket 内存
        pkt = av_packet_alloc();
        int ret = av_read_frame(formatContext, pkt);
        if (!ret) { // 获取到pkt就进来
            // 在该if内部区分音视频加入队列
            if (videoChannel && videoChannel->stream_index == pkt->stream_index) {
                // 将视频压缩包放入videoChannel中
                videoChannel->packets.insertToQueue(pkt);
            } else if (audioChannel && audioChannel->stream_index == pkt->stream_index) {
                audioChannel->packets.insertToQueue(pkt);
            }
        } else if (ret == AVERROR_EOF) {
            // 文件读到末尾，并不代表播放完毕，需要处理播放问题
            // TODO 优化1.3 : 文件读取完毕退出循环
            if (audioChannel->packets.empty() && videoChannel->packets.empty()) {
                break;
            }
        } else {
            // 读到的frame有问题，结束当前循环
            break;
        }
    } // end while

    isPlaying = false;
    videoChannel->stop();
    audioChannel->stop();
}

void *task_start(void *args) {
    JackPlayer *player = static_cast<JackPlayer *>(args);
    player->start_();
    return nullptr;
}

void JackPlayer::start() {
    isPlaying = true;

    // 视频解码，播放
    if (videoChannel) {
        videoChannel->setAudioChannel(audioChannel);
        videoChannel->start();
    }

    // 音频解码，播放
    if (audioChannel) {
        audioChannel->start();
    }

    pthread_create(&pid_start, nullptr, task_start, this);
}

void JackPlayer::setRenderCallback(RenderCallback renderCallback) {
    this->renderCallback = renderCallback;
}

int JackPlayer::getDuration() {
    return duration;
}

void JackPlayer::seek(int time) {

    if (time < 0 || time > duration) {
        return;
    }

    if (!audioChannel || !videoChannel) {
        return;
    }

    if (!formatContext) {
        return;
    }

    // 需要考虑formatContext的多线程安全性。
    pthread_mutex_lock(&seek_mutex);

    /**
     *  -1 代表 default，自动选择音频还是视频做基准
     *
     *  拖动出现花屏大概率会是这个选择成了AVSEEK_FLAG_ANY
     *  #define AVSEEK_FLAG_BACKWARD 1 ///< seek backward  向后参考，可以择优附件的关键帧，如果找不到也会花屏  ，一般采用这个
     *  #define AVSEEK_FLAG_BYTE     2 ///< seeking based on position in bytes
     *  #define AVSEEK_FLAG_ANY      4 ///< seek to any frame, even non-keyframes   拖到那里就显示那里，可能拖到的地方不是关键帧
     *  #define AVSEEK_FLAG_FRAME    8 ///< seeking based on frame number  找关键帧，时间非常不准确，可能调不到关键帧，一般配合AVSEEK_FLAG_BACKWARD用
     */
    // av_seek_frame(formatContext, -1, time * AV_TIME_BASE, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_FRAME);
    int ret = av_seek_frame(formatContext, -1, time * AV_TIME_BASE, AVSEEK_FLAG_BACKWARD);
    if (ret < 0) {
        return;
    }
    // seek的时候需要暂停队列
    // audio channel : frames packets
    // video channel : frames packets
    if (audioChannel) {
        audioChannel->packets.setWork(0); // 让队列不工作
        audioChannel->frames.setWork(0);
        audioChannel->packets.clear();
        audioChannel->frames.clear();
        audioChannel->packets.setWork(1);
        audioChannel->frames.setWork(1);
    }

    if (videoChannel) {
        videoChannel->packets.setWork(0);
        videoChannel->frames.setWork(0);
        videoChannel->packets.clear();
        videoChannel->frames.clear();
        videoChannel->packets.setWork(1);
        videoChannel->frames.setWork(1);
    }
    pthread_mutex_unlock(&seek_mutex);
}

void *task_stop(void *args) {
    JackPlayer *player = static_cast<JackPlayer *>(args);
    player->stop_(player);
    return nullptr;
}

void JackPlayer::stop() {
    // 拦截所有回调
    helper = nullptr;
    if (audioChannel) {
        audioChannel->jniCallbackHelper = nullptr;
    }
    if (videoChannel) {
        videoChannel->jniCallbackHelper = nullptr;
    }

    // prepare_ start_释放工作，不能暴力释放
    // 需要稳稳释放以上两个线程，再释放player的资源
    // 如果等待线程执行完毕，可能导致ANR
    // 需要开启stop线程来关闭所有资源，在释放自己。
    pthread_create(&pid_stop, nullptr,task_stop,this);
}

void JackPlayer::stop_(JackPlayer *player) {
    isPlaying = false;
    pthread_join(pid_prepare, nullptr);  // 加入到stop线程
    pthread_join(pid_start, nullptr);  // 加入到stop线程
    // 到达此处pid_prepare和pid_start已经完成工作，可以安心释放自己了
    if (formatContext) {
        avformat_close_input(&formatContext);
        avformat_free_context(formatContext);
        formatContext = nullptr;
    }

    DELETE(audioChannel);
    DELETE(videoChannel);
    DELETE(player);
}
