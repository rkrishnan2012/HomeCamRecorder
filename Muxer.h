#ifndef HOMECAMRECORDER_MUXER_H
#define HOMECAMRECORDER_MUXER_H

#include <iostream>
#include <iostream>
#include <thread>
#include <csignal>
#include <utility>
#include <vector>
#include <unistd.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavcodec/avcodec.h>
}

using namespace std;
using namespace std::chrono;

class Muxer {
public:
    bool should_add_streams = true;
    bool did_init = false;

    Muxer() = default;
    virtual void send_packet(AVPacket *packet) = 0;
    virtual void release() {
        last_frame_dts_per_stream[0] = -1;
        last_frame_dts_per_stream[1] = -1;
        should_add_streams = true;
    }
    virtual void init() = 0;
    
    void add_stream(AVStream *input_stream,
                    AVCodec *input_codec,
                    bool write_header) {
        should_add_streams = false;
        input_timebase_per_stream[output_ctx->nb_streams] = input_stream->time_base;

        AVCodecParameters *input_video_params = input_stream->codecpar;
        AVStream *output_stream = avformat_new_stream(output_ctx, input_codec);
        cout << "Created stream" << endl;
        if (output_stream == nullptr) {
            cerr << "Failed to create output stream." << endl;
            return;
        }
        if (avcodec_parameters_copy(output_stream->codecpar, input_video_params) < 0) {
            cerr << "Failed to copy output parameters." << endl;
            return;
        }
        output_stream->codecpar->codec_tag = 0;
        output_stream->time_base = input_stream->time_base;
        if (input_codec->type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = output_stream->index;
        } else if (input_codec->type == AVMEDIA_TYPE_AUDIO) {
            audio_stream_index = output_stream->index;
        }

        if (write_header) {
            if (avformat_write_header(output_ctx, nullptr) < 0) {
                cerr << "Failed to write header to file." << endl;
                return;
            }
            for (int i = 0; i < output_ctx->nb_streams; i++) {
                output_timebase_per_stream[i] = output_ctx->streams[i]->time_base;
                cout << "Input timebase for stream " << i << ": "
                     << input_timebase_per_stream[i].num << "/" << input_timebase_per_stream[i].den << endl;
                cout << "Output timebase for stream " << i << ": "
                     << output_timebase_per_stream[i].num << "/" << output_timebase_per_stream[i].den << endl;
            }
        }
    }

protected:
    const bool TRACE_LOG = false;

    AVFormatContext *output_ctx{};
    AVOutputFormat *output_format{};
    int audio_stream_index{-1};
    int audio_frames_written{};
    int video_stream_index{-1};
    int video_frames_written{};
    AVRational input_timebase_per_stream[2]{};
    AVRational output_timebase_per_stream[2]{};
    long last_frame_dts_per_stream[2]{};

    void fix_packet_timestamps(AVPacket *packet) {
        auto cts = packet->pts - packet->dts;
        long last_frame_dts = last_frame_dts_per_stream[packet->stream_index];
        if (last_frame_dts == -1) {
            packet->dts = 0;
        } else {
            packet->dts = last_frame_dts + packet->duration;
        }
        last_frame_dts_per_stream[packet->stream_index] = packet->dts;
        packet->pts = cts + packet->dts;
    }
};

class RotatingFileMuxer : public Muxer {
public:
    RotatingFileMuxer(const string &basename, const string &extension);

    void send_packet(AVPacket *packet) override;

    void release() override;

    void init() override;
    
    void add_stream(AVStream *input_stream, AVCodec *input_codec, bool write_header);

private:
    string get_output_file_name();

private:
    bool did_init;
    string basename;
    string extension;
    string output_file;
    const int MAX_FILES = 32 /* 16 hours */;
    const int MAX_FILE_DURATION_SEC = 30 * 60;/* 30 minutes */
    int file_number{0};
    time_point<system_clock> file_start_time{};
};

class FLVMuxer : public Muxer {
public:
    explicit FLVMuxer(const string& output_url);
    void send_packet(AVPacket *packet) override;
    void release() override;
    void init() override;
private:
    string output_url;
};

#endif //HOMECAMRECORDER_MUXER_H
