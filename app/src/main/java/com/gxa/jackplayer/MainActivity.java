package com.gxa.jackplayer;

import androidx.appcompat.app.AppCompatActivity;

import android.graphics.Color;
import android.os.Bundle;
import android.os.Environment;
import android.view.SurfaceView;
import android.view.Window;
import android.view.WindowManager;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;

public class MainActivity extends AppCompatActivity {

    private JackPlayer mJackPlayer;
    private TextView mTvState;
    private SurfaceView mSurfaceView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON,
                WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        setContentView(R.layout.activity_main);
        mTvState = findViewById(R.id.tv_time);
        mSurfaceView = findViewById(R.id.surfaceView);

        mJackPlayer = JackPlayer.getInstance();
        mJackPlayer.setSurfaceView(mSurfaceView);
        mJackPlayer.setSourceData(new File(Environment.getExternalStorageDirectory() + File.separator + "demo.mp4")
                .getAbsolutePath());
        mJackPlayer.setOnPreparedListener(new JackPlayer.OnPreparedListener() {
            @Override
            public void onPrepared() {
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
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
}
