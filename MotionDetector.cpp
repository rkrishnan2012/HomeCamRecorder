#include "MotionDetector.h"

MotionDetector::MotionDetector() {

}

void MotionDetector::release() {

}

void MotionDetector::send_packet(AVPacket *packet) {
    for (int i = 5; i < packet->size; i++) {
        if (packet->data[i-2] == 1 && packet->data[i-3] == 0 && packet->data[i-4] == 0 && packet->data[i-5] == 0) {
            auto data = packet->data;
            int header = data[i-1]   & (0b11111111);
            int forbidden = (header & (0b10000000)) >> 7;
            int ref_idc = (header & (0b01100000)) >> 5;
            int unit_type = header & (0b00011111);
            if (unit_type <= 5) {
                int leading_zero_bits = 0;
                for (int b = 0; !b; leading_zero_bits++) {
                    b = data[i] & (1 << (7 - leading_zero_bits));
                    if (leading_zero_bits > 7) {
                        cerr << "FAILED!" << endl;
                    }
                }
                leading_zero_bits++;
                cout << "   LeadingZeroBits: " << leading_zero_bits << endl;
                cout << "      ";
                for (int b = leading_zero_bits; b < 2 * leading_zero_bits; b++) {
                    cout << (data[i] & (1 << (7 - b)));
                }

            }
            cout << "   Header: " << forbidden << " " << ref_idc << " " << unit_type << endl;
        }
    }
}
