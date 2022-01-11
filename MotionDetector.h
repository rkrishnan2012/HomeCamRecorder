#ifndef HOMECAMRECORDER_MOTIONDETECTOR_H
#define HOMECAMRECORDER_MOTIONDETECTOR_H

#include <iostream>
#include <thread>
#include <csignal>
#include <utility>
#include <vector>
#include <unistd.h>
#include <fstream>

extern "C" {
#include <libavcodec/packet.h>
};

using namespace std;
using namespace std::chrono;

class MotionDetector {
public:
    void send_packet(AVPacket *packet);

    void release();

    MotionDetector(const string &motion_file, const int motion_threshold);

private:
    string motion_file_path;
    int motion_threshold;
    time_point<system_clock> last_write_time{};
    std::vector<long> write_queue{};
    void mark_idr_frame_size(int size);
    void flush_write_queue();
};


#endif //HOMECAMRECORDER_MOTIONDETECTOR_H
