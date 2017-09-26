package com.saka.recordudp;

import android.content.Context;
import android.content.Intent;
import android.databinding.DataBindingUtil;
import android.media.projection.MediaProjectionManager;
import android.os.Build;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.util.DisplayMetrics;
import android.widget.CompoundButton;
import android.widget.Toast;

import com.saka.logutil.LogUtil;
import com.saka.recordudp.databinding.ActivityRecordScreenBinding;

public class RecordScreenActivity extends AppCompatActivity {

    private ActivityRecordScreenBinding binding;
    private MediaProjectionManager projectionManager;
    private static final int PERMISSION_CODE = 1;
    private int mScreenWidth;
    private int mScreenHeight;
    private int mScreenDensity;
    private boolean isVideoSd = true;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        binding = DataBindingUtil.setContentView(this, R.layout.activity_record_screen);
        new Thread(new Runnable() {
            @Override
            public void run() {
                ReceiveRTPLib receiveRTPLib = new ReceiveRTPLib();
                receiveRTPLib.initSocket();
            }
        }).start();

        if (AppUtils.isServiceRunning(RecordServiceService.class.getName(), this)) {
            binding.cbRecord.setChecked(true);
        } else {
            binding.cbRecord.setChecked(false);
        }
        binding.cbRecord.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                if (!isChecked) {
                    LogUtil.e("未选中");
                    binding.cbRecord.setText("启动同屏");
                    Intent intent = new Intent(RecordScreenActivity.this, RecordServiceService.class);
                    stopService(intent);
                } else {
                    LogUtil.e("选中");
                    binding.cbRecord.setText("关闭同屏");
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                        startActivityForResult(projectionManager.createScreenCaptureIntent(), PERMISSION_CODE);
                    }
                }
            }
        });
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            projectionManager = (MediaProjectionManager) getSystemService(Context.MEDIA_PROJECTION_SERVICE);
        } else {
            Toast.makeText(this, "不支持", Toast.LENGTH_SHORT).show();
        }
        getScreenBaseInfo();
    }

    @Override
    protected void onResume() {
        super.onResume();
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (resultCode != RESULT_OK) {
            Toast.makeText(this, "启动失败", Toast.LENGTH_SHORT).show();
            return;
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            Intent service = new Intent(this, RecordServiceService.class);
            service.putExtra("code", resultCode);
            service.putExtra("data", data);
            service.putExtra("width", 1920);
            service.putExtra("height", 1080);
            service.putExtra("density", mScreenDensity);
            service.putExtra("quality", isVideoSd);
            startService(service);
        }
    }

    private void getScreenBaseInfo() {
        DisplayMetrics metrics = new DisplayMetrics();
        getWindowManager().getDefaultDisplay().getMetrics(metrics);
        mScreenWidth = metrics.widthPixels;
        mScreenHeight = metrics.heightPixels;
        mScreenDensity = metrics.densityDpi;
    }
}
