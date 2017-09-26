package com.saka.recordudp;

import android.app.ActivityManager;
import android.content.Context;
import android.os.Environment;

import java.io.File;
import java.io.FileInputStream;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.List;

/**
 * Created by unual on 2017/3/9.
 */

public class AppUtils {

    /**
     * 转化ip为字符串
     */
    private static String intToIp(int i) {
        return (i & 0xFF) + "." +
                ((i >> 8) & 0xFF) + "." +
                ((i >> 16) & 0xFF) + "." +
                (i >> 24 & 0xFF);
    }

    /**
     * 获取系统当前事件
     *
     * @return 字符串形式的时间格式
     */
    public static String getCurrentTime() {
        Date now = new Date();
        SimpleDateFormat dateFormat = new SimpleDateFormat("yyyy/MM/dd HH:mm:ss");
        return dateFormat.format(now);
    }

    public static String getFormateTime(String s) {
        Date now = new Date(Long.parseLong(s));
        SimpleDateFormat dateFormat = new SimpleDateFormat("yyyy/MM/dd HH:mm");
        return dateFormat.format(now);
    }


    /**
     * 校验某个服务是否还存在
     */
    public static boolean isServiceRunning(String serviceName, Context context) {
        // 校验服务是否还存在
        ActivityManager am = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        List<ActivityManager.RunningServiceInfo> services = am.getRunningServices(100);
        for (ActivityManager.RunningServiceInfo info : services) {
            // 得到所有正在运行的服务的名称
            String name = info.service.getClassName();
            if (serviceName.equals(name)) {
                return true;
            }
        }
        return false;
    }

    public static boolean hasSDCard() {
        return Environment.getExternalStorageState().equals(
                Environment.MEDIA_MOUNTED);
    }

    /**
     * 获取单个文件的MD5值！
     *
     * @param file
     * @return
     */

    public static String getFileMD5(File file) {
        if (!file.isFile()) {
            return null;
        }
        MessageDigest digest = null;
        FileInputStream in = null;
        byte buffer[] = new byte[1024];
        int len;
        try {
            digest = MessageDigest.getInstance("MD5");
            in = new FileInputStream(file);
            while ((len = in.read(buffer, 0, 1024)) != -1) {
                digest.update(buffer, 0, len);
            }
            in.close();
        } catch (Exception e) {
            e.printStackTrace();
            return null;
        }
        byte[] s = digest.digest();
        return bytes2String(s);
    }

    public static String getMD5(String str) {
        String reStr = null;
        try {
            MessageDigest md = MessageDigest.getInstance("MD5");//创建具有指定算法的信息摘要
            md.update(str.getBytes()); //使用指定的字节更新摘要
            byte ss[] = md.digest();  //通过执行诸如填充之类的最终操作完成哈希计算
            reStr = bytes2String(ss);
        } catch (NoSuchAlgorithmException e) {
            e.printStackTrace();
        }
        return reStr;
    }

    private static String bytes2String(byte[] aa) { //将字节数组转换为字符串
        String hash = "";
        for (int i = 0; i < aa.length; i++) {
            int temp;
            temp = aa[i] < 0 ? aa[i] + 256 : aa[i];    //如果小于0,转换为正整数
            if (temp < 16) {
                hash += "0";
            }
            hash += Integer.toString(temp, 16);  //转换为16进制
        }
        hash = hash.toUpperCase();   //全部转为大写
        return hash;
    }

    public static String getExtensionName(String fileName) {
        return fileName.substring(fileName.lastIndexOf(".") + 1);
    }

}
