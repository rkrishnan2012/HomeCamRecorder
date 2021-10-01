#include "Muxer.h"

RotatingFileMuxer::RotatingFileMuxer(const string &basename, const string &extension) {
    this->basename = basename;
    this->extension = extension;
    init();
}

void RotatingFileMuxer::init() {
    output_ctx = avformat_alloc_context();
    output_file = get_output_file_name();
    file_number = (file_number + 1) % MAX_FILES;
    file_start_time = system_clock::now();

    output_format = av_guess_format(extension.c_str(), nullptr, nullptr);
    if (avformat_alloc_output_context2(&output_ctx, output_format, nullptr, output_file.c_str()) < 0) {
        cerr << "RotatingFileMuxer failed to create output context." << endl;
        return;
    }

    if (!(output_ctx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&output_ctx->pb, output_file.c_str(), AVIO_FLAG_WRITE) < 0) {
            cerr << "RotatingFileMuxer failed to open output file." << endl;
            return;
        }
    }
}

void RotatingFileMuxer::send_packet(AVPacket *packet) {
    long prev_duration = packet->duration;
    long prev_pts = packet->pts;
    long prev_dts = packet->dts;
    long prev_pos = packet->pos;

    auto input_timebase = input_timebase_per_stream[packet->stream_index];
    auto output_timebase = output_timebase_per_stream[packet->stream_index];
    packet->duration = av_rescale_q((int64_t) packet->duration, input_timebase, output_timebase);
    packet->pts = av_rescale_q_rnd((int64_t) packet->pts, input_timebase, output_timebase,
                                   AVRounding(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
    packet->dts = av_rescale_q_rnd((int64_t) packet->dts, input_timebase, output_timebase,
                                   AVRounding(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
    packet->pos = -1;
    fix_packet_timestamps(packet);

    if (av_write_frame(output_ctx, packet) < 0) {
        int total_frames_read = (video_frames_written + audio_frames_written);
        cerr << "RotatingFileMuxer failed to write to packet " << total_frames_read << " to file."
             << " PTS: " << packet->pts << " DTS: " << packet->dts << endl;
        return;
    }
    if (video_frames_written == 0 && audio_frames_written == 0) {
        av_dump_format(output_ctx, 0, output_file.c_str(), 1);
    }
    if (packet->stream_index == video_stream_index) {
        if (TRACE_LOG)
            cout << "RotatingFileMuxer writing video frame " << video_frames_written << ". DTS: " << packet->dts << endl;

        video_frames_written++;
    }
    if (packet->stream_index == audio_stream_index) {
        if (TRACE_LOG)
            cout << "RotatingFileMuxer writing audio frame " << audio_frames_written << ". DTS: " << packet->dts << endl;
        audio_frames_written++;
    }
    packet->duration = prev_duration;
    packet->pts = prev_pts;
    packet->dts = prev_dts;
    packet->pos = prev_pos;
    if (duration_cast<seconds>(system_clock::now() - file_start_time).count() > MAX_FILE_DURATION_SEC) {
        cout << "RotatingFileMuxer rotating file " << file_number << endl;
        RotatingFileMuxer::release();
    }
}

string RotatingFileMuxer::get_output_file_name() {
    string output_file_full = basename + "_" + std::to_string(file_number) + "." + extension;
    if (remove(output_file_full.c_str()) == 0) {
        cout << "RotatingFileMuxer removed file " << output_file_full << endl;
    }
    return output_file_full;
}

void RotatingFileMuxer::release() {
    av_write_trailer(output_ctx);
    if (output_ctx && !(output_ctx->oformat->flags & AVFMT_NOFILE))
        avio_closep(&output_ctx->pb);
    avformat_free_context(output_ctx);
    Muxer::release();
    init();
}