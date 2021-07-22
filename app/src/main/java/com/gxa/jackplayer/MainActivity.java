package com.gxa.jackplayer;

import androidx.appcompat.app.AppCompatActivity;

import android.os.Bundle;
import android.os.Environment;
import android.view.Window;
import android.view.WindowManager;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;

public class MainActivity extends AppCompatActivity {

    private JackPlayer mJackPlayer;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON,
                WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        setContentView(R.layout.activity_main);

        mJackPlayer = JackPlayer.getInstance();
        mJackPlayer.setSourceData(new File(Environment.getExternalStorageDirectory() + File.separator + "demo.mp4")
                .getAbsolutePath());
        mJackPlayer.setOnPreparedListener(new JackPlayer.OnPreparedListener() {
            @Override
            public void onPrepared() {
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        Toast.makeText(MainActivity.this, "准备成功，即将开始播放", Toast.LENGTH_SHORT).show();
                    }
                });
                mJackPlayer.start();
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
