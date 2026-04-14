#pragma once
#include <mutex>
#include <string>
#include <chrono>

struct GarageState {
    int car_count = 0;
    std::string last_event = "None";
    std::string last_event_time = "N/A";

    std::string last_rfid_tag = "";
    std::chrono::steady_clock::time_point last_rfid_time = std::chrono::steady_clock::now();
    bool rfid_seen = false;

    std::mutex mtx;
};

extern GarageState shared_state;
