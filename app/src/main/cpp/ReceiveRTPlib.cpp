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

#ifdef __cplusplus
extern "C" {
#endif
JNIEXPORT void JNICALL
Java_com_saka_recordudp_ReceiveRTPLib_initSocket(JNIEnv *env, jobject instance) {
    struct sockaddr_in addr;
    int sockfd, len = 0;
    int addr_len = sizeof(struct sockaddr_in);
    char buffer[256];
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
    while (1) {
        bzero(buffer, sizeof(buffer));
        len = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *) &addr,
                       (socklen_t *) &addr_len);
        /* 显示client端的网络地址和收到的字符串消息 */
        LOGE("Received a string from client %s, string is: %s\n",
             inet_ntoa(addr.sin_addr), buffer);
    }
}


#ifdef __cplusplus
}
#endif