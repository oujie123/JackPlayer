#include <jni.h>
#include <string>
#include "JackPlayer.h"
#include "log4c.h"
#include <android/native_window_jni.h> // ANativeWindow 用来渲染画面的 == Surface对象

JackPlayer *player = nullptr;
JavaVM *vm = nullptr;
ANativeWindow *window = nullptr;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; // 静态初始化锁

/**
 * 当加载库的时候拿到JavaVM
 *
 * @param vm
 * @param args
 * @return
 */
jint JNI_OnLoad(JavaVM *vm, void *args) {
    ::vm = vm;
    return JNI_VERSION_1_6;
}

/**
 * 自定义函数指针
 */
void renderFrame(uint8_t *src_data, int w, int h, int src_size) {
    // 该函数指针最终被调到VideoChannel中，然后回调得到以上数据，供surfaceview显示
    // setSurfaceNative界面发生改变就会setSurface，创建window;
    pthread_mutex_lock(&mutex);

    if (!window) { //如果window是空的
        pthread_mutex_unlock(&mutex); //防止死锁
        return;
    }

    // 设置窗口大小和各个属性
    ANativeWindow_setBuffersGeometry(window, w, h, WINDOW_FORMAT_RGBA_8888);

    // 他自己有个缓冲区 buffer
    ANativeWindow_Buffer window_buffer;

    // 在真正渲染之前需要锁定window的下一帧,返回0锁定成功
    if (ANativeWindow_lock(window, &window_buffer, 0)) {
        ANativeWindow_release(window);   // 锁定失败就释放window
        window = 0;

        pthread_mutex_unlock(&mutex);  // 防止死锁
        return;
    }

    // TODO 锁定成功就开始渲染
    // 一行一行填充buffer就可以自动显示
    uint8_t *dst_data = static_cast<uint8_t *>(window_buffer.bits);
    size_t dst_linesize = static_cast<size_t>(window_buffer.stride * 4);  // 乘以4的原因是stride是int32_t类型的像素点个数，而memcpy长度是以字节算，RGBA占四个字节。

    for (int i = 0; i < window_buffer.height; i++) {
        // 视频分辨率：426 * 240
        // 视频分辨率：宽 426
        // 426 * 4(rgba8888) = 1704
        // memcpy(dst_data + i * 1704, src_data + i * 1704, 1704); // 花屏
        // 花屏原因：1704 无法 64字节对齐，所以花屏

        // ANativeWindow_Buffer 64字节对齐的算法，  1704无法以64位字节对齐
        // memcpy(dst_data + i * 1792, src_data + i * 1704, 1792); // OK的
        // memcpy(dst_data + i * 1793, src_data + i * 1704, 1793); // 部分花屏，无法64字节对齐
        // memcpy(dst_data + i * 1728, src_data + i * 1704, 1728); // 花屏

        // ANativeWindow_Buffer 64字节对齐的算法  1728
        // 占位 占位 占位 占位 占位 占位 占位 占位
        // 数据 数据 数据 数据 数据 数据 数据 空值

        // ANativeWindow_Buffer 64字节对齐的算法  1792  空间换时间
        // 占位 占位 占位 占位 占位 占位 占位 占位 占位
        // 数据 数据 数据 数据 数据 数据 数据 空值 空值

        // FFmpeg为什么认为  1704 没有问题 ？
        // FFmpeg是默认采用8字节对齐的，他就认为没有问题， 但是ANativeWindow_Buffer他是64字节对齐的，就有问题

        // 通用的
        memcpy(dst_data + i * dst_linesize, src_data + i * src_size, dst_linesize); // OK的
    }

    // 数据刷新
    ANativeWindow_unlockAndPost(window); // 解锁后 并且刷新 window_buffer的数据显示画面

    pthread_mutex_unlock(&mutex);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_gxa_jackplayer_JackPlayer_prepareNative(JNIEnv *env, jobject instance,
                                                 jstring sourceData_) {
    const char *sourceData = env->GetStringUTFChars(sourceData_, 0);
    auto *helper = new JNICallbackHelper(vm, env, instance);
    player = new JackPlayer(sourceData, helper);
    player->setRenderCallback(renderFrame);
    player->prepare();
    env->ReleaseStringUTFChars(sourceData_, sourceData);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_gxa_jackplayer_JackPlayer_startNative(JNIEnv *env, jobject instance) {
    if (player) {
        player->start();
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_gxa_jackplayer_JackPlayer_stopNative(JNIEnv *env, jobject instance) {
    if (player) {
        player->stop();
    }

}

extern "C"
JNIEXPORT void JNICALL
Java_com_gxa_jackplayer_JackPlayer_releaseNative(JNIEnv *env, jobject instance) {
    pthread_mutex_lock(&mutex);
    if (window) {
        ANativeWindow_release(window);
        window = 0;
    }
    pthread_mutex_unlock(&mutex);

    DELETE(player);
    DELETE(vm);
    DELETE(window);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_gxa_jackplayer_JackPlayer_setSurfaceNative(JNIEnv *env, jobject instance,
                                                    jobject surface) {
    pthread_mutex_lock(&mutex);

    if (window) {
        ANativeWindow_release(window);
        window = 0;
    }

    // 创建新window
    window = ANativeWindow_fromSurface(env, surface);

    pthread_mutex_unlock(&mutex);
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_gxa_jackplayer_JackPlayer_getDurationNative(JNIEnv *env, jobject instance) {
    if (player) {
        return player->getDuration();
    }
    return 0;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_gxa_jackplayer_JackPlayer_seekNative(JNIEnv *env, jobject instance, jint time) {

    int current_time = time;
    if (player) {
        player->seek(current_time);
    }
}