//
// Created by Administrator on 2017/9/26 0026.
//

#include <jni.h>
#include <string>
#include <android/log.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "h264.h"

#define TAG "JNI"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,TAG,__VA_ARGS__)
#define MAX 1600*50
#ifdef __cplusplus
extern "C" {
#endif
JNIEXPORT void JNICALL
Java_com_saka_recordudp_ReceiveRTPLib_initSocket(JNIEnv *env, jobject instance, jobject h) {
    struct sockaddr_in addr;
    int sockfd, len = 0;
    int addr_len = sizeof(struct sockaddr_in);
    char buffer[1600];
    char *result = new char[MAX];
    int currentIndex = 0;
    JNIEnv *c = env;
    /* 建立socket，注意必须是SOCK_DGRAM */
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        LOGE ("socket");
        exit(1);
    }
    /* 填写sockaddr_in 结构 */
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(5555);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);// 接收任意IP发来的数据

    /* 绑定socket */
    if (bind(sockfd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        LOGE("connect");
        exit(1);
    }
    //1.获得实例对应的class类
    jclass jcls = c->GetObjectClass(h);
    if (jcls == 0) {
        LOGE("出错了");
        return;
    }
    //2.通过class类找到对应的method id
    //name 为java类中变量名，Ljava/lang/String; 为变量的类型String
    jmethodID jmid = c->GetStaticMethodID(jcls, "getResultData", "([BI)V");
    if (jmid == 0) {
        LOGE("find method1 error");
        return;
    }
    jbyteArray re = env->NewByteArray(MAX);
    memset(result, 0, MAX);
    while (1) {
        bzero(buffer, sizeof(buffer));
        len = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *) &addr,
                       (socklen_t *) &addr_len);
        /* 显示client端的网络地址和收到的字符串消息 */
        LOGE("Received a string from client %s, string is: %d\n",
             inet_ntoa(addr.sin_addr), buffer[len-1]);
        if (buffer[0] == 0x00 && buffer[1] == 0x00 && buffer[2] == 0x00 && buffer[3] == 0x01) {

            c->SetByteArrayRegion(re, 0, MAX, (const jbyte *) result);

            c->CallStaticVoidMethod(jcls, jmid, re, currentIndex);
            memset(result, 0, MAX);
            currentIndex = 0;
            memcpy(result, &buffer[0], len);
        } else {
            if (currentIndex + len > MAX) {
                LOGE("超出范围");
                memset(result, 0, MAX);
            }
            memcpy(result + currentIndex, &buffer[0], len);
        }
        currentIndex += len;
    }
//    env->ReleaseByteArrayElements(re, (jbyte *) result, 0);
}


#ifdef __cplusplus
}
#endif