#include <libavutil/log.h>
#include <libavformat/avformat.h>

int main(int argc, char * argv[]) {
    // 变量
    int ret;
    AVFormatContext *fmt_ctx = NULL;
    
    av_log_set_level(AV_LOG_DEBUG);

    // 1
    av_register_all();

    // 2
    ret = avformat_open_input(&fmt_ctx, "./test.mp4", NULL, NULL);
    if (ret < 0) {
       av_log(NULL, AV_LOG_DEBUG, "Can't open file:%s\n", av_err2str(ret));
       return ret;
    }
    av_log(NULL, AV_LOG_INFO, "Success to open file!\n");

    // 3
    av_dump_format(fmt_ctx, 0, "./test.mp4", 0);

    avformat_close_input(&fmt_ctx);
    
    return 0;
}
