#include <libavformat/avformat.h>

int main(int argc, char * argv[])
{
    int ret;
    // rename
    ret = avpriv_io_move("111.txt", "222.txt");
    if (ret < 0) {
       av_log(NULL, AV_LOG_ERROR, "Failed to rename\n");
       return -1;
    }
    av_log(NULL, AV_LOG_INFO, "Success to rename!\n");

    // delete
    ret = avpriv_io_delete("333.txt");
    if (ret < 0) {
       av_log(NULL, AV_LOG_ERROR, "Failed to delete!\n");
       return -1;
    }
    av_log(NULL, AV_LOG_INFO, "Success to delete!\n");
    return 0;
}
