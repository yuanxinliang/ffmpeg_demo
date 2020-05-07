#include <libavutil/log.h>

int main(int argc, char *argv[]) {

   printf("Hello World!\n");
   // 日志系统
   av_log_set_level(AV_LOG_DEBUG);
   av_log(NULL, AV_LOG_INFO, "Hello FFmpeg Log!\n");
   return 0;
   
}
