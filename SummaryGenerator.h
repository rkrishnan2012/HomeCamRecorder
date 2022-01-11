#ifndef HOMECAMRECORDER_SUMMARYGENERATOR_H
#define HOMECAMRECORDER_SUMMARYGENERATOR_H

#include <iostream>
#include <thread>
#include <csignal>
#include <utility>
#include <vector>
#include <unistd.h>
#include <fstream>
#include <filesystem>
#include <algorithm>

extern "C" {
#include <libavcodec/packet.h>
};

#include "Muxer.h"

using namespace std;
using namespace std::chrono;

#ifdef __APPLE__
using namespace std::__fs::filesystem;
#else
using namespace std::filesystem;
#endif

class SummaryGenerator {
public:
    SummaryGenerator(
        const string &recordings_dir, 
        const string &basename, 
        const string &output_file);

    void run();
    void release();

private:
    const string recordings_dir;
    const string basename;
    const string output_file;

    vector<pair<string, long>> get_video_files();
    vector<long> get_motion_timestamps();
    void add_video(RotatingFileMuxer *muxer, const string video_file_path, const long start_timestamp, vector<long> motion_timestamps);
};


#endif //HOMECAMRECORDER_SUMMARYGENERATOR_H
