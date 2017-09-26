package com.saka.recordudp;

/**
 * Created by Administrator on 2017/9/25 0025.
 */

public class SendRTPLib {
    static {
        System.loadLibrary("native-lib");
    }

    public native void initSocket(String addr, int port);

    public native void sendData(byte[] data, Long timeStamp);

    public native void testData(int[] datas, int size);

    public native void releaseSocket();
}
