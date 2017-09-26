package com.saka.recordudp;

/**
 * Created by Administrator on 2017/9/26 0026.
 */

public class ReceiveRTPLib {
    static {
        System.loadLibrary("native-lib");
    }

    public native void initSocket();
}
