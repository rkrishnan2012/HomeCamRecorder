#include "Muxer.h"

FLVMuxer::FLVMuxer(const string &output_url) {
    this->output_url = output_url;
    cerr << "(FLVMuxer) Constructor calling init." << endl;
}

void FLVMuxer::init() {
    if (did_init) return;
    
    cerr << "(FLVMuxer) allocating context." << endl;
    output_ctx = avformat_alloc_context();

    output_format = av_guess_format("flv", nullptr, nullptr);
    if (avformat_alloc_output_context2(&output_ctx, output_format, nullptr, output_url.c_str()) < 0) {
        cerr << "FLVMuxer failed to create output context." << endl;
        return;
    }

    if (!(output_ctx->oformat->flags & AVFMT_NOFILE)) {
        cerr << "(FLVMuxer) opening output file: " << output_url.c_str() << endl;
        long error = avio_open(&output_ctx->pb, output_url.c_str(), AVIO_FLAG_WRITE);
        if (error < 0) {
            cerr << "FLVMuxer failed to open output file: " << output_url.c_str() 
                 << ". Error = " << error << endl;
            return;
        }
        cerr << "(FLVMuxer) Done opening output file." << endl;
    }

    did_init = true;
}

void FLVMuxer::send_packet(AVPacket *packet) {
    if (!did_init)
        init();
    
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

    int ret = av_write_frame(output_ctx, packet);
    if (ret < 0) {
        int total_frames_read = (video_frames_written + audio_frames_written);
        cerr << "FLVMuxer failed to write to packet " << total_frames_read << " to file."
             << " PTS: " << packet->pts << " DTS: " << packet->dts << endl;
        if (ret == AVERROR_EOF) {
            cerr << "Re-starting FLV muxer." << endl;
            release();
        }
        return;
    }
    if (packet->stream_index == video_stream_index) {
        if (TRACE_LOG)
            cout << "FLVMuxer writing video frame " << video_frames_written << ". DTS: " << packet->dts << endl;
        video_frames_written++;
    }
    if (packet->stream_index == audio_stream_index) {
        if (TRACE_LOG)
            cout << "FLVMuxer writing audio frame " << audio_frames_written << ". DTS: " << packet->dts << endl;
        audio_frames_written++;
    }
    packet->duration = prev_duration;
    packet->pts = prev_pts;
    packet->dts = prev_dts;
    packet->pos = prev_pos;
}

void FLVMuxer::release() {
    cerr << "(FLVMuxer) writing trailer." << endl;
    av_write_trailer(output_ctx);
    if (output_ctx && !(output_ctx->oformat->flags & AVFMT_NOFILE)) {
        cerr << "(FLVMuxer) closing output context." << endl;
        avio_closep(&output_ctx->pb);
    }
    cerr << "(FLVMuxer) freeing context." << endl;
    avformat_free_context(output_ctx);
    Muxer::release();
    did_init = false;
}

