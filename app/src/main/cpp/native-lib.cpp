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
int sd = 1;
sockaddr_in addr_dst;
unsigned short seq_num = 1;
unsigned int ts_current = 0;
unsigned int timestamp_increse = 0;
float framerate = 30;
unsigned int ssrc = 10;

typedef struct {
    int startcodeprefix_len;      //! 4 for parameter sets and first slice in picture, 3 for everything else (suggested)
    unsigned len;                 //! Length of the NAL unit (Excluding the start code, which does not belong to the NALU)
    unsigned max_size;            //! Nal Unit Buffer size
    int forbidden_bit;            //! should be always FALSE
    int nal_reference_idc;        //! NALU_PRIORITY_xxxx
    int nal_unit_type;            //! NALU_TYPE_xxxx
    char *buf;                    //! contains the first byte followed by the EBSP
    unsigned short lost_packets;  //! true, if packet loss is detected
} NALU_t;

static int FindStartCode2(char *Buf);//查找开始字符0x000001
static int FindStartCode3(char *Buf);//查找开始字符0x00000001
//static bool flag = true;
static int info2 = 0, info3 = 0;
RTP_FIXED_HEADER *rtp_hdr;

NALU_HEADER *nalu_hdr;
FU_INDICATOR *fu_ind;
FU_HEADER *fu_hdr;


//为NALU_t结构体分配内存空间
NALU_t *AllocNALU(int buffersize) {
    NALU_t *n;

    if ((n = (NALU_t *) calloc(1, sizeof(NALU_t))) == NULL) {
        printf("AllocNALU: n");
        exit(0);
    }

    n->max_size = buffersize;

    if ((n->buf = (char *) calloc(buffersize, sizeof(char))) == NULL) {
        free(n);
        printf("AllocNALU: n->buf");
        exit(0);
    }

    return n;
}
//释放
void FreeNALU(NALU_t *n) {
    if (n) {
        if (n->buf) {
            free(n->buf);
            n->buf = NULL;
        }
        free(n);
    }
}

//这个函数输入为一个NAL结构体，主要功能为得到一个完整的NALU并保存在NALU_t的buf中，获取他的长度，填充F,IDC,TYPE位。
//并且返回两个开始字符之间间隔的字节数，即包含有前缀的NALU的长度
int GetAnnexbNALU(NALU_t *nalu, char *data, int length) {
    int pos = 0;
    int StartCodeFound, rewind;
    nalu->startcodeprefix_len = 3;//初始化码流序列的开始字符为3个字节
    info2 = FindStartCode2(data);//判断是否为0x000001
    if (info2 != 1) {

        info3 = FindStartCode3(data);//判断是否为0x00000001
        if (info3 != 1)//如果不是，返回-1
        {
            return -1;
        } else {
            //如果是0x00000001,得到开始前缀为4个字节
            pos = 4;
            nalu->startcodeprefix_len = 4;
        }
    } else {
        //如果是0x000001,得到开始前缀为3个字节
        nalu->startcodeprefix_len = 3;
        pos = 3;
    }
    nalu->len = length - nalu->startcodeprefix_len;    //NALU长度，不包括头部。
    memcpy(nalu->buf, data + nalu->startcodeprefix_len,
           nalu->len);//拷贝一个完整NALU，不拷贝起始前缀0x000001或0x00000001
    nalu->forbidden_bit = nalu->buf[0] & 0x80; //1 bit
    nalu->nal_reference_idc = nalu->buf[0] & 0x60; // 2 bit
    nalu->nal_unit_type = (nalu->buf[0]) & 0x1f;// 5 bit
    return (nalu->len);//返回两个开始字符之间间隔的字节数，即包含有前缀的NALU的长度
}

int sendData(char *data, int length, long timeStamp) {
    NALU_t *nalu_t;
    char *nalu_payload;
    char sendbuf[1500];
    int bytes = 0;
    nalu_t = AllocNALU(800000);//为结构体nalu_t及其成员buf分配空间。返回值为指向nalu_t存储空间的指针
    GetAnnexbNALU(nalu_t, data, length);//每执行一次，文件的指针指向本次找到的NALU的末尾，下一个位置即为下个NALU的起始码0x000001
    //（1）一个NALU就是一个RTP包的情况： RTP_FIXED_HEADER（12字节）  + NALU_HEADER（1字节） + EBPS
    //（2）一个NALU分成多个RTP包的情况： RTP_FIXED_HEADER （12字节） + FU_INDICATOR （1字节）+  FU_HEADER（1字节） + EBPS(1400字节)

    memset(sendbuf, 0, 1500);//清空sendbuf；此时会将上次的时间戳清空，因此需要ts_current来保存上次的时间戳值
    //rtp固定包头，为12字节,该句将sendbuf[0]的地址赋给rtp_hdr，以后对rtp_hdr的写入操作将直接写入sendbuf。
    rtp_hdr = (RTP_FIXED_HEADER *) &sendbuf[0];
    //设置RTP HEADER，
    rtp_hdr->payload = H264;  //负载类型号，
    rtp_hdr->version = 2;  //版本号，此版本固定为2
    rtp_hdr->marker = 0;   //标志位，由具体协议规定其值。
    rtp_hdr->ssrc[0] = 0x00;    //随机指定为10，并且在本RTP会话中全局唯一
    rtp_hdr->ssrc[1] = 0x00;
    rtp_hdr->ssrc[2] = 0x00;
    rtp_hdr->ssrc[3] = 0x12;
    //  当一个NALU小于1400字节的时候，采用一个单RTP包发送
    if (nalu_t->len - 1 <= 1400) {
        //设置rtp M 位；
        rtp_hdr->marker = 1;
        rtp_hdr->seq_no = htons(seq_num++); //序列号，每发送一个RTP包增1，htons，将主机字节序转成网络字节序。
        //设置NALU HEADER,并将这个HEADER填入sendbuf[12]
        nalu_hdr = (NALU_HEADER *) &sendbuf[12]; //将sendbuf[12]的地址赋给nalu_hdr，之后对nalu_hdr的写入就将写入sendbuf中；
        nalu_hdr->F = nalu_t->forbidden_bit;
        nalu_hdr->NRI = nalu_t->nal_reference_idc
                >> 5;//有效数据在n->nal_reference_idc的第6，7位，需要右移5位才能将其值赋给nalu_hdr->NRI。
        nalu_hdr->TYPE = nalu_t->nal_unit_type;

        nalu_payload = &sendbuf[13];//同理将sendbuf[13]赋给nalu_payload
        memcpy(nalu_payload, nalu_t->buf + 1,
               nalu_t->len - 1);//去掉nalu头的nalu剩余内容写入sendbuf[13]开始的字符串。

        ts_current = ts_current + timestamp_increse;
        rtp_hdr->timestamp[0] = (unsigned char) (ts_current >> 24);
        rtp_hdr->timestamp[1] = (unsigned char) (ts_current >> 16);
        rtp_hdr->timestamp[2] = (unsigned char) (ts_current >> 8);
        rtp_hdr->timestamp[3] = (unsigned char) (ts_current >> 0);
        bytes = nalu_t->len + 12;  //获得sendbuf的长度,为nalu的长度（包含NALU头但除去起始前缀）加上rtp_header的固定长度12字节
//        send(socket1, sendbuf, bytes, 0);//发送rtp包
        sendto(sd, sendbuf, bytes, 0, (struct sockaddr *) &(addr_dst),
               sizeof(sockaddr_in));
        //  Sleep(100);

    } else if (nalu_t->len - 1 > 1400)  //这里就要分成多个RTP包发送了。
    {
        //得到该nalu需要用多少长度为1400字节的RTP包来发送
        int k = 0, last = 0;
        k = (nalu_t->len - 1) / 1400;//需要k个1400字节的RTP包，这里为什么不加1呢？因为是从0开始计数的。
        last = (nalu_t->len - 1) % 1400;//最后一个RTP包的需要装载的字节数
        int t = 0;//用于指示当前发送的是第几个分片RTP包
        ts_current = ts_current + timestamp_increse;
        rtp_hdr->timestamp[0] = (unsigned char) (ts_current >> 24);
        rtp_hdr->timestamp[1] = (unsigned char) (ts_current >> 16);
        rtp_hdr->timestamp[2] = (unsigned char) (ts_current >> 8);
        rtp_hdr->timestamp[3] = (unsigned char) (ts_current >> 0);
        if (last == 0) {
            for (t = 0; t < k; t++) {
                rtp_hdr->seq_no = htons(seq_num++); //序列号，每发送一个RTP包增1
                if (!t)//发送一个需要分片的NALU的第一个分片，置FU HEADER的S位,t = 0时进入此逻辑。
                {
                    //设置rtp M 位；
                    rtp_hdr->marker = 0;  //最后一个NALU时，该值设置成1，其他都设置成0。
                    //设置FU INDICATOR,并将这个HEADER填入sendbuf[12]
                    fu_ind = (FU_INDICATOR *) &sendbuf[12]; //将sendbuf[12]的地址赋给fu_ind，之后对fu_ind的写入就将写入sendbuf中；
                    fu_ind->F = nalu_t->forbidden_bit;
                    fu_ind->NRI = nalu_t->nal_reference_idc >> 5;
                    fu_ind->TYPE = 28;  //FU-A类型。

                    //设置FU HEADER,并将这个HEADER填入sendbuf[13]
                    fu_hdr = (FU_HEADER *) &sendbuf[13];
                    fu_hdr->E = 0;
                    fu_hdr->R = 0;
                    fu_hdr->S = 1;
                    fu_hdr->TYPE = nalu_t->nal_unit_type;

                    nalu_payload = &sendbuf[14];//同理将sendbuf[14]赋给nalu_payload
                    memcpy(nalu_payload, nalu_t->buf + 1, 1400);//去掉NALU头，每次拷贝1400个字节。

                    bytes = 1400 +
                            14;//获得sendbuf的长度,为nalu的长度（除去起始前缀和NALU头）加上rtp_header，fu_ind，fu_hdr的固定长度                                                            14字节
                    sendto(sd, sendbuf, bytes, 0, (struct sockaddr *) &(addr_dst),
                           sizeof(sockaddr_in));
                } else if (t != 0 && t == k - 1)//发送的是最后一个分片
                {                //设置rtp M 位；当前传输的是最后一个分片时该位置1
                    rtp_hdr->marker = 1;
                    //设置FU INDICATOR,并将这个HEADER填入sendbuf[12]
                    fu_ind = (FU_INDICATOR *) &sendbuf[12]; //将sendbuf[12]的地址赋给fu_ind，之后对fu_ind的写入就将写入sendbuf中；
                    fu_ind->F = nalu_t->forbidden_bit;
                    fu_ind->NRI = nalu_t->nal_reference_idc >> 5;
                    fu_ind->TYPE = 28;
                    //设置FU HEADER,并将这个HEADER填入sendbuf[13]
                    fu_hdr = (FU_HEADER *) &sendbuf[13];
                    fu_hdr->R = 0;
                    fu_hdr->S = 0;
                    fu_hdr->TYPE = nalu_t->nal_unit_type;
                    fu_hdr->E = 1;
                    nalu_payload = &sendbuf[14];//同理将sendbuf[14]的地址赋给nalu_payload
                    memcpy(nalu_payload, nalu_t->buf + t * 1400 + 1,
                           1400);//将nalu最后剩余的l-1(去掉了一个字节的NALU头)字节内容写入sendbuf[14]开始的字符串。
                    bytes = 1400;      //获得sendbuf的长度,为剩余nalu的长度l-1加上rtp_header，FU_INDICATOR,FU_HEADER三个包头共14字节
//                send(socket1, sendbuf, bytes, 0);//发送rtp包
                    sendto(sd, sendbuf, bytes, 0,
                           (struct sockaddr *) &(addr_dst),
                           sizeof(sockaddr_in));
                } else if (t < k - 1) {
                    //设置rtp M 位；
                    rtp_hdr->marker = 0;
                    //设置FU INDICATOR,并将这个HEADER填入sendbuf[12]
                    fu_ind = (FU_INDICATOR *) &sendbuf[12]; //将sendbuf[12]的地址赋给fu_ind，之后对fu_ind的写入就将写入sendbuf中；
                    fu_ind->F = nalu_t->forbidden_bit;
                    fu_ind->NRI = nalu_t->nal_reference_idc >> 5;
                    fu_ind->TYPE = 28;

                    //设置FU HEADER,并将这个HEADER填入sendbuf[13]
                    fu_hdr = (FU_HEADER *) &sendbuf[13];

                    fu_hdr->R = 0;
                    fu_hdr->S = 0;
                    fu_hdr->E = 0;
                    fu_hdr->TYPE = nalu_t->nal_unit_type;

                    nalu_payload = &sendbuf[14];//同理将sendbuf[14]的地址赋给nalu_payload
                    memcpy(nalu_payload, nalu_t->buf + t * 1400 + 1,
                           1400);//去掉起始前缀的nalu剩余内容写入sendbuf[14]开始的字符串。
                    bytes = 1400 +
                            14;                        //获得sendbuf的长度,为nalu的长度（除去原NALU头）加上rtp_header，fu_ind，fu_hdr的固定长度14字节
                    sendto(sd, sendbuf, bytes, 0, (struct sockaddr *) &(addr_dst),
                           sizeof(sockaddr_in));
                }
            }
        } else {
            for (t = 0; t < k + 1; t++) {
                rtp_hdr->seq_no = htons(seq_num++); //序列号，每发送一个RTP包增1
                if (!t)//发送一个需要分片的NALU的第一个分片，置FU HEADER的S位,t = 0时进入此逻辑。
                {
                    //设置rtp M 位；
                    rtp_hdr->marker = 0;  //最后一个NALU时，该值设置成1，其他都设置成0。
                    //设置FU INDICATOR,并将这个HEADER填入sendbuf[12]
                    fu_ind = (FU_INDICATOR *) &sendbuf[12]; //将sendbuf[12]的地址赋给fu_ind，之后对fu_ind的写入就将写入sendbuf中；
                    fu_ind->F = nalu_t->forbidden_bit;
                    fu_ind->NRI = nalu_t->nal_reference_idc >> 5;
                    fu_ind->TYPE = 28;  //FU-A类型。

                    //设置FU HEADER,并将这个HEADER填入sendbuf[13]
                    fu_hdr = (FU_HEADER *) &sendbuf[13];
                    fu_hdr->E = 0;
                    fu_hdr->R = 0;
                    fu_hdr->S = 1;
                    fu_hdr->TYPE = nalu_t->nal_unit_type;

                    nalu_payload = &sendbuf[14];//同理将sendbuf[14]赋给nalu_payload
                    memcpy(nalu_payload, nalu_t->buf + 1, 1400);//去掉NALU头，每次拷贝1400个字节。

                    bytes = 1400 +
                            14;//获得sendbuf的长度,为nalu的长度（除去起始前缀和NALU头）加上rtp_header，fu_ind，fu_hdr的固定长度                                                            14字节
                    sendto(sd, sendbuf, bytes, 0, (struct sockaddr *) &(addr_dst),
                           sizeof(sockaddr_in));
                } else if (t != 0 && t == k)//发送的是最后一个分片
                {                //设置rtp M 位；当前传输的是最后一个分片时该位置1
                    rtp_hdr->marker = 1;
                    //设置FU INDICATOR,并将这个HEADER填入sendbuf[12]
                    fu_ind = (FU_INDICATOR *) &sendbuf[12]; //将sendbuf[12]的地址赋给fu_ind，之后对fu_ind的写入就将写入sendbuf中；
                    fu_ind->F = nalu_t->forbidden_bit;
                    fu_ind->NRI = nalu_t->nal_reference_idc >> 5;
                    fu_ind->TYPE = 28;
                    //设置FU HEADER,并将这个HEADER填入sendbuf[13]
                    fu_hdr = (FU_HEADER *) &sendbuf[13];
                    fu_hdr->R = 0;
                    fu_hdr->S = 0;
                    fu_hdr->TYPE = nalu_t->nal_unit_type;
                    fu_hdr->E = 1;
                    nalu_payload = &sendbuf[14];//同理将sendbuf[14]的地址赋给nalu_payload
                    memcpy(nalu_payload, nalu_t->buf + t * 1400 + 1,
                           last);//将nalu最后剩余的l-1(去掉了一个字节的NALU头)字节内容写入sendbuf[14]开始的字符串。
                    bytes = last +
                            14;      //获得sendbuf的长度,为剩余nalu的长度l-1加上rtp_header，FU_INDICATOR,FU_HEADER三个包头共14字节
//                send(socket1, sendbuf, bytes, 0);//发送rtp包
                    sendto(sd, sendbuf, bytes, 0,
                           (struct sockaddr *) &(addr_dst),
                           sizeof(sockaddr_in));
                } else if (t < k) {
                    //设置rtp M 位；
                    rtp_hdr->marker = 0;
                    //设置FU INDICATOR,并将这个HEADER填入sendbuf[12]
                    fu_ind = (FU_INDICATOR *) &sendbuf[12]; //将sendbuf[12]的地址赋给fu_ind，之后对fu_ind的写入就将写入sendbuf中；
                    fu_ind->F = nalu_t->forbidden_bit;
                    fu_ind->NRI = nalu_t->nal_reference_idc >> 5;
                    fu_ind->TYPE = 28;

                    //设置FU HEADER,并将这个HEADER填入sendbuf[13]
                    fu_hdr = (FU_HEADER *) &sendbuf[13];

                    fu_hdr->R = 0;
                    fu_hdr->S = 0;
                    fu_hdr->E = 0;
                    fu_hdr->TYPE = nalu_t->nal_unit_type;

                    nalu_payload = &sendbuf[14];//同理将sendbuf[14]的地址赋给nalu_payload
                    memcpy(nalu_payload, nalu_t->buf + t * 1400 + 1,
                           1400);//去掉起始前缀的nalu剩余内容写入sendbuf[14]开始的字符串。
                    bytes = 1400 +
                            14;                        //获得sendbuf的长度,为nalu的长度（除去原NALU头）加上rtp_header，fu_ind，fu_hdr的固定长度14字节
                    sendto(sd, sendbuf, bytes, 0, (struct sockaddr *) &(addr_dst),
                           sizeof(sockaddr_in));
                }
            }
        }
//        while (t < k) {
//            rtp_hdr->seq_no = htons(seq_num++); //序列号，每发送一个RTP包增1
//            if (!t)//发送一个需要分片的NALU的第一个分片，置FU HEADER的S位,t = 0时进入此逻辑。
//            {
//                //设置rtp M 位；
//                rtp_hdr->marker = 0;  //最后一个NALU时，该值设置成1，其他都设置成0。
//                //设置FU INDICATOR,并将这个HEADER填入sendbuf[12]
//                fu_ind = (FU_INDICATOR *) &sendbuf[12]; //将sendbuf[12]的地址赋给fu_ind，之后对fu_ind的写入就将写入sendbuf中；
//                fu_ind->F = nalu_t->forbidden_bit;
//                fu_ind->NRI = nalu_t->nal_reference_idc >> 5;
//                fu_ind->TYPE = 28;  //FU-A类型。
//
//                //设置FU HEADER,并将这个HEADER填入sendbuf[13]
//                fu_hdr = (FU_HEADER *) &sendbuf[13];
//                fu_hdr->E = 0;
//                fu_hdr->R = 0;
//                fu_hdr->S = 1;
//                fu_hdr->TYPE = nalu_t->nal_unit_type;
//
//                nalu_payload = &sendbuf[14];//同理将sendbuf[14]赋给nalu_payload
//                memcpy(nalu_payload, nalu_t->buf + 1, 1400);//去掉NALU头，每次拷贝1400个字节。
//
//                bytes = 1400 +
//                        14;//获得sendbuf的长度,为nalu的长度（除去起始前缀和NALU头）加上rtp_header，fu_ind，fu_hdr的固定长度                                                            14字节
////                send(socket1, sendbuf, bytes, 0);//发送rtp包
//                sendto(sd, sendbuf, bytes, 0, (struct sockaddr *) &(addr_dst),
//                       sizeof(sockaddr_in));
//                t++;
//
//            }
//                //发送一个需要分片的NALU的非第一个分片，清零FU HEADER的S位，如果该分片是该NALU的最后一个分片，置FU HEADER的E位
//            else if (t == k - 1)//发送的是最后一个分片
//            {                //设置rtp M 位；当前传输的是最后一个分片时该位置1
//                rtp_hdr->marker = 1;
//                //设置FU INDICATOR,并将这个HEADER填入sendbuf[12]
//                fu_ind = (FU_INDICATOR *) &sendbuf[12]; //将sendbuf[12]的地址赋给fu_ind，之后对fu_ind的写入就将写入sendbuf中；
//                fu_ind->F = nalu_t->forbidden_bit;
//                fu_ind->NRI = nalu_t->nal_reference_idc >> 5;
//                fu_ind->TYPE = 28;
//
//                //设置FU HEADER,并将这个HEADER填入sendbuf[13]
//                fu_hdr = (FU_HEADER *) &sendbuf[13];
//                fu_hdr->R = 0;
//                fu_hdr->S = 0;
//                fu_hdr->TYPE = nalu_t->nal_unit_type;
//                fu_hdr->E = 1;
//
//                nalu_payload = &sendbuf[14];//同理将sendbuf[14]的地址赋给nalu_payload
//                memcpy(nalu_payload, nalu_t->buf + (t - 1) * 1400 + 1,
//                       last - 1);//将nalu最后剩余的l-1(去掉了一个字节的NALU头)字节内容写入sendbuf[14]开始的字符串。
//                bytes = last - 1 +
//                        14;      //获得sendbuf的长度,为剩余nalu的长度l-1加上rtp_header，FU_INDICATOR,FU_HEADER三个包头共14字节
////                send(socket1, sendbuf, bytes, 0);//发送rtp包
//                sendto(sd, sendbuf, bytes, 0,
//                       (struct sockaddr *) &(addr_dst),
//                       sizeof(sockaddr_in));
//                t++;
//                //Sleep(100);
//            }
//                //既不是第一个分片，也不是最后一个分片的处理。
//            else if (t < k - 1) {
//                //设置rtp M 位；
//                rtp_hdr->marker = 0;
//                //设置FU INDICATOR,并将这个HEADER填入sendbuf[12]
//                fu_ind = (FU_INDICATOR *) &sendbuf[12]; //将sendbuf[12]的地址赋给fu_ind，之后对fu_ind的写入就将写入sendbuf中；
//                fu_ind->F = nalu_t->forbidden_bit;
//                fu_ind->NRI = nalu_t->nal_reference_idc >> 5;
//                fu_ind->TYPE = 28;
//
//                //设置FU HEADER,并将这个HEADER填入sendbuf[13]
//                fu_hdr = (FU_HEADER *) &sendbuf[13];
//
//                fu_hdr->R = 0;
//                fu_hdr->S = 0;
//                fu_hdr->E = 0;
//                fu_hdr->TYPE = nalu_t->nal_unit_type;
//
//                nalu_payload = &sendbuf[14];//同理将sendbuf[14]的地址赋给nalu_payload
//                memcpy(nalu_payload, nalu_t->buf + t * 1400 + 1,
//                       1400);//去掉起始前缀的nalu剩余内容写入sendbuf[14]开始的字符串。
//                bytes = 1400 +
//                        14;                        //获得sendbuf的长度,为nalu的长度（除去原NALU头）加上rtp_header，fu_ind，fu_hdr的固定长度14字节
////                send(socket1, sendbuf, bytes, 0);//发送rtp包
//                sendto(sd, sendbuf, bytes, 0, (struct sockaddr *) &(addr_dst),
//                       sizeof(sockaddr_in));
//                t++;
//            }
//        }
    }
    FreeNALU(nalu_t);
    return 0;
}

static int FindStartCode2(char *Buf) {
    if (Buf[0] != 0 || Buf[1] != 0 || Buf[2] != 1) return 0; //判断是否为0x000001,如果是返回1
    else return 1;
}

static int FindStartCode3(char *Buf) {
    if (Buf[0] != 0 || Buf[1] != 0 || Buf[2] != 0 || Buf[3] != 1)
        return 0;//判断是否为0x00000001,如果是返回1
    else return 1;
}

JNIEXPORT void JNICALL
Java_com_saka_recordudp_SendRTPLib_initSocket(JNIEnv *env, jobject instance, jstring addr_,
                                              jint port) {
    const char *addr = env->GetStringUTFChars(addr_, 0);
    LOGE("地址是:%s", addr);
    LOGE("端口是:%d", port);
    if ((sd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        LOGE("连接错误");
    }
    addr_dst.sin_family = AF_INET;
    addr_dst.sin_addr.s_addr = inet_addr(addr);
    addr_dst.sin_port = htons(5555);
    LOGE("开始发送");
    sendto(sd, addr, strlen(addr), 0, (struct sockaddr *) &(addr_dst), sizeof(sockaddr_in));
    timestamp_increse = (unsigned int) (90000.0 /
                                        framerate); //+0.5);  //时间戳，H264的视频设置成90000
    env->ReleaseStringUTFChars(addr_, addr);
}


JNIEXPORT void JNICALL
Java_com_saka_recordudp_SendRTPLib_sendData(JNIEnv *env, jobject instance, jbyteArray data_,
                                            jlong timeStamp) {
    jbyte *data = env->GetByteArrayElements(data_, NULL);
    jsize length = env->GetArrayLength(data_);
    if (data == NULL) {
        LOGE("error,data is null");
    }
    char *buf = (char *) data;

    sendData(buf, length, timeStamp);
    env->ReleaseByteArrayElements(data_, data, 0);
}


JNIEXPORT void JNICALL
Java_com_saka_recordudp_SendRTPLib_testData(JNIEnv *env, jobject instance, jintArray datas_,
                                            jint size) {
    jint *datas = env->GetIntArrayElements(datas_, NULL);
    LOGE("得到的数据%d", *(datas + 1));
    env->ReleaseIntArrayElements(datas_, datas, 0);
}


JNIEXPORT void JNICALL
Java_com_saka_recordudp_SendRTPLib_releaseSocket(JNIEnv *env, jobject instance) {
    if (shutdown(sd, 1) == -1) {
        LOGE("关闭出错");
    }
}

#ifdef __cplusplus
}
#endif