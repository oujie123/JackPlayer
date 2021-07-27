package com.gxa.jackplayer;

import androidx.appcompat.app.AppCompatActivity;

import android.annotation.SuppressLint;
import android.graphics.Color;
import android.os.Bundle;
import android.os.Environment;
import android.view.SurfaceView;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;

public class MainActivity extends AppCompatActivity implements SeekBar.OnSeekBarChangeListener {

    private JackPlayer mJackPlayer;
    private TextView mTvState;
    private SurfaceView mSurfaceView;
    private TextView mTvTime;
    private SeekBar mSeekBar;
    private int duration;
    private boolean isTouch = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON,
                WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        setContentView(R.layout.activity_main);
        mTvState = findViewById(R.id.tv_status);
        mSurfaceView = findViewById(R.id.surfaceView);
        mTvTime = findViewById(R.id.tv_time);
        mSeekBar = findViewById(R.id.seekBar);
        mSeekBar.setOnSeekBarChangeListener(this);

        mJackPlayer = JackPlayer.getInstance();
        mJackPlayer.setSurfaceView(mSurfaceView);
        mJackPlayer.setSourceData(new File(Environment.getExternalStorageDirectory() + File.separator + "demo.mp4")
                .getAbsolutePath());

        // 拉流方式播放
        mJackPlayer.setSourceData("rtmp://58.200.131.2:1935/livetv/hunantv");

        mJackPlayer.setOnPreparedListener(new JackPlayer.OnPreparedListener() {
            @Override
            public void onPrepared() {

                duration = mJackPlayer.getDuration();

                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        if (duration != 0) {
                            mTvTime.setText("00:00/" + getMinutes(duration) + ":" + getSecond(duration));
                            mTvTime.setVisibility(View.VISIBLE);
                            mSeekBar.setVisibility(View.VISIBLE);
                        } else {
                            mTvTime.setVisibility(View.GONE);
                            mSeekBar.setVisibility(View.GONE);
                        }

                        mTvState.setTextColor(Color.GREEN); // 绿色
                        mTvState.setText("init succeed.");
                    }
                });
                mJackPlayer.start();
            }
        });

        mJackPlayer.setOnErrorListener(new JackPlayer.OnErrorListener() {
            @Override
            public void onError(final String errorInfo) {
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        // Toast.makeText(MainActivity.this, "出错了，错误详情是:" + errorInfo, Toast.LENGTH_SHORT).show();
                        mTvState.setTextColor(Color.RED); // 红色
                        mTvState.setText("Failed:" + errorInfo);
                    }
                });
            }
        });

        mJackPlayer.setOnProgressListener(new JackPlayer.OnProgressListener() {
            @Override
            public void onProgress(final int time) {
                // mSeekBar.setProgress(time * 100 / duration);
                if (!isTouch) {
                    // 判断是人为拖动还是自动在播放
                    runOnUiThread(new Runnable() {
                        @SuppressLint("SetTextI18n")
                        @Override
                        public void run() {
                            if (duration != 0) {
                                mTvTime.setText(getMinutes(time) + ":" + getSecond(time) + "/" +
                                        getMinutes(duration) + ":" + getSecond(duration));

                                // 拖动条
                                mSeekBar.setProgress(time * 100 / duration);
                            }
                        }
                    });
                }
            }
        });
    }

    // 获取分钟字符串
    private String getMinutes(int duration) {
        int min = duration / 60;
        if (min <= 9) {
            return "0" + min;
        }
        return String.valueOf(min);
    }

    // 获取秒钟字符串
    private String getSecond(int duration) {
        int second = duration % 60;
        if (second <= 9) {
            return "0" + second;
        }
        return String.valueOf(second);
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (mJackPlayer != null) {
            mJackPlayer.prepare();
        }
    }

    @Override
    protected void onStop() {
        super.onStop();
        if (mJackPlayer != null) {
            mJackPlayer.stop();
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (mJackPlayer != null) {
            mJackPlayer.release();
        }
    }

    /**
     * @param seekBar  控件
     * @param progress 1 - 100%
     * @param fromUser 是否是用户拖动改变
     */
    @SuppressLint("SetTextI18n")
    @Override
    public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
        if (fromUser) {
            mTvTime.setText(getMinutes(progress * duration / 100) + ":" + getSecond(progress * duration / 100) + "/" +
                    getMinutes(duration) + ":" + getSecond(duration));
        }
    }

    @Override
    public void onStartTrackingTouch(SeekBar seekBar) {
        isTouch = true;
    }

    @Override
    public void onStopTrackingTouch(SeekBar seekBar) {
        isTouch = false;

        int currentProgress = seekBar.getProgress();

        // 1 - 100 需要转化成c++的时间戳
        int playProgress = currentProgress * duration / 100;

        mJackPlayer.seek(playProgress);
    }
}
