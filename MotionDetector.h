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
};


#endif //HOMECAMRECORDER_MOTIONDETECTOR_H
