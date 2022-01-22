#include "MotionDetector.h"

MotionDetector::MotionDetector(const string &motion_file_path, const int motion_threshold) {
    this->motion_file_path = motion_file_path;
    this->motion_threshold = motion_threshold;
    this->last_write_time = system_clock::now();
}

void MotionDetector::release() {

}

/**
  Extract multiple H264 NAL units from an AVPacket, find the IDR frame, and mark its size. If size > some threshold, mark the timestamp to a text file.
 */
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
    const int WRITE_INTERVAL_MSEC = 15000;
    if (size < this->motion_threshold) {
        long ms_since_epoch = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        write_queue.push_back(ms_since_epoch);
        cout << this->motion_file_path << ": Motion detected at " << ms_since_epoch << ". Size is " << size << endl;
    }
    if (duration_cast<milliseconds>(system_clock::now() - last_write_time).count() > WRITE_INTERVAL_MSEC) {
        flush_write_queue();
    }
}

void MotionDetector::flush_write_queue() {
    ofstream motion_file;
    motion_file.open(motion_file_path, std::ios_base::app);
    for(long timestamp : this->write_queue)  {
        motion_file << timestamp << endl;
    }
    cout << "Wrote motion to " << motion_file_path << endl;
    motion_file.close();
    write_queue.clear();
    last_write_time = system_clock::now();
}
