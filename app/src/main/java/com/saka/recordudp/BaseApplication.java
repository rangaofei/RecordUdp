package com.saka.recordudp;

import android.app.Application;

import com.saka.logutil.LogConfig;
import com.saka.logutil.LogUtil;

/**
 * Created by Administrator on 2017/9/25 0025.
 */

public class BaseApplication extends Application {
    @Override
    public void onCreate() {
        super.onCreate();
        LogConfig logConfig = new LogConfig.Builder()
                .isDebug(true)
                .outputRect(false)
                .outputStackInfo(true)
                .build();
        LogUtil.init(logConfig);
    }
}
