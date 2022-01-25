#include <iostream>
#include <thread>
#include <csignal>
#include <utility>
#include <vector>
#include <unistd.h>
#include <sstream>
#include <execinfo.h>
#include <ctime>

#include "Muxer.h"
#include "MotionDetector.h"
#include "SummaryGenerator.h"
#include "twilio.h"

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
const int TIMEOUT_MILLI = 20000;
const string RECORDINGS_DIR = "/home/rohit/Recordings";

shared_ptr<twilio::Twilio> m_twilio = NULL;

const string ADMIN_PHONE = "3393641604";
const string FROM_PHONE = "8573550142";

class CameraSource {
public:
    CameraSource(string name, string input_url, vector<Muxer *> muxers, string recordings_dir, string output_file_basename, int motion_threshold) :
    name(std::move(name)),
    muxers(std::move(muxers)),
    url(std::move(input_url)),
    recordings_dir(std::move(recordings_dir)),
    output_file_basename(std::move(output_file_basename)),
    motion_threshold(std::move(motion_threshold)) {}
    
public:
    const string name;
    const string url;
    const string recordings_dir;
    const string output_file_basename;
    const int motion_threshold;
    
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
                 create_muxers(RECORDINGS_DIR + "/front_door", "flv",
                               "rtmp://localhost/flv/1"),
                 RECORDINGS_DIR, "front_door", 35000),
    //        CameraSource("Back yard", "rtsp://admin:2147483648@10.0.9.26/live/ch0",
    //                     create_muxers(RECORDINGS_DIR + "/back_yard", "flv",
    //                                   "rtmp://localhost/flv/2"),
    //                    RECORDINGS_DIR, "back_yard", 30000),
    CameraSource("Driveway", "rtsp://admin:2147483648@10.0.9.32/live/ch0",
                 create_muxers(RECORDINGS_DIR + "/driveway", "flv",
                               "rtmp://localhost/flv/3"),
                 RECORDINGS_DIR, "driveway", 30000)
};

void send_sms(string message);

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

void sigint_handler(int signum) {
    if (signum != SIGINT) return;
    cout << "Interrupt signal (" << signum << ") received.\n";
    kill_threads = true;
}

void segv_handler(int sig) {
  void *array[10];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, 10);

  // print out all the frames to stderr
  fprintf(stderr, "Error: signal %d:\n", sig);
  backtrace_symbols_fd(array, size, STDERR_FILENO);
  exit(1);
}

void sigpipe_handler(int sig) {
    return;
}


void run(int index) {
    CameraSource &source = cameras.at(index);
    int fail_count = 0;
    
    do {
        source.last_frame_read_start_time = system_clock::now();
        source.needs_restart = false;
        cout << "(" << source.name << ") Allocating context." << endl;
        AVFormatContext *input_ctx = avformat_alloc_context();
        AVIOInterruptCB callback = {interrupt_callback, (void *) &index};
        input_ctx->interrupt_callback = callback;
        
        AVCodec *input_video_codec;
        AVCodec *input_audio_codec;
        
        int video_stream_idx;
        int audio_stream_idx;
        
        cout << "(" << source.name << ") Opening input." << endl;
        
        try {
            int ret;
            ret = avformat_open_input(&input_ctx, source.url.c_str(), nullptr, nullptr);
            if (ret < 0) {
                cerr << "(" << source.name << ") Failed to open " << source.url << ". Error = " << av_err2str(ret) << endl;
                throw ret;
            }
            
            cout << "(" << source.name << ") Finding stream info." << endl;
            ret = avformat_find_stream_info(input_ctx, nullptr);
            if (ret < 0) {
                cerr << "(" << source.name << ") Failed to find stream info. Error = " << av_err2str(ret) << endl;
                throw ret;
            }
            
            cout << "(" << source.name << ") Finding video stream." << endl;
            
            video_stream_idx = av_find_best_stream(input_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &input_video_codec, 0);
            if (video_stream_idx < 0) {
                cerr << "(" << source.name << ") Failed to find video stream. Error = " << av_err2str(video_stream_idx) << endl;;
                throw video_stream_idx;
            }
            
            cout << "(" << source.name << ") Finding audio stream." << endl;
            
            audio_stream_idx = av_find_best_stream(input_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &input_audio_codec, 0);
            if (audio_stream_idx < 0) {
                cerr << "(" << source.name << ") Failed to find audio stream. Error = " << av_err2str(ret) << endl;;
                throw audio_stream_idx;
            }
            
            cout << "(" << source.name << ") Read stream for playback." << endl;
            ret = av_read_play(input_ctx);
            if (ret < 0) {
                cout << "(" << source.name << ") Failed to read stream for playback. Restarting." << endl;
                throw ret;
            }
        } catch(int e) {
            fail_count++;
            long sleep_time = 10 + pow(2, fail_count);
            if (e == AVERROR_EOF) {
                sleep_time = 0;
            }
            
            if (fail_count > 1) {
                string msg = source.name + " camera has died. Restarting in " + to_string(sleep_time) + " seconds.";
                cerr << msg << endl;
                send_sms(msg);
            }
            
            source.needs_restart = true;
            avformat_close_input(&input_ctx);
            
            sleep(sleep_time);
            continue;
        }
        
        int ret;
        bool saw_key_frame = false;
        
        auto motion_csv = source.recordings_dir + "/" + source.output_file_basename + ".csv";
        auto motion_detector = MotionDetector(source.name, motion_csv, source.motion_threshold);
        
        cout << "(" << source.name << ") Starting playback loop." << endl;

        send_sms(source.name + " camera is active");
        
        long video_packet_count = 0;
        try {
            while (!kill_threads && !source.needs_restart) {
                AVPacket *packet = av_packet_alloc();
                source.last_frame_read_start_time = system_clock::now();
                ret = av_read_frame(input_ctx, packet);
                if (ret < 0) {
                    cerr << "(" << source.name << ") Failed to read frame number " << (source.video_frames_read + source.audio_frames_read)
                    << ". Error = " << av_err2str(ret) << "." << endl;
                    av_packet_free(&packet);
                    throw ret;
                }
                if (!saw_key_frame && (packet->stream_index != video_stream_idx || !(packet->flags & AV_PKT_FLAG_KEY))) {
                    cout << "(" << source.name << ") Waiting for keyframe. Ignoring frame." << endl;
                    av_packet_unref(packet);
                    continue;
                }
                saw_key_frame = true;
                
                for (Muxer *muxer: source.muxers) {
                    if (!muxer->did_init) {
                        muxer->init();
                    }
                    if (muxer->should_add_streams) {
                        cout << "Setting input video stream." << endl;
                        auto input_video_stream = input_ctx->streams[video_stream_idx];
                        cout << "Adding streams." << endl;
                        muxer->add_stream(input_video_stream, input_video_codec, false);
                        auto input_audio_stream = input_ctx->streams[audio_stream_idx];
                        muxer->add_stream(input_audio_stream, input_audio_codec, true);
                    }
                }
                
                for (Muxer *muxer: source.muxers)
                    muxer->send_packet(packet);
                if (packet->stream_index == video_stream_idx) {
                    motion_detector.send_packet(packet);
                }
                
                if (packet->stream_index == video_stream_idx) source.video_frames_read++;
                if (packet->stream_index == audio_stream_idx) source.audio_frames_read++;
                
                if (packet->stream_index == video_stream_idx) {
                    video_packet_count++;
                    if (video_packet_count > 30) {
                        fail_count = 0;
                    }
                }
                
                av_packet_free(&packet);
            }
        } catch(int e) {
            fail_count++;
            long sleep_time = 10 + pow(2, fail_count);
            if (e == AVERROR_EOF) {
                sleep_time = 0;
            }
            
            if (fail_count > 1) {
                string msg = source.name + " camera has died. Restarting in " + to_string(sleep_time) + " seconds.";
                cerr << msg << endl;
                send_sms(msg);
            }
            
            source.needs_restart = true;

            sleep(sleep_time);
        }
        
        cerr << "(" << source.name << ") Releasing muxers." << endl;
        for (Muxer *muxer: source.muxers)
            muxer->release();
        motion_detector.release();
        
        cerr << "(" << source.name << ") closing input." << endl;
        avformat_close_input(&input_ctx);
        
        if (source.needs_restart) {
            cout << "(" << source.name << ") Restarting " << source.name << endl;
        } else {
            cout << "(" << source.name << ") Quitting " << source.name << endl;
            send_sms(source.name + " camera quitting");
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
                cout << "Camera (" << source.name << ") has died. Last frame was " << seconds_since_last_frame << " ms ago." << endl;
                source.last_frame_read_start_time = now;
                source.needs_restart = true;
            }
        }
        sleep(5);
    }
}

void generate_summaries() {
    for (CameraSource &source: cameras) {
        auto summary_generator = SummaryGenerator(
                                                  source.recordings_dir,
                                                  source.output_file_basename,
                                                  source.recordings_dir + source.output_file_basename + "/summary.flv"
                                                  );
        summary_generator.run();
    }
}

void init_twilio() {
    char* sid = getenv("TWILIO_SID");
    char* token = getenv("TWILIO_AUTH_TOKEN");
    if (!sid || !token) {
        cerr << "Failed to retrieve twilio sid and auth token" << endl;
        exit(1);
        return;
    }
    m_twilio = std::make_shared<twilio::Twilio>(sid, token);
}

void send_sms(string message) {
    string twilio_response;
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::ostringstream final_message;
    final_message << message << " at " << std::put_time(&tm, "%m/%d/%Y %r");
    
    bool success = m_twilio->send_message(ADMIN_PHONE, FROM_PHONE, final_message.str(), twilio_response, "", true);
    if (!success) {
        cout << "(Twilio) " << twilio_response << endl;
    }
}

int main(int argc, char* argv[]) {
    signal(SIGINT, sigint_handler);
    signal(SIGSEGV, segv_handler);
    signal(SIGPIPE, sigpipe_handler);
    
    std::cout << "JuniperCam v0" << std::endl;
    
    init_twilio();
    
    send_sms("JuniperCam starting up");
    
    avformat_network_init();
    
    bool run_summary = false;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--summarize") == 0) {
            run_summary = true;
            break;
        }
        
    }
    
    if (!run_summary) {
        thread camera1(run, 0);
        thread camera2(run, 1);
        //        thread camera3(run, 2);
        thread frame_rate_monitor(monitor_frame_rates);
        frame_rate_monitor.join();
        camera1.join();
        camera2.join();
        //        camera3.join();
    } else {
        cout << "Generating summary" << endl;
        generate_summaries();
    }
    
    return 0;
}


