//
//  VideoRecord.c
//  VideoCaptureYUV
//
//  Created by jdapple on 2020/4/24.
//  Copyright © 2020 jdapple. All rights reserved.
//

#include "VideoRecord.h"
#include "libavutil/avutil.h"
#include "libavdevice/avdevice.h"
#include "libavformat/avformat.h"

static int record_status = 0;

// 打开设备
static AVFormatContext* open_device() {
    // 1.注册设备
    avdevice_register_all();
    // 2.设置采集方式
    AVInputFormat *inputFormat = av_find_input_format("avfoundation");
    
    AVFormatContext *fmt_ctx = NULL;
    char *deviceName = "0";
    AVDictionary *option = NULL;
    av_dict_set(&option, "video_size", "1280x720", 0);
    av_dict_set(&option, "framerate", "30", 0);
    av_dict_set(&option, "pixel_format", "nv12", 0);
    
    // 3.打开设备
    int ret = avformat_open_input(&fmt_ctx, deviceName, inputFormat, &option);
    if (ret < 0) {
        av_log(NULL, AV_LOG_DEBUG, "打开设备失败\n");
        return NULL;
    }
    av_log(NULL, AV_LOG_DEBUG, "成功打开设备\n");
    return fmt_ctx;
}

void set_record_status(int status) {
    record_status = 0;
}

void video_record(void) {
    
    AVFormatContext *fmt_ctx = open_device();
    
    record_status = 1;
    int  ret = 0;
    AVPacket pkt;
    av_init_packet(&pkt);
    
    char *filePath = "/Users/jdapple/Desktop/Learnffmpeg/video/video.yuv";
    FILE *outFile = fopen(filePath, "wb+");
    
    while (record_status) {
        ret = av_read_frame(fmt_ctx, &pkt);
        if (ret == AVERROR(EAGAIN)) {
            continue;
        } else if (ret == 0) {
            printf("size: %d - [%p]\n", pkt.size, pkt.data);
            // (宽x高)x（yuv420=1.5/yuv422=2.0/yuv444=3.0）
            fwrite(pkt.data, 1280 * 720 * 1.5, 1, outFile);
            fflush(outFile);
            av_packet_unref(&pkt);
        }
    }
    fclose(outFile);
    avformat_close_input(&fmt_ctx);
    printf("录制结束...");
}
