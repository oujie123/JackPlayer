//
// Created by JackOu on 2021/7/23.
//

#include "AudioChannel.h"

AudioChannel::AudioChannel(int stream_index, AVCodecContext *avCodecContext, AVRational timeBase)
        : BaseChannel(stream_index, avCodecContext, timeBase) {
    // 定义缓冲区和缓冲区大小    out_buff/out_buff_size

    // 音频三要素
    // 1.采样率：44100/48000
    // 2.位声/采样格式：16bit/24bit    集成声卡一般16位，独立声卡有24位
    // 3.声道数：2，双声道

    // 音频压缩格式AAC是：32位，44100，双声道
    // 手机音频参数     ：16位，44100，双声道
    // 所以需要重采样

    out_channels = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO); // 双声道
    out_sample_size = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16); // 16bit
    out_sample_rate = 44100;

    out_buffer_size = static_cast<size_t>(out_channels * out_sample_size *
                                          out_sample_rate);  // 2 * 2 * 44100 = 176400
    out_buffer = static_cast<uint8_t *>(malloc(out_buffer_size));

    // 音频重采样
    swr_context = swr_alloc_set_opts(
            0,
            // 输出环节
            AV_CH_LAYOUT_STEREO,
            AV_SAMPLE_FMT_S16,
            out_sample_rate,

            // 输入环节
            avCodecContext->channel_layout,
            avCodecContext->sample_fmt,
            avCodecContext->sample_rate,
            0, 0
    );

    // 初始化重采样上下文
    swr_init(swr_context);
}

AudioChannel::~AudioChannel() {

}

void *task_audio_decode(void *args) {
    auto *channel = static_cast<AudioChannel *>(args);
    channel->audioDecode();
    return 0;
}

void *task_audio_play(void *args) {
    auto *channel = static_cast<AudioChannel *>(args);
    channel->audioPlay();
    return 0;
}

/**
 * 音频解码和播放
 */
void AudioChannel::start() {
    isPlaying = 1;

    // 队列开始工作了
    packets.setWork(1);
    frames.setWork(1);

    // 第一个线程： 音频：取出队列的压缩包 进行解码，解码后的原始包 再push队列中去
    pthread_create(&pid_audio_decode, 0, task_audio_decode, this);

    // 第二线线程：音频：从队列取出原始包，播放
    pthread_create(&pid_audio_play, 0, task_audio_play, this);
}

void AudioChannel::stop() {

}

void AudioChannel::audioDecode() {
    AVPacket *pkt = 0;
    while (isPlaying) {
        // TODO 优化 3.1 : 控制音频队列中的音频帧数量
        if (frames.size() > SOURCE_QUEUE_THRESHOLD) {
            av_usleep(PRODUCER_WAITING_TIME);
            continue;
        }

        int ret = packets.getQueueAndDel(pkt);
        if (!isPlaying) {
            break;
        }
        if (!ret) {
            continue;
        }

        // 发送packet
        ret = avcodec_send_packet(avCodecContext, pkt);
        // releaseAVPacket(&pkt);
        if (ret) {
            break;
        }

        // 取出音频帧
        AVFrame *frame = av_frame_alloc();
        ret = avcodec_receive_frame(avCodecContext, frame);
        if (ret == AVERROR(EAGAIN)) {
            continue;
        } else if (ret != 0) {
            // TODO 优化3.2 :获取帧出错，应该全部释放frame和packet
            if (frame) {
                releaseAVFrame(&frame);
                frame = nullptr;
            }
            break;
        }

        // 将pcm数据放入队列
        frames.insertToQueue(frame);

        // TODO 优化 4.1: 释放时机应该是当frame队列之后，然后还要释放所指堆空间的内存
        // 先释放引用，再释放堆
        av_packet_unref(pkt);
        releaseAVPacket(&pkt);
    } // end while
    // TODO 优化 4.2: 流程出错跳出循环内存释放点
    av_packet_unref(pkt);
    releaseAVPacket(&pkt);
}

// TODO 4.3 回调函数，设置播放的资源到队列中
/**
 * 源码中回调是这么写的
 *
typedef void (SLAPIENTRY *slAndroidSimpleBufferQueueCallback)(
	SLAndroidSimpleBufferQueueItf caller,
	void *pContext
);
 *
 * @param bq
 * @param context
 */
void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
    AudioChannel *channel = static_cast<AudioChannel *>(context);

    int pcm_size = channel->getPcm();

    // 添加数据到缓冲区去,只需要第一次插入数据，后面会主动调用回调函数来取数据
    (*bq)->Enqueue(
            bq, // 不是C++实现，没有this，所以需要传自己进去
            channel->out_buffer, //pcm 数据
            static_cast<SLuint32>(pcm_size)); // pcm数据的大小,大小需要重采样之后的大小
}

/**
 * 音频重采样
 * 给out_buffer赋值
 *
 * @return 重采样后的buffer大小
 */
int AudioChannel::getPcm() {
    int pcm_data_size = 0;

    //从frame中获取pcm数据
    AVFrame *frame = nullptr;
    while (isPlaying) {
        int ret = frames.getQueueAndDel(frame);
        if (!isPlaying) {
            break;
        }
        if (!ret) {
            continue;
        }

        // TODO 开始重采样
        // 获取单通道样本数
        int dsr_nb_samples = static_cast<int>(av_rescale_rnd(
                swr_get_delay(swr_context, frame->sample_rate) + frame->sample_rate,
                out_sample_rate, // 输出采样率
                frame->sample_rate,//输入采样率
                AV_ROUND_UP));

        // samples_per_channel每个通道重采样后的样本数，即重采样后的采样数
        int samples_per_channel = swr_convert(swr_context,
                // 输出区域
                                              &out_buffer, // 输出结果的buffer
                                              dsr_nb_samples, // 单通道样本数,数据真正大小还需要与位声和通道数相乘

                //输入区域
                                              (const uint8_t **) (frame->data),
                                              frame->nb_samples);
        // 计算buffer中的数据大小
        pcm_data_size = samples_per_channel * out_sample_size * out_channels;

        // 每一个pcm音频包大小
        // 单通道样本数为1024；包大小为  1024 * 2 * 2 = 4096字节
        // 4096是一帧音频包的大小，44100是每秒钟采集的采样点数

        // 1024 是单通道采集的样本数
        // 一秒多少帧：44100 / 1024 = 43.066 帧 （每秒43帧）
        // 一秒钟音频大小： 44100 * 2 * 2 = 176400 byte
        // 一帧音频大小：1024 * 2 * 2 = 4096 byte

        // TODO 音视频同步，以音频为基准
        /**
        typedef struct AVRational{
            int num; ///< Numerator   分子
            int den; ///< Denominator 分母
        } AVRational;
         */

        // audio_time：0.000000  0.0231123
        audio_time = frame->best_effort_timestamp * av_q2d(time_base);  // 音频播放的时间戳， 单位：时间基 （time base）

        if (this->jniCallbackHelper) {
            jniCallbackHelper->onProgress(THREAD_CHILD, static_cast<int>(audio_time));
        }

        break;
    } // end while

    // TODO 优化6.1: Frame 释放
    av_frame_unref(frame);
    releaseAVFrame(&frame);

    return pcm_data_size;
}

void AudioChannel::audioPlay() {
    SLresult result;

    // TODO 1.创建引擎并且获取引擎接口
    // TODO 1.1 创建引擎对象 SLObjectItf engineObject
    result = slCreateEngine(&engineObject, 0, 0, 0, 0, 0);
    if (SL_RESULT_SUCCESS != result) {
        LOGE("create engine error.");
        return;
    }

    // TODO 1.2 初始化引擎
    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE); //SL_BOOLEAN_FALSE: 延时等待创建成功
    if (SL_RESULT_SUCCESS != result) {
        LOGE("realize engine error.");
        return;
    }

    // TODO 1.3 获取引擎接口
    result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineItf);  // 获取引擎接口的flag
    if (SL_RESULT_SUCCESS != result) {
        LOGE("get engine interface error.");
        return;
    }

    if (engineItf) {
        LOGD("create engine interface succeed.");
    } else {
        LOGE("get engine interface error.");
        return;
    }

    // TODO 2.设置混音器
    // TODO 2.1 创建混音器
    result = (*engineItf)->CreateOutputMix(engineItf, &outputMixObject, 0, 0, 0);
    if (SL_RESULT_SUCCESS != result) {
        LOGE("create mix failed.");
        return;
    }

    // TODO 2.2 初始化混音器
    result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        LOGE("init mix error.");
        return;
    }

    // 如果不启用混响，可以不获取混音器接口
    // TODO 2.3 获取混响器接口
    result = (*outputMixObject)->GetInterface(outputMixObject, SL_IID_ENVIRONMENTALREVERB,
                                              &outputMixEnvironmentalReverb);
    if (SL_RESULT_SUCCESS != result) {
        LOGE("get mix environment error.");
        return;
    }

    // 设置默认混响器
    //SL_I3DL2_ENVIRONMENT_PRESET_ROOM: 室内
    //SL_I3DL2_ENVIRONMENT_PRESET_AUDITORIUM: 礼堂
    const SLEnvironmentalReverbSettings settings = SL_I3DL2_ENVIRONMENT_PRESET_DEFAULT;
    result = (*outputMixEnvironmentalReverb)->SetEnvironmentalReverbProperties(
            outputMixEnvironmentalReverb, &settings);
    if (SL_RESULT_SUCCESS != result) {
        LOGE("set environment effect error.");
        return;
    }
    LOGI("set mix success.");

    // TODO 3.创建播放器
    // TODO 3.1 配置音频格式和缓存队列
    // 创建buffer缓存队列，   10是大小
    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 10};

    // pcm数据格式
    // SL_DATAFORMAT_PCM:数据格式为pcm格式
    // 2.双声道
    // SL_SAMPLINGRATE_44_1:采样率为44100
    // SL_PCMSAMPLEFORMAT_FIXED_16:采样格式为16bit
    // SL_PCMSAMPLEFORMAT_FIXED_16:数据格式为16bit
    // SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT : 左右双声道
    // SL_BYTEORDER_LITTLEENDIAN:小端模式
    SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM, SL_SAMPLINGRATE_44_1,
                                   SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
                                   SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
                                   SL_BYTEORDER_LITTLEENDIAN};

    // 最终配置
    SLDataSource audio_src = {&loc_bufq, &format_pcm};

    // TODO 3.2 配置音轨
    // 设置混音器
    SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject}; // 设置混音器类型
    SLDataSink audio_snk = {&loc_outmix, NULL};
    // 操作队列接口  开放队列标志位
    const SLInterfaceID ids[1] = {SL_IID_BUFFERQUEUE};
    const SLboolean req[1] = {SL_BOOLEAN_TRUE};

    // TODO 3.3 创建播放器 SLObjectItf bqPlayerObject
    result = (*engineItf)->CreateAudioPlayer(
            engineItf, // 引擎接口
            &bqPlayerObject, // 播放器对象
            &audio_src,  //音频配置
            &audio_snk,  //混音配置
            // 打开队列,不打开无法播放
            1, //开放参数个数
            ids, // 开放buff队列
            req); // 将buff队列开放出去
    if (SL_RESULT_SUCCESS != result) {
        LOGE("create player error.");
        return;
    }

    // TODO 3.4 初始化播放器
    result = (*bqPlayerObject)->Realize(bqPlayerObject, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        LOGE("init player error.");
        return;
    }
    LOGD("init player succeed.");

    // TODO 3.5 获取播放器接口
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_PLAY, &bqPlayerItf);
    if (SL_RESULT_SUCCESS != result) {
        LOGE("get player interface error.");
        return;
    }
    LOGD("get player interface succeed.");

    // TODO 4.设置播放回调
    // TODO 4.1 设置播放队列接口 SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_BUFFERQUEUE,
                                             &bqPlayerBufferQueue);
    if (SL_RESULT_SUCCESS != result) {
        LOGE("get queue interface error.");
        return;
    }

    // TODO 4.2 设置回调 void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context);
    result = (*bqPlayerBufferQueue)->RegisterCallback(
            bqPlayerBufferQueue,
            bqPlayerCallback,
            this); // this 是传给回调函数的参数
    if (SL_RESULT_SUCCESS != result) {
        LOGE("register callback error.");
        return;
    }

    // TODO 5. 设置播放器状态为播放状态
    (*bqPlayerItf)->SetPlayState(bqPlayerItf, SL_PLAYSTATE_PLAYING);

    // TODO 6.手动激活调用回调函数，才开始播放
    // 在回调函数中添加需要播放的数据，一添加就开始播放了。
    bqPlayerCallback(bqPlayerBufferQueue, this);

}
