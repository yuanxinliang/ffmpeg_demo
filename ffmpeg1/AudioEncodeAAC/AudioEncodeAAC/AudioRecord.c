//
//  AudioRecord.c
//  AudioEncodeAAC
//
//  Created by jdapple on 2020/4/24.
//  Copyright © 2020 jdapple. All rights reserved.
//

#include "AudioRecord.h"
#include "libavutil/avutil.h"
#include "libavdevice/avdevice.h"
#include "libavformat/avformat.h"
#include "libswresample/swresample.h"

// MARK: -- Private --

static int record_status = 0;

// 打开设备
static AVFormatContext* open_device() {
    // 1.注册设备
    avdevice_register_all();
    
    // 2.设置采集方式
    AVInputFormat *inputFormat = av_find_input_format("avfoundation");
    
    AVFormatContext *fmt_ctx = NULL;
    char *deviceName = ":0";
    AVDictionary *option = NULL;
    
    // 3.打开设备
    int ret = avformat_open_input(&fmt_ctx, deviceName, inputFormat, &option);
    if (ret < 0) {
        av_log(NULL, AV_LOG_DEBUG, "打开设备失败\n");
        return NULL;
    }
    av_log(NULL, AV_LOG_DEBUG, "成功打开设备\n");
    return fmt_ctx;
}

// 重采样上下文
static SwrContext* create_swr_ctx() {
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
        return NULL;
    }
    
    if (swr_init(swr_ctx) < 0) {
        av_log(NULL, AV_LOG_DEBUG, "swr_ctx init fail.");
        return NULL;
    }
    return swr_ctx;
}

// 打开编码器
static AVCodecContext* open_avcodec() {
    // 创建编码器
    // AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    AVCodec *codec = avcodec_find_encoder_by_name("libfdk_aac");
    // 创建codec上下文
    AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
    codec_ctx->sample_fmt = AV_SAMPLE_FMT_S16;          // 输入音频的采样大小
    codec_ctx->channel_layout = AV_CH_LAYOUT_STEREO;    // 输入音频的channel layout
    codec_ctx->channels = 2;                            // 输入音频的channel个数
    codec_ctx->sample_rate = 44100;                     // 输入音频的采样率
    codec_ctx->bit_rate = 0;                            // AAC_LC:128k, AAC HE:64k, AAC HE V2:32k
    codec_ctx->profile = FF_PROFILE_AAC_HE;          // 如果设置profile，则bit_rate应该设为0
    // 打开编码器
    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        return NULL;
    }
    return codec_ctx;
}

// 创建重采样的输入输出数据
static void alloc_out_in_buffer(uint8_t ***dst_data, int *dst_linesize, uint8_t ***src_data, int *src_linesize) {
    // 4096/4 = 1024/2 = 512
    // 创建输出缓冲区
    av_samples_alloc_array_and_samples(dst_data,           // 输出缓冲区的地址
                                       dst_linesize,       // 输出缓冲区的大小
                                       2,                   // 通道数
                                       512,                 // 每个通道采样个数
                                       AV_SAMPLE_FMT_S16,   // 音频采样格式
                                       0);
    
    // 创建输入缓冲区
    av_samples_alloc_array_and_samples(src_data,           // 输入缓冲区的地址
                                       src_linesize,       // 输入缓冲区的大小
                                       2,                   // 通道数
                                       512,                 // 每个通道采样个数
                                       AV_SAMPLE_FMT_FLT,   // 音频采样格式
                                       0);
}

// 释放重采样的输入输出数据
static void release_out_in_buffer(uint8_t **dst_data, uint8_t **src_data) {
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
}

static AVFrame* create_frame() {
    // 音频输入数据
    AVFrame *frame = av_frame_alloc();
    if (!frame) {
        // TODO
    }
    frame->nb_samples = 512;
    frame->format = AV_SAMPLE_FMT_S16;
    frame->channel_layout = AV_CH_LAYOUT_STEREO;
    av_frame_get_buffer(frame, 0);
    if (!frame->data[0]) {
        // TODO
    }
    return frame;
}

static AVPacket* create_packet() {
    return av_packet_alloc();
}

// 编码，将pcm数据编码成aac数据
static void encode(AVCodecContext *codec_ctx, AVFrame *frame, AVPacket *pkt, FILE *outFile) {
    int ret = 0;
    // 将数据送到编码器
    ret = avcodec_send_frame(codec_ctx, frame);
    // 如果ret >= 0，则说明数据读取成功
    while (ret >= 0) {
        // 获取编码后的音频数据, 如果成功，需要重复获取，知道失败为止
        ret = avcodec_receive_packet(codec_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            av_log(NULL, AV_LOG_DEBUG, "Error, encoding audio frame\n");
            exit(-1);
        }
        fwrite(pkt->data, 1, pkt->size, outFile);
        fflush(outFile);
    }
}

// MARK: -- Public --

void set_record_status(int status) {
    record_status = 0;
}

void audio_record(void){
    
    record_status = 1;
    int ret = 0;
    
    // 上下文变量
    AVFormatContext *fmt_ctx = NULL;
    SwrContext *swr_ctx = NULL;
    AVCodecContext *codec_ctx = NULL;
    
    // 重采样数据缓冲区变量
    uint8_t **dst_data = NULL;
    int dst_linesize = 0;
    uint8_t **src_data = NULL;
    int src_linesize = 0;
    
    AVFrame *frame = NULL;
    AVPacket *newPkt = NULL;
    
    AVPacket pkt;
    
    char *outPath = "/Users/jdapple/Desktop/Learnffmpeg/audio/audio.aac";
    FILE *outFile = fopen(outPath, "wb+");
    
    // 打开设备上下文
    fmt_ctx = open_device();
    if (!fmt_ctx) {
        av_log(NULL, AV_LOG_DEBUG, "Error: Failed to open device!\n");
        goto __ERROR;
    }
    // 重采样上下文
    swr_ctx = create_swr_ctx();
    if (!swr_ctx) {
        av_log(NULL, AV_LOG_DEBUG, "Error: Failed to create swr_ctx!\n");
        goto __ERROR;
    }
    // 重采样数据缓冲区
    alloc_out_in_buffer(&dst_data, &dst_linesize, &src_data, &src_linesize);
    
    // 编码器上下文
    codec_ctx = open_avcodec();
    if (!codec_ctx) {
        av_log(NULL, AV_LOG_DEBUG, "Error: Failed to open codec!\n");
        goto __ERROR;
    }
    // 编码数据
    frame = create_frame();
    if (!frame) {
        av_log(NULL, AV_LOG_DEBUG, "Error: Failed to create frame!\n");
        goto __ERROR;
    }
    // 分配编码后的数据空间
    newPkt = create_packet();
    if (!newPkt) {
        av_log(NULL, AV_LOG_DEBUG, "Error: Failed to create newPkt!\n");
        goto __ERROR;
    }
    
    av_init_packet(&pkt);
    
    // 读取数据、重采样、编码
    while (record_status) {
        ret = av_read_frame(fmt_ctx, &pkt);
        if (ret == AVERROR(EAGAIN)) {
            continue;
        } else if (ret == 0) {
            // 进行内存拷贝，按字节拷贝
            memcpy((void *)src_data[0], (void *)pkt.data, pkt.size);
            // 重采样
            swr_convert(swr_ctx,                    // 重采样的上下文
                        dst_data,                   // 输出缓冲区
                        512,                        // 输出每个通道的采样数
                        (const uint8_t **)src_data, // 输入缓冲区
                        512);                       // 输入每个通道的采样数
            // 将重采样的数据拷贝到frame中
            memcpy((void *)frame->data[0], dst_data[0], dst_linesize);
            encode(codec_ctx, frame, newPkt, outFile);
            printf("size: %d - [%p]\n", pkt.size, pkt.data);
            av_packet_unref(&pkt);
        }
    }
    
    encode(codec_ctx, NULL, newPkt, outFile);
    
__ERROR:
    
    if (outFile) {
        fclose(outFile);
    }

    // 释放重采样缓冲区
    release_out_in_buffer(dst_data, src_data);

    // 释放AVFrame和AVPacket
    if (frame) {
        av_frame_free(&frame);
    }
    if (newPkt) {
        av_packet_free(&newPkt);
    }

    // 释放编码器上下文
    if (codec_ctx) {
        avcodec_free_context(&codec_ctx);
    }
    // 释放重采样上下文
    if (swr_ctx) {
        swr_free(&swr_ctx);
    }
    // 释放上下文
    if (fmt_ctx) {
        avformat_close_input(&fmt_ctx);
    }
    
    av_log(NULL, AV_LOG_DEBUG, "结束录制");
}
