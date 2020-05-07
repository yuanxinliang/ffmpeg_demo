#include <stdio.h>
#include <libavutil/log.h>
#include <libavformat/avformat.h>

int main(int argc, char * argv[]) {
    
    int ret;
    AVIODirContext *dir_ctx = NULL;
    AVIODirEntry *dir_entry = NULL;

    av_log_set_level(AV_LOG_INFO);

    ret = avio_open_dir(&dir_ctx, "./", NULL);
    if (ret < 0) {
       av_log(NULL, AV_LOG_ERROR, "Can't open dir:%s!\n", av_err2str(ret));
       return -1;
    }

    while (1) {
       ret = avio_read_dir(dir_ctx, &dir_entry);
       if (ret < 0) {
          av_log(NULL, AV_LOG_ERROR, "Can't read dir:%s!\n", av_err2str(ret));
	  goto __fail;
       }
       if (!dir_entry) {
          break;
       }
       av_log(NULL, AV_LOG_INFO, "%s - %lld\n", dir_entry->name, dir_entry->size);
       avio_free_directory_entry(&dir_entry);
    }
__fail:
    avio_close_dir(&dir_ctx);
    return 0;
}
