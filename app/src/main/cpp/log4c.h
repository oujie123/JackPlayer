//
// Created by JackOu on 2021/7/24.
//

#ifndef JACKPLAYER_LOG4C_H
#define JACKPLAYER_LOG4C_H

#include <android/log.h>
#define TAG "JackOu"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG,  __VA_ARGS__);
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG,  __VA_ARGS__);
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG,  __VA_ARGS__);

#endif //JACKPLAYER_LOG4C_H
