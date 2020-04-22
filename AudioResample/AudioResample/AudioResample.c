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
    
    char *outPath = "/Users/jdapple/Desktop/Learnffmpeg/audio/audio.pcm";
    FILE *outFile = fopen(outPath, "wb+");
    
    // 创建重采样上下文
    SwrContext *swr_ctx = NULL;
    swr_ctx = swr_alloc_set_opts(NULL,                  // ctx
                                 AV_CH_LAYOUT_STEREO,   // 输出channel布局 - 通道
                                 AV_SAMPLE_FMT_S16,     // 输出的采样格式
                                 44100,                 // 输出的采样率
                                 AV_CH_LAYOUT_STEREO,   // 输入channel布局 - 通道
                                 AV_SAMPLE_FMT_FLT,     // 输入的采样格式
                                 44100,                 // 输入的采样率
                                 0,
                                 NULL);
    
    // 异常处理
    if (!swr_ctx) {
        av_log(NULL, AV_LOG_DEBUG, "swr_ctx is null.");
    }
    
    if (swr_init(swr_ctx) < 0) {
        av_log(NULL, AV_LOG_DEBUG, "swr_ctx init fail.");
    }
    
    // 设置参数
    
    uint8_t **dst_data = NULL;
    int dst_linesize = 0;
    
    uint8_t **src_data = NULL;
    int src_linesize = 0;
    
    // 4096/4 = 1024/2 = 512
    // 创建输出缓冲区
    av_samples_alloc_array_and_samples(&dst_data,           // 输出缓冲区的地址
                                       &dst_linesize,       // 输出缓冲区的大小
                                       2,                   // 通道数
                                       512,                 // 每个通道采样个数
                                       AV_SAMPLE_FMT_S16,   // 音频采样格式
                                       0);
    
    // 创建输入缓冲区
    av_samples_alloc_array_and_samples(&src_data,           // 输入缓冲区的地址
                                       &src_linesize,       // 输入缓冲区的大小
                                       2,                   // 通道数
                                       512,                 // 每个通道采样个数
                                       AV_SAMPLE_FMT_FLT,   // 音频采样格式
                                       0);
    
    while (record_status) {
        ret = av_read_frame(fmt_ctx, &pkt);
        if (ret == AVERROR(EAGAIN)) {
            continue;
        } else if (ret == 0) {
            av_log(NULL, AV_LOG_DEBUG, "读取数据 - pack size：%d【%p】\n", pkt.size, pkt.data);
            // 进行内存拷贝，按字节拷贝
            memcpy((void *)src_data[0], (void *)pkt.data, pkt.size);
            // 重采样
            swr_convert(swr_ctx,                    // 重采样的上下文
                        dst_data,                   // 输出缓冲区
                        512,                        // 输出每个通道的采样数
                        (const uint8_t **)src_data, // 输入缓冲区
                        512);                       // 输入每个通道的采样数
            fwrite(dst_data[0], 1, dst_linesize, outFile);
            fflush(outFile);
        }
        av_packet_unref(&pkt);
    }
    
    // close file
    fclose(outFile);
    
    // 释放输出缓冲区
    if (dst_data) {
        av_freep(&dst_data[0]);
    }
    av_freep(&dst_data);
    
    // 释放输入缓冲区
    if (src_data) {
        av_freep(&src_data[0]);
    }
    av_freep(&src_data);
    
    // 释放上下文
    swr_free(&swr_ctx);
    
    avformat_close_input(&fmt_ctx);
    
    av_log(NULL, AV_LOG_DEBUG, "结束录制");
}
