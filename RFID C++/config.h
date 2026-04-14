#pragma once
#include <cmath>

namespace Config {
    const float GARAGE_HEIGHT = (126.0/12.0)/24.0;
    const float GARAGE_WIDTH  = (125.0/12.0)/24.0;

    const char* LIDAR_PORT = "/dev/ttyUSB0";
    const char* RFID_PORT  = "/dev/ttyUSB1"; // change if needed

    const int RFID_BAUD = 115200;
    const int RFID_READ_POWER = 500; // 5.00 dBm to start safely
    const int RFID_WINDOW_MS = 2000;

    const float ENTRY_LINE = 210 - (atan(GARAGE_WIDTH / GARAGE_HEIGHT) * 180.0 / M_PI);
    const float EXIT_LINE  = 35 + ENTRY_LINE;

    const int EVENT_COOLDOWN_SECONDS = 3;
    const float TRIGGER_DISTANCE = 3500.0 / 20.0;
    const float ANGLE_WINDOW = 10.0;
    const int MAX_SPOTS = 36;
}
