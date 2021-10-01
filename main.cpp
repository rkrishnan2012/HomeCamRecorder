#include <iostream>
#include <thread>

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavformat/avio.h>
    #include <libavcodec/avcodec.h>
}

/**
 * rtsp://admin:2147483648@10.0.9.48/live/ch0 -> rtmp://localhost/flv/1
 * rtsp://admin:2147483648@10.0.9.26/live/ch -> rtmp://localhost/flv/2
 * rtsp://admin:2147483648@10.0.9.32/live/ch0 -> rtmp://localhost/flv/3
 * @return
 */
using namespace std;

std::atomic<bool> kill_threads;

void copy_streams(const string &input_url) {
    char c_input_url[input_url.size() + 1];
    input_url.copy(c_input_url, input_url.size() + 1);

    AVFormatContext *format_context = avformat_alloc_context();

    if (avformat_open_input(&format_context, c_input_url, nullptr, nullptr) != 0) {
        cerr << "Failed to open input." << endl;
        return;
    }

    if (avformat_find_stream_info(format_context, nullptr) < 0) {
        cerr << "Failed to find stream info." << endl;
        return;
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

    av_read_play(format_context);

    AVPacket *packet = av_packet_alloc();
    int ret = 0;
    int frame_num = 0;

    while(!kill_threads) {
        ret = av_read_frame(format_context, packet);
        if (ret != 0) {
            cerr << "Failed to read frame number " << frame_num << ". Ret = " << ret << "." << endl;
            return;
        }
        frame_num++;
        if (packet->stream_index == video_stream_idx) {
            cout << "(Video) Frame " << frame_num << endl;
        } else if (packet->stream_index == audio_stream_idx) {
            cout << "(Audio) Frame " << frame_num << endl;
        }
    }
}

void signalHandler( int signum ) {
    if (signum != SIGINT) return;
    cout << "Interrupt signal (" << signum << ") received.\n";
    kill_threads = true;
}

int main() {
    signal(SIGINT, signalHandler);

    std::cout << "HomeCam v0" << std::endl;
    avformat_network_init();

    thread camera1(copy_streams, "rtsp://admin:2147483648@10.0.9.48/live/ch0");
    camera1.join();

    return 0;
}


