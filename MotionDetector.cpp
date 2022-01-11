#include "MotionDetector.h"

MotionDetector::MotionDetector() {

}

void MotionDetector::release() {

}

void MotionDetector::send_packet(AVPacket *packet) {
    int last_IDR_index = -1;
    const int UNIT_TYPE_IDR = 5; // https://www.itu.int/rec/T-REC-H.264-200305-S Table 7-1.

    for (int i = 5; i < packet->size; i++) {
        if (packet->data[i-2] == 1 && packet->data[i-3] == 0 && packet->data[i-4] == 0 && packet->data[i-5] == 0) {
            auto data = packet->data;
            int header = data[i-1]   & (0b11111111);
            int forbidden = (header & (0b10000000)) >> 7;
            int ref_idc = (header & (0b01100000)) >> 5;
            int unit_type = header & (0b00011111);
            if (unit_type == UNIT_TYPE_IDR) {
                if (last_IDR_index == -1) {
                    last_IDR_index = i;
                }
            } else {
                if (last_IDR_index != -1) {
                    mark_idr_frame_size(i - last_IDR_index);
                    last_IDR_index = -1;
                }
            }
        }
    }
    if (last_IDR_index != -1) {
        mark_idr_frame_size(packet->size - last_IDR_index);
        last_IDR_index = -1;
    }
}

void MotionDetector::mark_idr_frame_size(int size) {
    const int THRESHOLD = 50000;
    if (size < THRESHOLD) {
        cout << "Motion detected!" << endl;
    }
}
