#include "SummaryGenerator.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavcodec/avcodec.h>
}

SummaryGenerator::SummaryGenerator(
    const string &recordings_dir, 
    const string &basename, 
    const string &output_file) : recordings_dir(std::move(recordings_dir)), basename(std::move(basename)), output_file(std::move(output_file)) {}

void SummaryGenerator::run() {
    cout << "Running summary for " << basename << endl;
//     Get list of video files ordered by timestamp
//     Get list of motion timestamps
//     Add each video file's frames with motion into the output file
//         Get list of motion timestamps > time start of video and < time end of video
//             Add motion time - 5s until motion time + 5s into output file
    auto video_files = get_video_files();
    auto motion_timestamps = get_motion_timestamps();
    RotatingFileMuxer muxer = RotatingFileMuxer(recordings_dir + "/" + basename + "_summary", "flv");
    muxer.init();
    
    for (pair<string, long> video_file : video_files) {
        cout << video_file.first << " " << video_file.second << endl;
        add_video(&muxer, recordings_dir + "/" + video_file.first, video_file.second, motion_timestamps);
    }
    muxer.release();
}
void SummaryGenerator::release() {

}

vector<pair<string, long>> SummaryGenerator::get_video_files() {
    vector<pair<string, long>> video_files;
    directory_iterator end_itr;
    for (directory_iterator itr(this->recordings_dir); itr != end_itr; ++itr ) {
        if (is_regular_file(itr->status())) {
            auto is_relevant = itr->path().filename().string().find(this->basename) == 0;
            auto is_not_summary = itr->path().filename().string().find("_summary") == -1;
            auto is_flv_file = itr->path().extension().string().compare(".flv") == 0;
            if (is_not_summary && is_relevant && is_flv_file && file_size(itr->path()) > 0) {
                auto path = itr->path().filename().string();
                ifstream input_file(this->recordings_dir + "/" + itr->path().stem().string() + "_start_time.txt");
                long start_timestamp;
                input_file >> start_timestamp;
                input_file.close();
                video_files.push_back(pair(path, start_timestamp));
            }
        }
    }
    sort(video_files.begin(), video_files.end(),
        [](const pair<string, long> &a, const pair<string, long> &b) -> bool {
        return b.second > a.second;
    });
    return video_files;
}

vector<long> SummaryGenerator::get_motion_timestamps() {
    vector<long> motion_timestamps;
    ifstream motion_file(this->recordings_dir + "/" + this->basename + ".csv");
    cout << this->recordings_dir + "/" + this->basename + ".csv" << endl;
    long timestamp;
    while(motion_file >> timestamp) {
        motion_timestamps.push_back(timestamp);
    }
    motion_file.close();
    return motion_timestamps;
}

void SummaryGenerator::add_video(RotatingFileMuxer *muxer,
                                 const string video_file_path,
                                 const long start_timestamp,
                                 vector<long> motion_timestamps) {
    
    cout << "Reading " << video_file_path.c_str() << endl;
    AVFormatContext *input_ctx = avformat_alloc_context();
    int ret;
    ret = avformat_open_input(&input_ctx, video_file_path.c_str(), nullptr, nullptr);
    
    ret = avformat_find_stream_info(input_ctx, nullptr);
    
    AVCodec *input_video_codec;
    int video_stream_idx = av_find_best_stream(input_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &input_video_codec, 0);
    
    AVCodec *input_audio_codec;
    int audio_stream_idx = av_find_best_stream(input_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &input_audio_codec, 0);
    
    av_read_play(input_ctx);
    
    AVRational input_timebase_per_stream[2]{};
    if (muxer->should_add_streams) {
        auto input_video_stream = input_ctx->streams[video_stream_idx];
        muxer->add_stream(input_video_stream, input_video_codec, false);

        auto input_audio_stream = input_ctx->streams[audio_stream_idx];
        muxer->add_stream(input_audio_stream, input_audio_codec, true);
        
        input_timebase_per_stream[0] = input_video_stream->time_base;
        input_timebase_per_stream[1] = input_audio_stream->time_base;
    }
    
    const long BUFFER_TIME_BEFORE_MS = 10000;
    const long BUFFER_TIME_AFTER_MS = 5000;
    
    long next_motion_timestamp_video = -1;
    long next_motion_timestamp_audio = -1;
    for (long timestamp : motion_timestamps) {
        if (timestamp > start_timestamp) {
            next_motion_timestamp_video = timestamp;
            next_motion_timestamp_audio = timestamp;
            break;
        }
    }
    
    bool has_more_frames = true;
    bool saw_key_frame = false;
    while (has_more_frames && next_motion_timestamp_video != -1) {
        AVPacket *packet = av_packet_alloc();
        ret = av_read_frame(input_ctx, packet);
        if (ret != 0) {
            has_more_frames = false;
            break;
        }
        
        if (packet->stream_index == video_stream_idx) {
            long epoch_time_ms = packet->pts + start_timestamp;
            long motion_start = next_motion_timestamp_video - BUFFER_TIME_BEFORE_MS;
            long motion_end = next_motion_timestamp_video + BUFFER_TIME_AFTER_MS;
            
            if (!saw_key_frame && !(packet->flags & AV_PKT_FLAG_KEY)) {
                av_packet_free(&packet);
                continue;
            }
            saw_key_frame = true;
            
            if (epoch_time_ms > motion_start && epoch_time_ms < motion_end) {
                muxer->send_packet(packet);
            }
            if (epoch_time_ms > motion_end) {
                for (long timestamp : motion_timestamps) {
                    if (timestamp > motion_end) {
                        next_motion_timestamp_video = timestamp;
                        break;
                    }
                }
            }
        } else {
//            long epoch_time_ms = packet->pts + start_timestamp;
//            long motion_start = next_motion_timestamp_audio - BUFFER_TIME_BEFORE_MS;
//            long motion_end = next_motion_timestamp_audio + BUFFER_TIME_AFTER_MS;
//            if (epoch_time_ms > motion_start && epoch_time_ms < motion_end) {
//                muxer->send_packet(packet);
//            }
//            if (epoch_time_ms > motion_end) {
//                for (long timestamp : motion_timestamps) {
//                    if (timestamp > motion_end) {
//                        next_motion_timestamp_audio = timestamp;
//                        break;
//                    }
//                }
//            }
        }
        av_packet_free(&packet);
    }
    
    avformat_close_input(&input_ctx);
}
