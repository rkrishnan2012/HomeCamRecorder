#include <iostream>
#include <thread>
#include <csignal>
#include <utility>
#include <vector>
#include <unistd.h>

#include "Muxer.h"
#include "MotionDetector.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavcodec/avcodec.h>
}

#undef av_err2str
#define av_err2str(errnum) av_make_error_string((char*)__builtin_alloca(AV_ERROR_MAX_STRING_SIZE), AV_ERROR_MAX_STRING_SIZE, errnum)

/**
 * rtsp://admin:2147483648@10.0.9.48/live/ch0 -> rtmp://localhost/flv/1
 * rtsp://admin:2147483648@10.0.9.26/live/ch0 -> rtmp://localhost/flv/2
 * rtsp://admin:2147483648@10.0.9.32/live/ch0 -> rtmp://localhost/flv/3
 * @return
 */
using namespace std;
using namespace std::chrono;

bool kill_threads;
const int TIMEOUT_MILLI = 10000;

class CameraSource {
public:
    CameraSource(string name, string input_url, vector<Muxer *> muxers) :
            name(std::move(name)),
            muxers(std::move(muxers)),
            url(std::move(input_url)) {}

public:
    const string name;
    const string url;
    vector<Muxer *> muxers;
    bool needs_restart{false};
    int video_frames_read{};
    int audio_frames_read{};

    time_point<system_clock> last_frame_read_start_time{};
};

vector<Muxer *> create_muxers(const string &basename,
                              const string &extension,
                              const string &remote_server_url) {
    vector<Muxer *> muxers;
    muxers.push_back(new RotatingFileMuxer(basename, extension));
    muxers.push_back(new FLVMuxer(remote_server_url));
    return muxers;
}

std::vector<CameraSource> cameras = { // NOLINT(cert-err58-cpp)
        CameraSource("Front door", "rtsp://admin:2147483648@10.0.9.48/live/ch0",
                     create_muxers("/home/rohit/Recordings/front_door", "flv",
                                   "rtmp://localhost/flv/1")),
        CameraSource("Back yard", "rtsp://admin:2147483648@10.0.9.26/live/ch0",
                     create_muxers("/home/rohit/Recordings/back_yard", "flv",
                                   "rtmp://localhost/flv/2")),
        CameraSource("Driveway", "rtsp://admin:2147483648@10.0.9.32/live/ch0",
                     create_muxers("/home/rohit/Recordings/driveway", "flv",
                                   "rtmp://localhost/flv/3"))
};

int interrupt_callback(void *ptr) {
    int index = *(int *) ptr;
    CameraSource &source = cameras.at(index);
    auto now = system_clock::now();
    if (duration_cast<milliseconds>(now - source.last_frame_read_start_time).count() > TIMEOUT_MILLI) {
        cerr << "(" << source.name << ") Timed out " << endl;
        return 1;
    }
    return 0;
}

void run(int index) {
    CameraSource &source = cameras.at(index);
    do {
        source.last_frame_read_start_time = system_clock::now();
        source.needs_restart = false;

        AVFormatContext *input_ctx = avformat_alloc_context();
        AVIOInterruptCB callback = {interrupt_callback, (void *) &index};
        input_ctx->interrupt_callback = callback;

        int ret;
        ret = avformat_open_input(&input_ctx, source.url.c_str(), nullptr, nullptr);
        if (ret < 0) {
            cerr << "(" << source.name << ") Failed to open " << source.url 
                 << ". Error = " << av_err2str(ret) << endl;
            source.needs_restart = true;
            avformat_free_context(input_ctx);
            avformat_close_input(&input_ctx);
            cerr << "(" << source.name << ") Sleeping for 10s before restarting." << source.url << endl;
            sleep(10);
            continue;
        }

        ret = avformat_find_stream_info(input_ctx, nullptr);
        if (ret < 0) {
            cerr << "(" << source.name << ") Failed to find stream info"
                 << ". Error = " << av_err2str(ret) << endl;;
            source.needs_restart = true;
            avformat_free_context(input_ctx);
            avformat_close_input(&input_ctx);
            continue;
        }

        AVCodec *input_video_codec;
        int video_stream_idx = av_find_best_stream(input_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &input_video_codec, 0);

        AVCodec *input_audio_codec;
        int audio_stream_idx = av_find_best_stream(input_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &input_audio_codec, 0);

        av_read_play(input_ctx);

        bool saw_key_frame = false;

        //auto motion_detector = MotionDetector();

        while (!kill_threads && !source.needs_restart) {
            AVPacket *packet = av_packet_alloc();
            source.last_frame_read_start_time = system_clock::now();
            ret = av_read_frame(input_ctx, packet);
            if (ret != 0) {
                cerr << "(" << source.name << ") Failed to read frame number " << (source.video_frames_read + source.audio_frames_read)
                    << ". Error = " << av_err2str(ret) << "." << endl;
                source.needs_restart = true;
                break;
            }
            if (!saw_key_frame && (packet->stream_index != video_stream_idx || !(packet->flags & AV_PKT_FLAG_KEY))) {
                cerr << "(" << source.name << ") Waiting for keyframe. Ignoring frame." << endl;
                av_packet_unref(packet);
                continue;
            }
            saw_key_frame = true;

            for (Muxer *muxer: source.muxers) {
                if (muxer->should_add_streams) {
                    auto input_video_stream = input_ctx->streams[video_stream_idx];
                    muxer->add_stream(input_video_stream, input_video_codec, false);
                    auto input_audio_stream = input_ctx->streams[audio_stream_idx];
                    muxer->add_stream(input_audio_stream, input_audio_codec, true);
                }
            }

            for (Muxer *muxer: source.muxers)
                muxer->send_packet(packet);
            if (packet->stream_index == video_stream_idx) {
                //motion_detector.send_packet(packet);
            }

            if (packet->stream_index == video_stream_idx) source.video_frames_read++;
            if (packet->stream_index == audio_stream_idx) source.audio_frames_read++;

            av_packet_free(&packet);
        }

        cerr << "(" << source.name << ") Releasing muxers." << endl;
        for (Muxer *muxer: source.muxers)
            muxer->release();
        //motion_detector.release();
        
        cerr << "(" << source.name << ") closing input." << endl;
        avformat_close_input(&input_ctx);

        if (source.needs_restart) {
            cout << "(" << source.name << ") Restarting " << source.name << endl;
        } else {
            cout << "(" << source.name << ") Quitting " << source.name << endl;
        }
    } while(source.needs_restart);
}

void monitor_frame_rates() {
    auto t_start = system_clock::now();
    while (!kill_threads) {
        for (CameraSource &source: cameras) {
            auto t_end = system_clock::now();
            long elapsed_time_ms = duration_cast<milliseconds>(t_end - t_start).count();
            int rate = (int) (source.video_frames_read / ((long double) elapsed_time_ms / 1000.0));
            auto now = system_clock::now();
            cout << "(" << source.name << ") Video frames read: " << source.video_frames_read
                 << " Audio frames read: " << source.audio_frames_read
                 << " Rate: " << rate << endl;

            long seconds_since_last_frame = duration_cast<milliseconds>(now - source.last_frame_read_start_time).count();
            if (seconds_since_last_frame > TIMEOUT_MILLI) {
                cout << "Camera (" << source.name << ") has died. Last frame was " << seconds_since_last_frame << " seconds ago." << endl;
                source.last_frame_read_start_time = now;
                source.needs_restart = true;
            }
        }
        sleep(5);
    }
}

void signal_handler(int signum) {
    if (signum != SIGINT) return;
    cout << "Interrupt signal (" << signum << ") received.\n";
    kill_threads = true;
}

int main() {
    signal(SIGINT, signal_handler);

    std::cout << "HomeCam v0" << std::endl;
    avformat_network_init();

    thread camera1(run, 0);
    thread camera2(run, 1);
    thread camera3(run, 2);
    thread frame_rate_monitor(monitor_frame_rates);

    frame_rate_monitor.join();
    camera1.join();
    camera2.join();
    camera3.join();

    return 0;
}


