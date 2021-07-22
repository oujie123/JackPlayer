#include <jni.h>
#include <string>
#include "JackPlayer.h"

JackPlayer *player = 0;
JavaVM *vm = 0;

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

extern "C"
JNIEXPORT void JNICALL
Java_com_gxa_jackplayer_JackPlayer_prepareNative(JNIEnv *env, jobject instance,
                                                 jstring sourceData_) {
    const char *sourceData = env->GetStringUTFChars(sourceData_, 0);
    auto *helper = new JNICallbackHelper(vm, env, instance);
    player = new JackPlayer(sourceData, helper);
    player->prepare();
    env->ReleaseStringUTFChars(sourceData_, sourceData);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_gxa_jackplayer_JackPlayer_startNative(JNIEnv *env, jobject instance) {

    // TODO

}

extern "C"
JNIEXPORT void JNICALL
Java_com_gxa_jackplayer_JackPlayer_stopNative(JNIEnv *env, jobject instance) {

    // TODO

}

extern "C"
JNIEXPORT void JNICALL
Java_com_gxa_jackplayer_JackPlayer_releaseNative(JNIEnv *env, jobject instance) {

    // TODO

}