package com.saka.recordudp;

import android.content.Context;
import android.content.Intent;
import android.databinding.DataBindingUtil;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaFormat;
import android.media.projection.MediaProjectionManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.support.annotation.RequiresApi;
import android.support.v7.app.AppCompatActivity;
import android.util.DisplayMetrics;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.widget.CompoundButton;
import android.widget.Toast;

import com.saka.logutil.LogUtil;
import com.saka.recordudp.databinding.ActivityRecordScreenBinding;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.Arrays;

import static android.media.MediaFormat.KEY_MAX_INPUT_SIZE;

@RequiresApi(api = Build.VERSION_CODES.LOLLIPOP)
public class RecordScreenActivity extends AppCompatActivity implements SurfaceHolder.Callback {
    private SurfaceHolder surfaceHolder;
    private SurfaceView surfaceView;
    private static MediaCodec mediaCodec;
    private long mCount = 0;
    private ActivityRecordScreenBinding binding;
    private MediaProjectionManager projectionManager;
    private static final int PERMISSION_CODE = 1;
    private int mScreenWidth;
    private int mScreenHeight;
    private int mScreenDensity;
    private boolean isVideoSd = true;
    static MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
    static int inIndex = -1;
    int sampleSize = 0;
    static long pts = 0;

    byte[] sps = {0, 0, 0, 1, 103, 100, 0, 40, -84, 52, -59, 1, -32, 17, 31, 120, 11, 80, 16, 16, 31, 0, 0, 3, 3, -23, 0, 0, -22, 96, -108};
    byte[] pps = {0, 0, 0, 1, 104, -18, 60, -128};

    public static void getResultData(byte[] data, int length) {
        LogUtil.d("接收到数据" + Arrays.toString(data));
        if (length < 0) {
            return;
        }
        appendFileData(data,length);
        ByteBuffer[] inputBuffers = mediaCodec.getInputBuffers();
        inIndex = mediaCodec.dequeueInputBuffer(0);
        LogUtil.d("index=" + inIndex);
        if (inIndex >= 0) {
            ByteBuffer buffer = inputBuffers[inIndex];
            buffer.clear();
//            buffer.rewind();
            buffer.put(data, 0, length);
            mediaCodec.queueInputBuffer(inIndex, 0, length, ++pts, 0);
//            dequeueAndRenderOutputBuffer(0);
        }
        MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
        int outIndex = mediaCodec.dequeueOutputBuffer(info, 0);
        if (outIndex >= 0) {
            mediaCodec.releaseOutputBuffer(outIndex, true);
            outIndex = mediaCodec.dequeueOutputBuffer(info, 0);
        }
//        else {
//            if (!dequeueAndRenderOutputBuffer(1000) && !dequeueAndRenderOutputBuffer(30 * 1000)) {
//            }
//        }

        if ((info.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) {
            LogUtil.e("eror");
        }
    }

    public static boolean dequeueAndRenderOutputBuffer(int outtime) {
        int outIndex = mediaCodec.dequeueOutputBuffer(info, outtime);
        switch (outIndex) {
            case MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED:
                return false;
            case MediaCodec.INFO_OUTPUT_FORMAT_CHANGED:
                return false;
            case MediaCodec.INFO_TRY_AGAIN_LATER:
                return false;
            case MediaCodec.BUFFER_FLAG_SYNC_FRAME:
                return false;
            default:
                mediaCodec.releaseOutputBuffer(outIndex, true);//show image right now
                return true;
        }
    }


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        binding = DataBindingUtil.setContentView(this, R.layout.activity_record_screen);
        surfaceHolder = binding.sv.getHolder();
        surfaceHolder.addCallback(this);

        new Thread(new Runnable() {
            @Override
            public void run() {
                ReceiveRTPLib.initSocket(RecordScreenActivity.this);
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

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        try {
            mediaCodec = MediaCodec.createDecoderByType("Video/AVC");
            MediaFormat mediaFormat = MediaFormat.createVideoFormat("video/avc", 1080, 1920);
            mediaFormat.setByteBuffer("csd-0", ByteBuffer.wrap(sps));
            mediaFormat.setByteBuffer("csd-1", ByteBuffer.wrap(pps));
//            mediaFormat.setInteger(MediaFormat.KEY_COLOR_FORMAT, MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface);
//            mediaFormat.setInteger(MediaFormat.KEY_BIT_RATE, 2000000);
//            mediaFormat.setInteger(MediaFormat.KEY_FRAME_RATE, 30);
//            mediaFormat.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 1);
//            mediaFormat.setInteger(KEY_MAX_INPUT_SIZE, 0);
            mediaCodec.configure(mediaFormat, surfaceHolder.getSurface(), null, 0);
            mediaCodec.start();

        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {

    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {

    }

    private static void appendFileData(byte[] data,int length) {
        try {
            File file = new File(Environment.getExternalStorageDirectory(),
                    "test.h264");
            if (!file.exists()) {
                file.createNewFile();
            }
            FileOutputStream fileOutputStream = new FileOutputStream(file, true);
            fileOutputStream.write(data,0,length);
            fileOutputStream.close();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }
}
