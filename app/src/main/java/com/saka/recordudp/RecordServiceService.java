package com.saka.recordudp;

import android.annotation.TargetApi;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.hardware.display.DisplayManager;
import android.hardware.display.VirtualDisplay;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaFormat;
import android.media.projection.MediaProjection;
import android.media.projection.MediaProjectionManager;
import android.os.Build;
import android.os.Environment;
import android.os.IBinder;
import android.view.Surface;

import com.saka.logutil.LogUtil;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.atomic.AtomicBoolean;

import static android.media.MediaFormat.KEY_MAX_INPUT_SIZE;

@TargetApi(Build.VERSION_CODES.LOLLIPOP)
public class RecordServiceService extends Service {

    private int mScreenWidth;
    private int mScreenHeight;
    private int mScreenDensity;
    private int mResultCode;
    private Intent mResultData;
    private MediaCodec mEncoder;
    private Surface mSurface;
    private static final String MIME_TYPE = "video/avc"; // H.264 Advanced Video Coding
    private static final int FRAME_RATE = 30; // 30 fps
    private static final int IFRAME_INTERVAL = 1; // 10 seconds between I-frames
    private static final int TIMEOUT_US = 10000;
    private AtomicBoolean mQuit = new AtomicBoolean(false);
    private MediaCodec.BufferInfo mBufferInfo = new MediaCodec.BufferInfo();
    private static final int FRAME_LENGTH = 1400;
    private long presentationTimeUs;
    private SendRTPLib udpTools;
    private LinkedBlockingQueue<byte[]> datas = new LinkedBlockingQueue<>();
    private Thread sendThread;
    /**
     * 是否为标清视频
     */
    private boolean isVideoSd;

    private MediaProjection mMediaProjection;
    private VirtualDisplay mVirtualDisplay;

    public RecordServiceService() {
    }

    @Override
    public IBinder onBind(Intent intent) {
        LogUtil.e("onbind");
        return null;
    }

    @Override
    public void onCreate() {
        LogUtil.e("oncreate");
        udpTools = new SendRTPLib();
        sendThread = new Thread(new Runnable() {
            @Override
            public void run() {
                udpTools.initSocket("192.168.31.144", 5555);
                while (!mQuit.get() && !Thread.interrupted()) {
                    try {
                        byte[] tmp = datas.take();
                        LogUtil.e(Arrays.toString(tmp));
                        udpTools.sendData(tmp, System.currentTimeMillis());
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                }
            }
        });
        sendThread.start();
        super.onCreate();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        LogUtil.d("onStartCommand");
        mResultCode = intent.getIntExtra("code", -1);
        mResultData = intent.getParcelableExtra("data");
        mScreenWidth = intent.getIntExtra("width", 320);
        mScreenHeight = intent.getIntExtra("height", 240);
        mScreenDensity = intent.getIntExtra("density", 1);
        isVideoSd = intent.getBooleanExtra("quality", true);
        createMediaProjection();
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    try {
                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2) {
                            prepareEncoder();
                        }
                    } catch (IOException e) {
                        e.printStackTrace();
                    }
                    recordVirtualDisplay();
                } finally {
                    release();
                }
            }
        }).start();

        return Service.START_NOT_STICKY;

    }

    private void createMediaProjection() {
        mMediaProjection = ((MediaProjectionManager) getSystemService(Context.MEDIA_PROJECTION_SERVICE))
                .getMediaProjection(mResultCode, mResultData);
    }

    private void prepareEncoder() throws IOException {
        MediaFormat format = MediaFormat.createVideoFormat(MIME_TYPE, mScreenWidth, mScreenHeight);
        format.setInteger(MediaFormat.KEY_COLOR_FORMAT, MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface);
        format.setInteger(MediaFormat.KEY_BIT_RATE, 2000000);
        format.setInteger(MediaFormat.KEY_FRAME_RATE, FRAME_RATE);
        format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, IFRAME_INTERVAL);
        format.setInteger(KEY_MAX_INPUT_SIZE, 0);
        mEncoder = MediaCodec.createEncoderByType(MIME_TYPE);
        mEncoder.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);

        // 这一步非常关键，它设置的，是MediaCodec的编码源，也就是说，我要告诉mEncoder，你给我解码哪些流。
        mSurface = mEncoder.createInputSurface();
//        mMediaPublisher.setEncoder(mEncoder);
//        start();
        mEncoder.start();
        mVirtualDisplay = mMediaProjection.createVirtualDisplay("-display",
                mScreenWidth, mScreenHeight, mScreenDensity, DisplayManager.VIRTUAL_DISPLAY_FLAG_PUBLIC,
                mSurface, null, null);

    }

    private void recordVirtualDisplay() {
        android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_URGENT_AUDIO);
        while (!mQuit.get()) {
            // 通过outputBufferId获取buffer 2016/12/1 15:23
            int outputBufferId = mEncoder.dequeueOutputBuffer(mBufferInfo, TIMEOUT_US);
            if (outputBufferId == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                resetOutputFormat(mBufferInfo);
            } else if (outputBufferId == MediaCodec.INFO_TRY_AGAIN_LATER) {
                try {
                    // wait 10ms
                    Thread.sleep(10);
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
            } else if (outputBufferId >= 0) {
                // 根据outputBufferId获取ByteBuffer后，写入文件 2016/12/1 15:24
                encodeToVideoTrack(outputBufferId, mBufferInfo);
                mEncoder.releaseOutputBuffer(outputBufferId, false);
            }
        }
    }

    private void resetOutputFormat(final MediaCodec.BufferInfo vBufferInfo) {
        MediaFormat newFormat = mEncoder.getOutputFormat();
        ByteBuffer b = newFormat.getByteBuffer("csd-0");    // SPS
        final byte[] sps = new byte[b.capacity()];
        b.get(sps);
        ByteBuffer c = newFormat.getByteBuffer("csd-1");    // PPS
        final byte[] pps = new byte[c.capacity()];
        c.get(pps);
        try {
            datas.put(sps);
            datas.put(pps);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    private void appendFileData(byte[] data) {
        try {
            File file = new File(Environment.getExternalStorageDirectory(),
                    "5.h264");
            if (!file.exists()) {
                file.createNewFile();
            }
            FileOutputStream fileOutputStream = new FileOutputStream(file, true);
            fileOutputStream.write(data);
            fileOutputStream.close();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    /**
     * 写入文件 2016/12/1 15:24
     *
     * @param outputBufferId bufferId
     */
    private void encodeToVideoTrack(int outputBufferId, final MediaCodec.BufferInfo vBufferInfo) {
        ByteBuffer encodedData = mEncoder.getOutputBuffer(outputBufferId);
        if ((mBufferInfo.flags & MediaCodec.BUFFER_FLAG_CODEC_CONFIG) != 0) {
            mBufferInfo.size = 0;
        }
        if (mBufferInfo.size == 0) {
            encodedData = null;
        }

        if (encodedData != null) {
            encodedData.position(mBufferInfo.offset);
            encodedData.limit(mBufferInfo.offset + mBufferInfo.size);
            final byte[] bytes = new byte[encodedData.remaining()];
            encodedData.get(bytes, 0, bytes.length);
            try {
                datas.put(bytes);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
    }

    private void release() {
        if (mEncoder != null) {
            mEncoder.stop();
            mEncoder.release();
            mEncoder = null;
        }


        if (mVirtualDisplay != null) {
            mVirtualDisplay.release();
        }

        if (mMediaProjection != null) {
            mMediaProjection.stop();
        }
        udpTools.releaseSocket();
    }

    @Override
    public void onDestroy() {
        LogUtil.d("onDestroy");
        mQuit.set(true);
        super.onDestroy();
    }


}
