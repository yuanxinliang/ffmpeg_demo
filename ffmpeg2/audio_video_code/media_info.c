#include <stdio.h>
#include <libavutil/log.h>
#include <libavformat/avformat.h>

int main(int argc, char * argv[]) {
    
    int ret;
    char *src;
    AVFormatContext *fmt_ctx = NULL;
    
    av_log_set_level(AV_LOG_DEBUG);

    if (argc < 2) {
       av_log(NULL, AV_LOG_ERROR, "Error: Must input src!\n");
    }
    src = argv[1];
    // open
    ret = avformat_open_input(&fmt_ctx, src, NULL, NULL);
    if (ret < 0) {
       av_log(NULL, AV_LOG_ERROR, "Error: Can't open src file!\n");
       return -1;
    }
    av_log(NULL, AV_LOG_INFO, "Success to open src file!\n");
    // log media info
    av_dump_format(fmt_ctx, 0, src, 0);
    // close
    avformat_close_input(&fmt_ctx);

    return 0;
}
