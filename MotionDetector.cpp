#include "MotionDetector.h"

const string ADMIN_PHONE = "3393641604";
const string FROM_PHONE = "8573550142";

MotionDetector::MotionDetector(const string camera_name, const string &motion_file_path, const int motion_threshold) {
    this->camera_name = camera_name;
    this->motion_file_path = motion_file_path;
    this->motion_threshold = motion_threshold;
    this->last_write_time = system_clock::now();
    init_twilio();
}

void MotionDetector::release() {

}

/**
  Extract multiple H264 NAL units from an AVPacket, find the IDR frame, and mark its size. If size > some threshold, mark the timestamp to a text file.
 */
void MotionDetector::send_packet(AVPacket *packet) {
    int last_IDR_index = -1;
    int last_non_IDR_index = -1;
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
            
            if (unit_type != UNIT_TYPE_IDR) {
                if (last_non_IDR_index != -1) {
                    mark_non_idr_frame_size(i - last_non_IDR_index);
                    last_non_IDR_index = -1;
                }
            } else {
                if (last_non_IDR_index == -1) {
                    last_non_IDR_index = i;
                }
            }
        }
    }
    if (last_IDR_index != -1) {
        mark_idr_frame_size(packet->size - last_IDR_index);
        last_IDR_index = -1;
    }
    if (last_non_IDR_index != -1) {
        mark_non_idr_frame_size(packet->size - last_non_IDR_index);
        last_non_IDR_index = -1;
    }
}

void MotionDetector::init_twilio() {
    char* sid = getenv("TWILIO_SID");
    char* token = getenv("TWILIO_AUTH_TOKEN");
    if (!sid || !token) {
        cerr << "Failed to retrieve twilio sid and auth token" << endl;
        exit(1);
        return;
    }
    this->m_twilio = std::make_shared<twilio::Twilio>(sid, token);
}

void MotionDetector::send_sms(string message) {
    string twilio_response;
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::ostringstream final_message;
    final_message << message << " at " << std::put_time(&tm, "%m/%d/%Y %r");
    
    bool success = m_twilio->send_message(ADMIN_PHONE, FROM_PHONE, final_message.str(), twilio_response, "", true);
    if (!success) {
        cout << "(Twilio) " << twilio_response << endl;
    }
}

void MotionDetector::mark_idr_frame_size(int size) {
    return;
//    const int WRITE_INTERVAL_MSEC = 15000;
//    if (size < this->motion_threshold) {
//        long ms_since_epoch = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
//        write_queue.push_back(ms_since_epoch);
//        cout << this->motion_file_path << ": Motion detected at " << ms_since_epoch << ". Size is " << size << endl;
//    }
//    if (duration_cast<milliseconds>(system_clock::now() - last_write_time).count() > WRITE_INTERVAL_MSEC) {
//        flush_write_queue();
//    }
}

void MotionDetector::mark_non_idr_frame_size(int size) {
    const int WRITE_INTERVAL_MSEC = 15000;
    const int ALERT_INTERVAL_MSEC = 30000;
    cout << this->camera_name << ": " << to_string(size) << endl;
    if (size < this->motion_threshold) {
        long ms_since_epoch = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        write_queue.push_back(ms_since_epoch);
        cout << this->camera_name << ": Motion detected at " << ms_since_epoch << ". Size is " << size << endl;
        if (duration_cast<milliseconds>(system_clock::now() - last_alert_time).count() > ALERT_INTERVAL_MSEC) {
            //send_sms("Motion (" + to_string(size) + ") at " + this->camera_name);
            last_alert_time = system_clock::now();
        }
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
    motion_file.close();
    write_queue.clear();
    last_write_time = system_clock::now();
}
