//
//  AudioResample.c
//  AudioResample
//
//  Created by jdapple on 2020/4/22.
//  Copyright © 2020 jdapple. All rights reserved.
//

#include "AudioResample.h"
#include "libavutil/avutil.h"
#include "libavdevice/avdevice.h"
#include "libavformat/avformat.h"
#include "libswresample/swresample.h"

static int record_status = 0;

void verify(void) {
    av_log_set_level(AV_LOG_DEBUG);
    av_log(NULL, AV_LOG_DEBUG, "Hello ffmpeg!\n");
}

void set_record_status(int status) {
    record_status = 0;
}

void audio_record(void){
    
    // 1.注册设备
    avdevice_register_all();
    
    // 2.设置采集方式
    AVInputFormat *inputFormat = av_find_input_format("avfoundation");
    
    // 3.打开设备
    AVFormatContext *fmt_ctx = NULL;
    char *deviceName = ":0";
    AVDictionary *option = NULL;
    
    int ret = avformat_open_input(&fmt_ctx, deviceName, inputFormat, &option);
    if (ret < 0) {
        av_log(NULL, AV_LOG_DEBUG, "打开设备失败");
        return;
    } else {
        av_log(NULL, AV_LOG_DEBUG, "成功打开设备");
    }
    
    
    // 4.读取数据
    
    AVPacket pkt;
    av_init_packet(&pkt);
    record_status = 1;
    
    char *outPath = "/Users/jdapple/Desktop/Learnffmpeg/audio/1.pcm";
    FILE *outFile = fopen(outPath, "wb+");
    
    // 创建重采样上下文
    SwrContext *swr_ctx = NULL;
    swr_ctx = swr_alloc_set_opts(NULL,
                                 <#int64_t out_ch_layout#>,
                                 <#enum AVSampleFormat out_sample_fmt#>,
                                 <#int out_sample_rate#>,
                                 <#int64_t in_ch_layout#>,
                                 <#enum AVSampleFormat in_sample_fmt#>,
                                 <#int in_sample_rate#>,
                                 0,
                                 NULL);
    
    
    while (record_status) {
        ret = av_read_frame(fmt_ctx, &pkt);
        if (ret == AVERROR(EAGAIN)) {
            continue;
        } else if (ret == 0) {
            fwrite(pkt.data, pkt.size, 1, outFile);
            fflush(outFile);
            av_log(NULL, AV_LOG_DEBUG, "读取数据 - pack size：%d【%p】\n", pkt.size, pkt.data);
        }
        av_packet_unref(&pkt);
    }
    
    fclose(outFile);
    avformat_close_input(&fmt_ctx);
    av_log(NULL, AV_LOG_DEBUG, "结束录制");
}
