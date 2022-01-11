#ifndef HOMECAMRECORDER_MOTIONDETECTOR_H
#define HOMECAMRECORDER_MOTIONDETECTOR_H

#include <iostream>

extern "C" {
#include <libavcodec/packet.h>
};

using namespace std;

class MotionDetector {

public:
    void send_packet(AVPacket *packet);

    void release();

    MotionDetector();

private:
    void mark_idr_frame_size(int size);
};


#endif //HOMECAMRECORDER_MOTIONDETECTOR_H
