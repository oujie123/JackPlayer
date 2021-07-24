package com.gxa.jackplayer;

import android.text.TextUtils;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

/**
 * @Author: Jack Ou
 * @CreateDate: 2021/7/21 23:13
 * @UpdateUser: 更新者
 * @UpdateDate: 2021/7/21 23:13
 * @UpdateRemark: 更新说明
 */
public class JackPlayer implements SurfaceHolder.Callback {

    private OnPreparedListener mOnPreparedListener;
    private OnErrorListener onErrorListener;
    private String mSourceData;
    private SurfaceHolder mSurfaceHolder;
    private static JackPlayer mInstance;
    // 错误代码 ================ 如下
    // 打不开视频
    // #define FFMPEG_CAN_NOT_OPEN_URL 1
    private static final int FFMPEG_CAN_NOT_OPEN_URL = 1;

    // 找不到流媒体
    // #define FFMPEG_CAN_NOT_FIND_STREAMS 2
    private static final int FFMPEG_CAN_NOT_FIND_STREAMS = 2;

    // 找不到解码器
    // #define FFMPEG_FIND_DECODER_FAIL 3
    private static final int FFMPEG_FIND_DECODER_FAIL = 3;

    // 无法根据解码器创建上下文
    // #define FFMPEG_ALLOC_CODEC_CONTEXT_FAIL 4
    private static final int FFMPEG_ALLOC_CODEC_CONTEXT_FAIL = 4;

    //  根据流信息 配置上下文参数失败
    // #define FFMPEG_CODEC_CONTEXT_PARAMETERS_FAIL 6
    private static final int FFMPEG_CODEC_CONTEXT_PARAMETERS_FAIL = 6;

    // 打开解码器失败
    // #define FFMPEG_OPEN_DECODER_FAIL 7
    private static final int FFMPEG_OPEN_DECODER_FAIL = 7;

    // 没有音视频
    // #define FFMPEG_NOMEDIA 8
    private static final int FFMPEG_NOMEDIA = 8;

    static {
        System.loadLibrary("native-lib");
    }

    public static JackPlayer getInstance() {
        if (mInstance == null) {
            synchronized (JackPlayer.class) {
                if (mInstance == null) {
                    mInstance = new JackPlayer();
                }
            }
        }
        return mInstance;
    }

    /**
     * prepare the player
     */
    public void prepare() {
        if (!TextUtils.isEmpty(mSourceData)) {
            prepareNative(mSourceData);
        } else {
            throw new IllegalArgumentException("please set the path of source");
        }
    }

    public void onError(int code) {
        if (onErrorListener != null) {
            String msg = null;
            switch (code) {
                case FFMPEG_CAN_NOT_OPEN_URL:
                    msg = "打不开视频";
                    break;
                case FFMPEG_CAN_NOT_FIND_STREAMS:
                    msg = "找不到流媒体";
                    break;
                case FFMPEG_FIND_DECODER_FAIL:
                    msg = "找不到解码器";
                    break;
                case FFMPEG_ALLOC_CODEC_CONTEXT_FAIL:
                    msg = "无法根据解码器创建上下文";
                    break;
                case FFMPEG_CODEC_CONTEXT_PARAMETERS_FAIL:
                    msg = "根据流信息 配置上下文参数失败";
                    break;
                case FFMPEG_OPEN_DECODER_FAIL:
                    msg = "打开解码器失败";
                    break;
                case FFMPEG_NOMEDIA:
                    msg = "没有音视频";
                    break;
            }
            onErrorListener.onError(msg);
        }
    }

    /**
     * start to play
     */
    public void start() {
        startNative();
    }

    /**
     * stop
     */
    public void stop() {
        stopNative();
    }

    /**
     * release
     */
    public void release() {
        releaseNative();
    }

    /**
     * the path of source.
     *
     * @param sourcePath path
     */
    public void setSourceData(String sourcePath) {
        this.mSourceData = sourcePath;
    }

    /**
     * view set listener to get the player status.
     *
     * @param listener listener
     */
    public void setOnPreparedListener(OnPreparedListener listener) {
        this.mOnPreparedListener = listener;
    }

    /**
     * native notify the player status
     */
    public void onPrepared() {
        if (mOnPreparedListener != null) {
            mOnPreparedListener.onPrepared();
        }
    }

    /**
     * get notification from native that player is prepared
     */
    public interface OnPreparedListener {
        void onPrepared();
    }

    interface OnErrorListener {
        void onError(String errorCode);
    }

    public void setOnErrorListener(OnErrorListener onErrorListener) {
        this.onErrorListener = onErrorListener;
    }

    //===================Surface View回调==============================

    public void setSurfaceView(SurfaceView surfaceView){
        if (this.mSurfaceHolder != null) {
            mSurfaceHolder.removeCallback(this);
        }
        mSurfaceHolder = surfaceView.getHolder();
        mSurfaceHolder.addCallback(this);
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {

    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        setSurfaceNative(holder.getSurface());
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {

    }

    // native function
    private native void prepareNative(String sourceData);

    private native void startNative();

    private native void stopNative();

    private native void releaseNative();

    private native void setSurfaceNative(Surface surface);
}
