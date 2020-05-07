//
//  PushSteam.c
//  PushStream
//
//  Created by jdapple on 2020/4/30.
//  Copyright © 2020 jdapple. All rights reserved.
//

#include "PushSteam.h"
#include "librtmp/rtmp.h"
#include <stdlib.h>
#include <unistd.h>

// 1
static FILE* open_flv(char *addr) {
    FILE *fq = NULL;
    fq = fopen(addr, "rb");
    if (!fq) {
        printf("Error: failed to open flv.\n");
        return NULL;
    }
    fseek(fq, 9, SEEK_SET); // 跳过 9 字节的 FLV Header
    fseek(fq, 4, SEEK_CUR); // 跳过 4 字节的PreTagSize
    return fq;
}

// 2
static RTMP* connect_rtmp_server(char *addr) {
    RTMP *rtmp = NULL;
    
    // 1.创建RTMP对象,并进行初始化
    rtmp = RTMP_Alloc();
    if (!rtmp) {
        printf("Error: failed to rtmp alloc.\n");
        return NULL;
    }
    RTMP_Init(rtmp);
    
    // 2.设置RTMP服务地址，以及设置连接超时间
    rtmp->Link.timeout = 10;
    RTMP_SetupURL(rtmp, addr);
    
    // 3.设置推流 - 如果未设置，默认是拉流
    RTMP_EnableWrite(rtmp);
    
    // 4.建立连接
    int ret = 0;
    ret = RTMP_Connect(rtmp, NULL);
    if (ret <= 0) {
        printf("Error: failed to connect RTMP.\n");
        goto __ERROR;
    }
    RTMP_ConnectStream(rtmp, 0);
    return rtmp;
    
__ERROR:
    if (rtmp) {
        RTMP_Close(rtmp);
        RTMP_Free(rtmp);
    }
    
    return rtmp;
}

// 分配RTMPPacket空间
static RTMPPacket* alloc_packet() {
    RTMPPacket *pack = NULL;
    pack = (RTMPPacket *)malloc(sizeof(RTMPPacket));
    if (!pack) {
        printf("Error: failed to RTMPPacket malloc.\n");
        return NULL;
    }
    if (!RTMPPacket_Alloc(pack, 64 * 1024)) {
        printf("Error: failed to RTMPPacket alloc.\n");
        return NULL;
    }
    RTMPPacket_Reset(pack);
    pack->m_hasAbsTimestamp = 0;
    pack->m_nChannel = 0x4;
    return pack;
}

static int read_u8(FILE *fq, unsigned int *value) {
    unsigned int temp;
    if (fread(&temp, 1, 1, fq) != 1) {
        printf("Error: failed to read u8.\n");
        return 0;
    }
    *value = temp & 0xFF;
    return 1;
}

static int read_u24(FILE *fq, unsigned int *value) {
    unsigned int temp;
    if (fread(&temp, 1, 3, fq) != 3) {
        printf("Error: failed to read u24.\n");
        return 0;
    }
    *value = ((temp >> 16) & 0xFF) | ((temp << 16) & 0xFF0000) | (temp & 0xFF00);
    return 1;
}

static int read_u32(FILE *fq, unsigned int *value) {
    unsigned int temp;
    if (fread(&temp, 1, 4, fq) != 4) {
        printf("Error: failed to read u32.\n");
        return 0;
    }
    *value = ((temp >> 24) & 0xFF) | ((temp >> 8) & 0xFF00) | ((temp << 8) & 0xFF00) | ((temp << 24) & 0xFF000000);
    return 1;
}

static int read_ts(FILE *fq, unsigned int *value) {
    unsigned int temp;
    if (fread(&temp, 1, 4, fq) != 4) {
        printf("Error: failed to read ts.\n");
        return 0;
    }
    *value = ((temp >> 16) & 0xFF) | ((temp << 16) & 0xFF0000) | (temp & 0xFF00) | (temp & 0xFF000000);
    return 1;
}

static int read_data(FILE *file, RTMPPacket **packet) {
    /*
     * tag header
     * 第一个字节 TT（Tag Type）, 0x08 音频，0x09 视频， 0x12 script
     * 2-4, Tag body 的长度， PreTagSize - Tag Header size
     * 5-7, 时间戳，单位是毫秒; script 它的时间戳是0
     * 第8个字节，扩展时间戳。真正时间戳结格 [扩展，时间戳] 一共是4字节。
     * 9-11, streamID, 0
     */
    
    /*
     * flv
     * flv header(9), tagpresize, tag(header+data), tagpresize
     */
    
    size_t datasize = 0;
    
    unsigned int tt = 0; // 1
    unsigned int tag_data_size = 0; // 3
    unsigned int ts = 0; // 4
    unsigned int stream_id = 0; // 3
    unsigned int tag_pre_size;
    
    if (read_u8(file, &tt) != 1) {
        printf("Error: failed to read Tag Type.\n");
        return -1;
    }
    
    if (read_u24(file, &tag_data_size) != 1) {
        printf("Error: failed to read Tag body length.\n");
        return -1;
    }
    
    if (read_ts(file, &ts) != 1) {
        printf("Error: failed to read times.\n");
        return -1;
    }
    
    if (read_u24(file, &stream_id) != 1) {
        printf("Error: failed to read streamID.\n");
        return -1;
    }
    
    printf("tag header, ts: %u, tt: %d, datasize:%d \n", ts, tt, tag_data_size);
    
    datasize = fread((*packet)->m_body, 1, tag_data_size, file);
    if (datasize != tag_data_size) {
        return -1;
    }
    
    // 设置pakcet数据
    (*packet)->m_headerType = RTMP_PACKET_SIZE_LARGE;
    (*packet)->m_nTimeStamp = ts;
    (*packet)->m_packetType = tt;
    (*packet)->m_nBodySize = tag_data_size;
    
    read_u32(file, &tag_pre_size);

    return 1;
}

// 3
static void send_data(FILE *file, RTMP *rtmp) {
    // 1.创建RTMPPACKET
    RTMPPacket *packet = NULL;
    packet = alloc_packet();
    packet->m_nInfoField2 = rtmp->m_stream_id;
    
    unsigned int pre_ts = 0;
    
    while (1) {
        
        // 2.从flv文件中读取数据
        if (read_data(file, &packet) != 1) {
            printf("over...\n");
            break;
        }
        
        // 3.判断RTMP连接是否正常
        if (!RTMP_IsConnected(rtmp)) {
            printf("Error: RTMP is not connect.\n");
            break;
        }
        
        // 延迟发送数据的时间点
        unsigned int diff = packet->m_nTimeStamp - pre_ts;
        usleep(diff * 1000);
        
        // 4.发送数据
        RTMP_SendPacket(rtmp, packet, 0);
        
        pre_ts = packet->m_nTimeStamp;
    }
    
}

void push_stream() {
    
    printf("push stream...\n");
    
    char *flv_addr  = "/Users/jdapple/Desktop/666.flv";
    char *rtmp_addr = "rtmp://localhost/live/room";
    
    // 1.打开flv文件
    FILE *file = open_flv(flv_addr);
    
    // 2.连接RTMP服务器
    RTMP *rtmp = connect_rtmp_server(rtmp_addr);
    
    // 3.向流媒体服务器推流
    send_data(file, rtmp);
    
    // 4.释放内存
//    fclose(file);
//    RTMP_Close(rtmp);
//    RTMP_Free(rtmp);
    
    printf("Finish...\n");
}
