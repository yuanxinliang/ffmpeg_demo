//
//  VideoEncode.c
//  VideoEncode
//
//  Created by jdapple on 2020/4/26.
//  Copyright © 2020 jdapple. All rights reserved.
//

#include "VideoEncode.h"
#include "libavutil/avutil.h"
#include "libavdevice/avdevice.h"
#include "libavformat/avformat.h"

#define Width   1280
#define Height  720

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

// 打开编码器
static void open_encoder(int width, int height, AVCodecContext **enc_ctx) {
    AVCodec *codec = NULL;
    codec = avcodec_find_encoder_by_name("libx264");
    if (!codec) {
        printf("codec libx264 not found\n");
        exit(1);
    }
    *enc_ctx = avcodec_alloc_context3(codec);
    if (!enc_ctx) {
        printf("could not allocate video codec context!\n");
        exit(1);
    }
    // SPS/PPS
    (*enc_ctx)->profile = FF_PROFILE_H264_HIGH_444;
    (*enc_ctx)->level = 50; // 表示LEVEL是5.0
    
    // 设置分辨率
    (*enc_ctx)->width = width;
    (*enc_ctx)->height = height;
    
    // GOP
    (*enc_ctx)->gop_size = 250;
    (*enc_ctx)->keyint_min = 25; // option
    
    // 设置B帧数据
    (*enc_ctx)->max_b_frames = 3; // option
    (*enc_ctx)->has_b_frames = 1; // option
    
    // 参考帧的数量
    (*enc_ctx)->refs = 3; // option
    
    // 设置输入YUV格式
    (*enc_ctx)->pix_fmt = AV_PIX_FMT_YUV420P;
    
    // 设置码率
    (*enc_ctx)->bit_rate = 600000; // 600kbps
    
    // 设置帧率
    (*enc_ctx)->time_base = (AVRational){1, 25};
    (*enc_ctx)->framerate = (AVRational){25, 1};
    
    int ret = avcodec_open2((*enc_ctx), codec, NULL);
    if (ret < 0) {
        printf("Could not open codec:%s\n", av_err2str(ret));
        exit(1);
    }
}

static AVFrame* create_frame(int width, int height) {
    int ret;
    AVFrame* frame = NULL;
    frame = av_frame_alloc();
    if (!frame) {
        printf("Error: No Memory!\n");
        goto __ERROR;
    }
    // 设置参数
    frame->width = width;
    frame->height = height;
    frame->format = AV_PIX_FMT_YUV420P;
    // alloc inner memory
    ret = av_frame_get_buffer(frame, 32);
    if (ret < 0) {
        printf("Error：failed to alloc buffer for frame!\n");
        goto __ERROR;
    }
    return frame;
__ERROR:
    return NULL;
}

static void encode(AVCodecContext *enc_ctx, AVFrame *frame, AVPacket *newPkt, FILE *outfile) {
//    if (frame) {
//        printf("send frame to encoder, pts = %lld\n", frame->pts);
//    }
    
    // 输送原始数据给编码器进行编码
    int ret = avcodec_send_frame(enc_ctx, frame);
    if (ret < 0) {
        printf("Error: failed to send a frame for encoding!\n");
    }
    while (ret >= 0) {
        ret = avcodec_receive_packet(enc_ctx, newPkt);
        // 如果编码器数据不足时会返回EAGAIN，或者到数据尾时会返回AVERROR
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return;
        } else if (ret < 0) {
            printf("Error: Failed to encode!\n");
        }
        fwrite(newPkt->data, 1, newPkt->size, outfile);
        av_packet_unref(newPkt);
    }
}

void set_record_status(int status) {
    record_status = 0;
}

void video_record(void) {
    printf("开始录制...\n");
    // 录制
    record_status = 1;
    
    AVFormatContext *fmt_ctx = NULL;
    AVCodecContext *enc_ctx = NULL;
    
    int ret = 0;
    int ptsSize = 0;
    AVPacket pkt;
    av_init_packet(&pkt);
    
    char *yuvFilePath = "/Users/jdapple/Desktop/Learnffmpeg/video/video.yuv";
    FILE *yuvOutFile = fopen(yuvFilePath, "wb+");
    
    char *filePath = "/Users/jdapple/Desktop/Learnffmpeg/video/video.h264";
    FILE *outFile = fopen(filePath, "wb+");
    
    fmt_ctx = open_device();
    open_encoder(Width, Height, &enc_ctx);
    
    // 创建Frame
    AVFrame *frame = create_frame(Width, Height);
    AVPacket *newPkt = av_packet_alloc();
    if (!newPkt) {
        printf("Error, Failed to alloc avpacket!\n");
        goto __ERROR;
    }
    
    while (record_status) {
        ret = av_read_frame(fmt_ctx, &pkt);
        if (ret == AVERROR(EAGAIN)) {
            continue;
        } else if (ret == 0) {
//            printf("size: %d - [%p]\n", pkt.size, pkt.data);
            
            // NV12 转 YUV420
            // YYYYYYYYUVVU NV12
            // YYYYYYYYUUVV YUV420
            memcpy(frame->data[0], pkt.data, Width * Height); // copy y data
            // 1280 * 720之后，是UV
            for (int i = 0; i < Width * Height / 4; i++) {
                frame->data[1][i] = pkt.data[Width * Height + i * 2];
                frame->data[2][i] = pkt.data[Width * Height + i * 2 + 1];
            }
            fwrite(frame->data[0], 1, Width * Height, yuvOutFile);
            fwrite(frame->data[1], 1, Width * Height / 4, yuvOutFile);
            fwrite(frame->data[2], 1, Width * Height / 4, yuvOutFile);
            frame->pts = ptsSize++;
            encode(enc_ctx, frame, newPkt, outFile);
            
            av_packet_unref(&pkt);
        }
    }
    
    encode(enc_ctx, NULL, newPkt, outFile);
    fclose(yuvOutFile);
    fclose(outFile);
    avformat_close_input(&fmt_ctx);
    avcodec_free_context(&enc_ctx);
    
__ERROR:
    printf("录制结束...\n");
}
