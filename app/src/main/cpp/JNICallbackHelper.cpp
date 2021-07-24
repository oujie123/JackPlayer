//
// Created by JackOu on 2021/7/22.
//

#include "JNICallbackHelper.h"

JNICallbackHelper::JNICallbackHelper(JavaVM *vm, JNIEnv *env, jobject obj) {
    this->vm = vm;
    this->env = env;
    //this->job = obj; //jobject是不能跨线程和跨方法使用的，由于我们需要在onPrepared中使用obj来访问到method方法。所以需要升级为全局引用
    this->job = env->NewGlobalRef(obj);

    jclass clazz = env->GetObjectClass(job);
    jmd_prepared = env->GetMethodID(clazz, "onPrepared", "()V");
    jmd_on_error = env->GetMethodID(clazz, "onError", "(I)V");
}

JNICallbackHelper::~JNICallbackHelper() {
    vm = 0;
    env->DeleteGlobalRef(job);
    job = 0;
    env = 0;
}

void JNICallbackHelper::onPrepared(int thread_mode) {
    if (thread_mode == THREAD_MAIN) {
        // 主线程
        env->CallVoidMethod(job, jmd_prepared);
    } else if (thread_mode == THREAD_CHILD) {
        // 子线程
        // env不能跨线程
        JNIEnv *env_child;
        vm->AttachCurrentThread(&env_child, 0);
        env_child->CallVoidMethod(job, jmd_prepared);
        vm->DetachCurrentThread();
    }
}

void JNICallbackHelper::onError(int thread_mode, int error_code) {
    if (thread_mode == THREAD_MAIN) {
        // 主线程
        env->CallVoidMethod(job, jmd_on_error);
    } else if (thread_mode == THREAD_CHILD) {
        // 子线程
        // env不能跨线程
        JNIEnv *env_child;
        vm->AttachCurrentThread(&env_child, 0);
        env_child->CallVoidMethod(job, jmd_on_error, error_code);
        vm->DetachCurrentThread();
    }
}
