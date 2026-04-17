#pragma once
#include <cmath>

namespace Config {
    inline constexpr const char* LIDAR_PORT = "/dev/ttyUSB0";
    inline constexpr const char* RFID_PORT  = "/dev/ttyUSB1"; // change if needed

    inline constexpr int RFID_BAUD = 115200;
    inline constexpr int RFID_READ_POWER = 1500; // 15.00 dBm
    inline constexpr int RFID_WINDOW_MS = 2000;

    inline constexpr float GARAGE_HEIGHT = (126.0f / 12.0f) / 24.0f;
    inline constexpr float GARAGE_WIDTH  = (125.0f / 12.0f) / 24.0f;

    inline constexpr float ENTRY_LINE =
        210.0f - (std::atan(GARAGE_WIDTH / GARAGE_HEIGHT) * 180.0f / static_cast<float>(M_PI));
    inline constexpr float EXIT_LINE = 35.0f + ENTRY_LINE;

    inline constexpr int EVENT_COOLDOWN_SECONDS = 3;
    inline constexpr float TRIGGER_DISTANCE = 3500.0f / 20.0f;
    inline constexpr float ANGLE_WINDOW = 10.0f;
    inline constexpr int MAX_SPOTS = 36;
}
