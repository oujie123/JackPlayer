package com.gxa.jackplayer;

import android.text.TextUtils;

/**
 * @Author: Jack Ou
 * @CreateDate: 2021/7/21 23:13
 * @UpdateUser: 更新者
 * @UpdateDate: 2021/7/21 23:13
 * @UpdateRemark: 更新说明
 */
public class JackPlayer {

    private OnPreparedListener mOnPreparedListener;
    private String mSourceData;
    private static JackPlayer mInstance;

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
    public void prepare(){
        if (!TextUtils.isEmpty(mSourceData)) {
            prepareNative(mSourceData);
        } else {
            throw new IllegalArgumentException("please set the path of source");
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
     *  native notify the player status
     */
    public void onPrepared(){
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

    // native function
    private native void prepareNative(String sourceData);
    private native void startNative();
    private native void stopNative();
    private native void releaseNative();
}
