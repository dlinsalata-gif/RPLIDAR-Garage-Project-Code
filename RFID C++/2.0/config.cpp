namespace Config {

inline constexpr const char* LIDAR_PORT = "/dev/ttyUSB1";
inline constexpr const char* RFID_PORT  = "/dev/ttyUSB0";

// From your Python geometry
inline constexpr float GARAGE_HEIGHT = (126.0f / 12.0f) / 24.0f;
inline constexpr float GARAGE_WIDTH  = (125.0f / 12.0f) / 24.0f;

// Same trig calculation as Python
inline constexpr float ENTRY_LINE =
    210.0f - (std::atan(GARAGE_WIDTH / GARAGE_HEIGHT) * 180.0f / M_PI);

inline constexpr float EXIT_LINE = 35.0f + ENTRY_LINE;

// Same as Python:
// 3500 / 20 = 175 mm
inline constexpr float TRIGGER_DISTANCE = 175.0f;

// Same as Python
inline constexpr float ANGLE_WINDOW = 10.0f;

// Same as Python
inline constexpr int EVENT_COOLDOWN_SECONDS = 3;

// Existing
inline constexpr int MAX_SPOTS = 36;

// RFID
inline constexpr int RFID_READ_POWER = 500;
inline constexpr int RFID_WINDOW_MS = 3000;

}
