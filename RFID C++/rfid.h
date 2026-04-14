#pragma once

#include <string>
#include <vector>
#include <cstdint>

struct RFIDTagRead {
    std::string epc;
    int rssi = 0;
    long freq = 0;
    long timestamp = 0;
};

void start_rfid();
bool recent_teacher_detected();
std::string get_last_rfid_tag();
