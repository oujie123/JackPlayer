//
// Created by JackOu on 2021/7/21.
//

#ifndef JACKPLAYER_JNICALLBAKCHELPER_H
#define JACKPLAYER_JNICALLBAKCHELPER_H

#include <jni.h>
#include "utils.h"

class JNICallbackHelper {

private:
    JavaVM *vm = 0;
    JNIEnv *env = 0;
    jobject job;
    jmethodID jmd_prepared;

public:
    JNICallbackHelper(JavaVM *vm,JNIEnv *env,jobject obj);

    virtual ~JNICallbackHelper();

    void onPrepared(int thread_mode);
};

#endif //JACKPLAYER_JNICALLBAKCHELPER_H
