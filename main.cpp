#include <iostream>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>


/**
 * rtsp://admin:2147483648@10.0.9.48/live/ch0 -> rtmp://localhost/flv/1
 * rtsp://admin:2147483648@10.0.9.26/live/ch -> rtmp://localhost/flv/2
 * rtsp://admin:2147483648@10.0.9.32/live/ch0 -> rtmp://localhost/flv/3
 * @return
 */
using namespace std;


void dump_stream_info(const string &input_url) {
    char c_input_url[input_url.size() + 1];
    input_url.copy(c_input_url, input_url.size() + 1);

    AVFormatContext *format_context = avformat_alloc_context();
    if (avformat_open_input(&format_context, c_input_url, nullptr, nullptr) != 0) {
        cerr << "Failed to open input." << endl;
        return;
    }

    if (avformat_find_stream_info(format_context, nullptr) < 0) {
        cerr << "Failed to find stream info." << endl;
    }

    int video_stream_idx = -1;
    int audio_stream_idx = -1;
    for (int i = 0; i < format_context->nb_streams; i++) {
        if (format_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_idx = i;
        } else if (format_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream_idx = i;
        }
    }

    cout << "Audio stream at index: " << audio_stream_idx << endl;
    cout << "Video stream at index: " << video_stream_idx << endl;
}

int main() {
    std::cout << "Hello, World!" << std::endl;
    avformat_network_init();

    dump_stream_info("rtsp://admin:2147483648@10.0.9.48/live/ch0");
    return 0;
}


