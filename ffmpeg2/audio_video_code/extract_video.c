#include <stdio.h>
#include <libavutil/log.h>
#include <libavformat/avio.h>
#include <libavformat/avformat.h>

#ifndef AV_WB32
#   define AV_WB32(p, val) do {                 \
        uint32_t d = (val);                     \
        ((uint8_t*)(p))[3] = (d);               \
        ((uint8_t*)(p))[2] = (d)>>8;            \
        ((uint8_t*)(p))[1] = (d)>>16;           \
        ((uint8_t*)(p))[0] = (d)>>24;           \
    } while(0)
#endif

#ifndef AV_RB16
#   define AV_RB16(x)                           \
    ((((const uint8_t*)(x))[0] << 8) |          \
      ((const uint8_t*)(x))[1])
#endif

// 添加特征码数据 - start code
static int alloc_and_copy(AVPacket *out,
                   const uint8_t *sps_pps, uint32_t sps_pps_size,
                   const uint8_t *in, uint32_t in_size)
{
    uint32_t offset             = out->size;
    uint8_t nal_header_size     = 4;
    int err;

    err = av_grow_packet(out, sps_pps_size + in_size + nal_header_size);
    if (err < 0) {
        return err;
    }

    if (sps_pps) {
        memcpy(out->data + offset, sps_pps, sps_pps_size);
    }
    memcpy(out->data + sps_pps_size + nal_header_size + offset, in, in_size);
    if (!offset) {
        AV_WB32(out->data + sps_pps_size, 1);
    } else {
        (out->data + offset + sps_pps_size)[0] =
        (out->data + offset + sps_pps_size)[1] = 0;
        (out->data + offset + sps_pps_size)[2] = 1;
    }

    return 0;
}

// 添加sps、pps数据
int h264_extradata_to_annexb(const uint8_t *codec_extradata,
                             const int codec_extradata_size,
                             AVPacket *out_extradata,
                             int padding) {
    uint16_t unit_size;
    uint64_t total_size                 = 0;
    uint8_t *out                        = NULL, unit_nb, sps_done = 0, sps_seen = 0, pps_seen = 0, sps_offset = 0, pps_offset = 0;
    const uint8_t *extradata            = codec_extradata + 4;
    static const uint8_t nalu_header[4] = { 0, 0, 0, 1 };
    int length_size = (*extradata++ & 0x3) + 1; // retrieve length coded size, 用于指示表示NALU数据长度所需占用的字节数

    sps_offset = pps_offset = -1;

    /* retrieve sps and pps unit(s) */
    unit_nb = *extradata++ & 0x1f; /* number of sps unit(s) */
    if (!unit_nb) {
        goto __pps;
    } else {
        sps_offset = 0;
        sps_seen = 1;
    }

    while (unit_nb--) {
        int err;
        unit_size = AV_RB16(extradata);
        total_size += unit_size + 4;
        if (total_size > INT_MAX - padding) {
            av_log(NULL, AV_LOG_ERROR,
            "Too big extradata size, corrupted stream or invalid MP4/AVCC bitstream\n");
            av_free(out);
            return AVERROR(EINVAL);
        }
        if (extradata + 2 + unit_size > codec_extradata + codec_extradata_size) {
            av_log(NULL, AV_LOG_ERROR, "Packet header is not contained in global extradata, "
            "corrupted stream or invalid MP4/AVCC bitstream\n");
            av_free(out);
            return AVERROR(EINVAL);
        }
        err = av_reallocp(&out, total_size + padding);
        if (err < 0) {
            return err;
        }
        memcpy(out + total_size - unit_size - 4, nalu_header, 4);
        memcpy(out + total_size - unit_size, extradata + 2, unit_size);
        extradata += 2 + unit_size;
__pps:
        if (!unit_nb && !sps_done++) {
            unit_nb = *extradata++;
            if (unit_nb) {
                pps_offset = total_size;
                pps_seen = 1;
            }
        }
    }

    if (out) {
        memset(out + total_size, 0, padding);
    }

    if (!sps_seen)
        av_log(NULL, AV_LOG_WARNING,
               "Warning: SPS NALU missing or invalid. "
               "The resulting stream may not play.\n");

    if (!pps_seen)
        av_log(NULL, AV_LOG_WARNING,
               "Warning: PPS NALU missing or invalid. "
               "The resulting stream may not play.\n");

    out_extradata->data = out;
    out_extradata->size = total_size;

    return length_size;
}

// 抽取数据（sps、pps、start code）
int h264_mp4toannexb(AVFormatContext *fmt_ctx,
                     AVPacket *in,
                     FILE *dst_fd) {

    AVPacket *out = NULL;
    AVPacket sps_pps_pkt;

    int len;
    uint8_t unit_type;
    int32_t nal_size;
    uint32_t cumul_size = 0;
    const uint8_t *buf;
    const uint8_t *buf_end;
    int buf_size;
    int ret = 0, i;

    out = av_packet_alloc();

    buf = in->data;
    buf_size = in->size;
    buf_end = in->data + in->size;

    do {
        ret = AVERROR(EINVAL);
        if (buf + 4 > buf_end) {
            goto __fail;
        }

        for (nal_size = 0, i = 0; i < 4; i++) {
            nal_size = (nal_size << 8) | buf[i];
        }
        buf += 4;
        unit_type = *buf & 0x1f;

        if ((nal_size > buf_end - buf) || nal_size < 0) {
            goto __fail;
        }

        if (unit_type == 5) {

            h264_extradata_to_annexb(fmt_ctx->streams[in->stream_index]->codec->extradata,
                                     fmt_ctx->streams[in->stream_index]->codec->extradata_size,
                                     &sps_pps_pkt,
                                     AV_INPUT_BUFFER_PADDING_SIZE);

            ret = alloc_and_copy(out,
                                 sps_pps_pkt.data, sps_pps_pkt.size,
                                 buf, nal_size);

            if (ret < 0) {
                goto __fail;
            }

        } else {

            ret = alloc_and_copy(out, NULL, 0, buf, nal_size);
            if (ret < 0) {
                goto __fail;
            }

        }

        len = fwrite(out->data, 1, out->size, dst_fd);
        if (len != out->size) {
            av_log(NULL, AV_LOG_DEBUG, "Warning, length of writed data isn't equal pkt.size(%d, %d).\n", len, out->size);
        }
        fflush(dst_fd);

        buf         += nal_size;
        cumul_size  += nal_size + 4;

    } while (cumul_size < buf_size);

__fail:
    av_packet_free(&out);

    return ret;
}

int main(int argc, char * argv[]) {

    int ret;
    int video_stream_index = -1;

    char errors[1024];

    char *src_filename;
    char *dst_filename;

    FILE *dst_fd = NULL;

    AVPacket pkt;
    AVFormatContext *fmt_ctx = NULL;

    av_log_set_level(AV_LOG_DEBUG);

    av_log(NULL, AV_LOG_INFO, "Begin!\n");

    if (argc < 3) {
       av_log(NULL, AV_LOG_ERROR, "The count of parameters should be more than 3!\n");
       return -1;
    }

    src_filename = argv[1];
    dst_filename = argv[2];

    if (!src_filename||!dst_filename) {
       av_log(NULL, AV_LOG_ERROR, "Src or dst file is NULL, please check again!\n");
       return -1;
    }

    av_register_all();

    // src_filename
    ret = avformat_open_input(&fmt_ctx, src_filename, NULL, NULL);
    if (ret < 0) {
       av_log(NULL, AV_LOG_ERROR, "Could not open scr file!\n");
       return -1;
    }
    av_log(NULL, AV_LOG_INFO, "Success to open scr file!\n");

    // dst_filename
    dst_fd = fopen(dst_filename, "wb");
    if (!dst_fd) {
       av_log(NULL, AV_LOG_ERROR, "Could not open dst file!\n");
    }
    av_log(NULL, AV_LOG_INFO, "Success to open dst file!\n");

    // log media infomation
    av_dump_format(fmt_ctx, 0, src_filename, 0);

    // init packet
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;

    // find best stream
    video_stream_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_stream_index < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not find video stream in input file!\n");
        return AVERROR(EINVAL);
    }

    // read frame for media file
    while (av_read_frame(fmt_ctx, &pkt) >= 0) {
       if (pkt.stream_index == video_stream_index) {
           // 抽取数据
           h264_mp4toannexb(fmt_ctx, &pkt, dst_fd);
       }
       av_packet_unref(&pkt);
    }

    // close input media file
    avformat_close_input(&fmt_ctx);
    if (dst_fd) {
       fclose(dst_fd);
    }

    av_log(NULL, AV_LOG_INFO, "Finish!\n");
    return 0;
}
