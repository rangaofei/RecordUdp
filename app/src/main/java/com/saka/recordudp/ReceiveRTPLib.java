package com.saka.recordudp;

import android.content.Context;

import com.saka.logutil.LogUtil;

/**
 * Created by Administrator on 2017/9/26 0026.
 */

public class ReceiveRTPLib {
    static {
        System.loadLibrary("native-lib");
    }

    public static H264dataCallBack h264dataCallBack;

    public void setH264dataCallBack(H264dataCallBack h264dataCallBack) {
        this.h264dataCallBack = h264dataCallBack;
    }

    public static native void initSocket(Context h);

}
