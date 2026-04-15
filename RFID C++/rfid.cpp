#include "rfid.h"
#include "config.h"
#include "state.h"
#include "rfid_utils.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <thread>
#include <chrono>
#include <cstring>
#include <cstdint>
#include <cerrno>

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

// ===== SparkFun / ThingMagic constants =====
static constexpr uint8_t MAX_MSG_SIZE = 255;

static constexpr uint8_t TMR_SR_OPCODE_VERSION                    = 0x03;
static constexpr uint8_t TMR_SR_OPCODE_SET_BAUD_RATE              = 0x06;
static constexpr uint8_t TMR_SR_OPCODE_READ_TAG_ID_MULTIPLE       = 0x22;
static constexpr uint8_t TMR_SR_OPCODE_MULTI_PROTOCOL_TAG_OP      = 0x2F;
static constexpr uint8_t TMR_SR_OPCODE_SET_ANTENNA_PORT           = 0x91;
static constexpr uint8_t TMR_SR_OPCODE_SET_READ_TX_POWER          = 0x92;
static constexpr uint8_t TMR_SR_OPCODE_SET_TAG_PROTOCOL           = 0x93;
static constexpr uint8_t TMR_SR_OPCODE_SET_REGION                 = 0x97;
static constexpr uint8_t TMR_SR_OPCODE_SET_READER_OPTIONAL_PARAMS = 0x9A;

static constexpr uint8_t ALL_GOOD                       = 0;
static constexpr uint8_t ERROR_COMMAND_RESPONSE_TIMEOUT = 1;
static constexpr uint8_t ERROR_CORRUPT_RESPONSE         = 2;
static constexpr uint8_t ERROR_WRONG_OPCODE_RESPONSE    = 3;
static constexpr uint8_t ERROR_UNKNOWN_OPCODE           = 4;
static constexpr uint8_t RESPONSE_IS_TEMPERATURE        = 5;
static constexpr uint8_t RESPONSE_IS_KEEPALIVE          = 6;
static constexpr uint8_t RESPONSE_IS_TEMPTHROTTLE       = 7;
static constexpr uint8_t RESPONSE_IS_TAGFOUND           = 8;
static constexpr uint8_t RESPONSE_IS_NOTAGFOUND         = 9;
static constexpr uint8_t RESPONSE_IS_UNKNOWN            = 10;
static constexpr uint8_t RESPONSE_SUCCESS               = 11;
static constexpr uint8_t RESPONSE_FAIL                  = 12;
static constexpr uint8_t RESPONSE_IS_HIGHRETURNLOSS     = 13;

static constexpr uint8_t REGION_NORTHAMERICA = 0x01;
static constexpr uint16_t COMMAND_TIME_OUT = 2000;

// ===== Pi serial wrapper =====
class LinuxSerial {
public:
    LinuxSerial() = default;
    ~LinuxSerial() { closePort(); }

    bool openPort(const std::string& path, int baud) {
        closePort();

        fd_ = open(path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (fd_ < 0) {
            std::cerr << "Failed to open " << path << ": " << strerror(errno) << "\n";
            return false;
        }

        termios tty{};
        if (tcgetattr(fd_, &tty) != 0) {
            std::cerr << "tcgetattr failed: " << strerror(errno) << "\n";
            closePort();
            return false;
        }

        cfmakeraw(&tty);
        tty.c_cflag |= (CLOCAL | CREAD);
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CRTSCTS;
        tty.c_cflag &= ~PARENB;
        tty.c_cflag &= ~CSIZE;
        tty.c_cflag |= CS8;

        speed_t speed = baudToTermios(baud);
        cfsetispeed(&tty, speed);
        cfsetospeed(&tty, speed);

        tty.c_cc[VMIN] = 0;
        tty.c_cc[VTIME] = 0;

        if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
            std::cerr << "tcsetattr failed: " << strerror(errno) << "\n";
            closePort();
            return false;
        }

        tcflush(fd_, TCIOFLUSH);
        return true;
    }

    void closePort() {
        if (fd_ >= 0) {
            close(fd_);
            fd_ = -1;
        }
    }

    bool isOpen() const { return fd_ >= 0; }

    int available() {
        if (fd_ < 0) return 0;
        int bytes = 0;
        if (ioctl(fd_, FIONREAD, &bytes) == -1) return 0;
        return bytes;
    }

    int readByte() {
        if (fd_ < 0) return -1;
        uint8_t b;
        ssize_t n = ::read(fd_, &b, 1);
        if (n == 1) return b;
        return -1;
    }

    void writeBytes(const uint8_t* data, size_t len) {
        if (fd_ < 0) return;
        ::write(fd_, data, len);
        tcdrain(fd_);
    }

    void flushInput() {
        if (fd_ >= 0) tcflush(fd_, TCIFLUSH);
    }

    void flushBoth() {
        if (fd_ >= 0) tcflush(fd_, TCIOFLUSH);
    }

private:
    int fd_ = -1;

    speed_t baudToTermios(int baud) {
        switch (baud) {
            case 9600:   return B9600;
            case 19200:  return B19200;
            case 38400:  return B38400;
            case 57600:  return B57600;
            case 115200: return B115200;
            default:     return B115200;
        }
    }
};

// ===== RFID port of SparkFun logic =====
class PiRFID {
public:
    bool begin(const std::string& port, int baud) {
        port_ = port;
        baud_ = baud;
        return serial_.openPort(port_, baud_);
    }

    void enableDebugging(bool en = true) { debug_ = en; }

    void setBaud(long baudRate) {
        uint8_t size = sizeof(baudRate);
        uint8_t data[sizeof(long)];
        for (uint8_t x = 0; x < size; x++) {
            data[x] = (uint8_t)(baudRate >> (8 * (size - 1 - x)));
        }
        sendMessage(TMR_SR_OPCODE_SET_BAUD_RATE, data, size, COMMAND_TIME_OUT, false);
    }

    void getVersion() {
        sendMessage(TMR_SR_OPCODE_VERSION);
    }

    void setRegion(uint8_t region) {
        sendMessage(TMR_SR_OPCODE_SET_REGION, &region, sizeof(region));
    }

    void setReadPower(int16_t powerSetting) {
        if (powerSetting > 2700) powerSetting = 2700;
        uint8_t size = sizeof(powerSetting);
        uint8_t data[sizeof(int16_t)];
        for (uint8_t x = 0; x < size; x++) {
            data[x] = (uint8_t)(powerSetting >> (8 * (size - 1 - x)));
        }
        sendMessage(TMR_SR_OPCODE_SET_READ_TX_POWER, data, size);
    }

    void setAntennaPort() {
        uint8_t configBlob[] = {0x01, 0x01};
        sendMessage(TMR_SR_OPCODE_SET_ANTENNA_PORT, configBlob, sizeof(configBlob));
    }

    void setTagProtocol(uint8_t protocol = 0x05) {
        uint8_t data[2];
        data[0] = 0;
        data[1] = protocol;
        sendMessage(TMR_SR_OPCODE_SET_TAG_PROTOCOL, data, sizeof(data));
    }

    void enableReadFilter() {
        setReaderConfiguration(0x0C, 0x01);
    }

    void disableReadFilter() {
        setReaderConfiguration(0x0C, 0x00);
    }

    void setReaderConfiguration(uint8_t option1, uint8_t option2) {
        uint8_t data[3];
        data[0] = 1;
        data[1] = option1;
        data[2] = option2;
        sendMessage(TMR_SR_OPCODE_SET_READER_OPTIONAL_PARAMS, data, sizeof(data));
    }

    void startReading() {
        disableReadFilter();
        uint8_t configBlob[] = {
            0x00, 0x00, 0x01, 0x22, 0x00, 0x00, 0x05, 0x07,
            0x22, 0x10, 0x00, 0x1B, 0x03, 0xE8, 0x01, 0xFF
        };
        sendMessage(TMR_SR_OPCODE_MULTI_PROTOCOL_TAG_OP, configBlob, sizeof(configBlob));
    }

    void stopReading() {
        uint8_t configBlob[] = {0x00, 0x00, 0x02};
        sendMessage(TMR_SR_OPCODE_MULTI_PROTOCOL_TAG_OP, configBlob, sizeof(configBlob), COMMAND_TIME_OUT, false);
    }

    bool check() {
        while (serial_.available()) {
            int incoming = serial_.readByte();
            if (incoming < 0) break;
            uint8_t incomingData = static_cast<uint8_t>(incoming);

            if (head_ == 0 && incomingData != 0xFF) {
                // ignore until header
            } else {
                msg[head_++] = incomingData;
                head_ %= MAX_MSG_SIZE;

                if ((head_ > 0) && (head_ == msg[1] + 7)) {
                    for (uint8_t x = head_; x < MAX_MSG_SIZE; x++) msg[x] = 0;
                    head_ = 0;

                    if (debug_) {
                        std::cerr << "response:";
                        printMessageArray();
                    }
                    return true;
                }
            }
        }
        return false;
    }

    uint8_t parseResponse() {
        uint8_t msgLength = msg[1] + 7;
        uint8_t opCode = msg[2];

        uint16_t messageCRC = calculateCRC(&msg[1], msgLength - 3);
        if ((msg[msgLength - 2] != (messageCRC >> 8)) ||
            (msg[msgLength - 1] != (messageCRC & 0xFF))) {
            return ERROR_CORRUPT_RESPONSE;
        }

        if (opCode == TMR_SR_OPCODE_READ_TAG_ID_MULTIPLE) {
            if (msg[1] == 0x00) {
                uint16_t statusMsg = 0;
                for (uint8_t x = 0; x < 2; x++) {
                    statusMsg |= (uint32_t)msg[3 + x] << (8 * (1 - x));
                }

                if (statusMsg == 0x0400) return RESPONSE_IS_KEEPALIVE;
                if (statusMsg == 0x0504) return RESPONSE_IS_TEMPTHROTTLE;
                if (statusMsg == 0x0505) return RESPONSE_IS_HIGHRETURNLOSS;
                return RESPONSE_IS_UNKNOWN;
            } else if (msg[1] == 0x08) {
                return RESPONSE_IS_UNKNOWN;
            } else if (msg[1] == 0x0a) {
                return RESPONSE_IS_TEMPERATURE;
            } else {
                return RESPONSE_IS_TAGFOUND;
            }
        }

        return ERROR_UNKNOWN_OPCODE;
    }

    uint8_t getTagDataBytes() {
        uint16_t tagDataLength = 0;
        for (uint8_t x = 0; x < 2; x++) {
            tagDataLength |= (uint16_t)msg[24 + x] << (8 * (1 - x));
        }
        uint8_t tagDataBytes = tagDataLength / 8;
        if (tagDataLength % 8 > 0) tagDataBytes++;
        return tagDataBytes;
    }

    uint8_t getTagEPCBytes() {
        uint16_t epcBits = 0;
        uint8_t tagDataBytes = getTagDataBytes();
        for (uint8_t x = 0; x < 2; x++) {
            epcBits |= (uint16_t)msg[27 + tagDataBytes + x] << (8 * (1 - x));
        }
        uint8_t epcBytes = epcBits / 8;
        epcBytes -= 4;
        return epcBytes;
    }

    uint16_t getTagTimestamp() {
        uint32_t timeStamp = 0;
        for (uint8_t x = 0; x < 4; x++) {
            timeStamp |= (uint32_t)msg[17 + x] << (8 * (3 - x));
        }
        return timeStamp;
    }

    uint32_t getTagFreq() {
        uint32_t freq = 0;
        for (uint8_t x = 0; x < 3; x++) {
            freq |= (uint32_t)msg[14 + x] << (8 * (2 - x));
        }
        return freq;
    }

    int8_t getTagRSSI() {
        return (msg[12] - 256);
    }

    std::string getCurrentEPC() {
        uint8_t tagEPCBytes = getTagEPCBytes();
        std::ostringstream oss;
        for (uint8_t x = 0; x < tagEPCBytes; x++) {
            oss << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<int>(msg[31 + x]);
        }
        return oss.str();
    }

    bool setupModule() {
        if (!begin(Config::RFID_PORT, Config::RFID_BAUD)) {
            return false;
        }

        // Give the CH340 + reader time to settle after opening
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        serial_.flushBoth();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        serial_.flushBoth();

        // Try getVersion a few times in case startup bytes were still present
        bool gotVersion = false;
        for (int attempt = 0; attempt < 3; attempt++) {
            getVersion();

            if (msg[0] == ALL_GOOD) {
                gotVersion = true;
                break;
            }

            if (msg[0] == ERROR_WRONG_OPCODE_RESPONSE) {
                std::cerr << "RFID may already be continuously reading. Sending stopReading...\n";
                stopReading();
                std::this_thread::sleep_for(std::chrono::milliseconds(1500));
                serial_.flushBoth();
            } else {
                std::cerr << "getVersion attempt " << (attempt + 1)
                          << " failed. msg[0]=" << (int)msg[0] << "\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
                serial_.flushBoth();
            }
        }

        if (!gotVersion) {
            std::cerr << "RFID getVersion failed after retries. msg[0]=" << (int)msg[0] << "\n";
            return false;
        }

        setTagProtocol();
        if (msg[0] != ALL_GOOD) {
            std::cerr << "setTagProtocol failed. msg[0]=" << (int)msg[0] << "\n";
        }

        setAntennaPort();
        if (msg[0] != ALL_GOOD) {
            std::cerr << "setAntennaPort failed. msg[0]=" << (int)msg[0] << "\n";
        }

        setRegion(REGION_NORTHAMERICA);
        if (msg[0] != ALL_GOOD) {
            std::cerr << "setRegion failed. msg[0]=" << (int)msg[0] << "\n";
        }

        setReadPower(Config::RFID_READ_POWER);
        if (msg[0] != ALL_GOOD) {
            std::cerr << "setReadPower failed. msg[0]=" << (int)msg[0] << "\n";
        }

        return true;
    }

    void sendMessage(uint8_t opcode, uint8_t* data = nullptr, uint8_t size = 0,
                     uint16_t timeOut = COMMAND_TIME_OUT, bool waitForResponse = true) {
        msg[1] = size;
        msg[2] = opcode;

        for (uint8_t x = 0; x < size; x++) {
            msg[3 + x] = data[x];
        }

        sendCommand(timeOut, waitForResponse);
    }

    uint8_t msg[MAX_MSG_SIZE] = {0};

private:
    LinuxSerial serial_;
    std::string port_;
    int baud_ = 115200;
    uint8_t head_ = 0;
    bool debug_ = false;

    void sendCommand(uint16_t timeOut = COMMAND_TIME_OUT, bool waitForResponse = true) {
        msg[0] = 0xFF;
        uint8_t messageLength = msg[1];
        uint8_t opcode = msg[2];

        uint16_t crc = calculateCRC(&msg[1], messageLength + 2);
        msg[messageLength + 3] = crc >> 8;
        msg[messageLength + 4] = crc & 0xFF;

        if (debug_) {
            std::cerr << "sendCommand:";
            printMessageArray();
        }

        serial_.flushInput();
        serial_.writeBytes(msg, messageLength + 5);

        if (!waitForResponse) return;

        auto startTime = std::chrono::steady_clock::now();

        while (serial_.available() == 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startTime).count();

            if (elapsed > timeOut) {
                msg[0] = ERROR_COMMAND_RESPONSE_TIMEOUT;
                return;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        messageLength = MAX_MSG_SIZE - 1;
        uint8_t spot = 0;

        while (spot < messageLength) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startTime).count();

            if (elapsed > timeOut) {
                msg[0] = ERROR_COMMAND_RESPONSE_TIMEOUT;
                return;
            }

            if (serial_.available()) {
                int value = serial_.readByte();
                if (value < 0) continue;

                msg[spot] = static_cast<uint8_t>(value);

                if (spot == 1) {
                    messageLength = msg[1] + 7;
                }

                spot++;
                spot %= MAX_MSG_SIZE;
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        crc = calculateCRC(&msg[1], messageLength - 3);
        if ((msg[messageLength - 2] != (crc >> 8)) ||
            (msg[messageLength - 1] != (crc & 0xFF))) {
            msg[0] = ERROR_CORRUPT_RESPONSE;
            return;
        }

        if (msg[2] != opcode) {
            msg[0] = ERROR_WRONG_OPCODE_RESPONSE;
            return;
        }

        msg[0] = ALL_GOOD;
    }

    void printMessageArray() {
        uint8_t amtToPrint = msg[1] + 5;
        if (amtToPrint > MAX_MSG_SIZE) amtToPrint = MAX_MSG_SIZE;

        for (uint16_t x = 0; x < amtToPrint; x++) {
            std::cerr << " [";
            if (msg[x] < 0x10) std::cerr << "0";
            std::cerr << std::uppercase << std::hex << static_cast<int>(msg[x]);
            std::cerr << "]";
        }
        std::cerr << std::dec << "\n";
    }

    uint16_t calculateCRC(uint8_t* u8Buf, uint8_t len) {
        static uint16_t crctable[] = {
            0x0000, 0x1021, 0x2042, 0x3063,
            0x4084, 0x50A5, 0x60C6, 0x70E7,
            0x8108, 0x9129, 0xA14A, 0xB16B,
            0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
        };

        uint16_t crc = 0xFFFF;

        for (uint8_t i = 0; i < len; i++) {
            crc = ((crc << 4) | (u8Buf[i] >> 4)) ^ crctable[crc >> 12];
            crc = ((crc << 4) | (u8Buf[i] & 0x0F)) ^ crctable[crc >> 12];
        }

        return crc;
    }
};

static PiRFID g_rfid;

bool recent_teacher_detected() {
    std::lock_guard<std::mutex> lock(shared_state.mtx);

    if (!shared_state.rfid_seen) return false;

    auto age_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - shared_state.last_rfid_time
    ).count();

    return age_ms <= Config::RFID_WINDOW_MS &&
           isTeacherTag(shared_state.last_rfid_tag);
}

std::string get_last_rfid_tag() {
    std::lock_guard<std::mutex> lock(shared_state.mtx);
    return shared_state.last_rfid_tag;
}

void start_rfid() {
    std::cerr << "Starting RFID on " << Config::RFID_PORT << "\n";
    g_rfid.enableDebugging(true);

    if (!g_rfid.setupModule()) {
        std::cerr << "RFID setup failed.\n";
        return;
    }

    std::cerr << "RFID setup OK. Starting continuous read...\n";
    g_rfid.startReading();

    if (g_rfid.msg[0] != ALL_GOOD) {
        std::cerr << "startReading warning. msg[0]=" << (int)g_rfid.msg[0] << "\n";
    }

    while (true) {
        if (g_rfid.check()) {
            uint8_t responseType = g_rfid.parseResponse();

            if (responseType == RESPONSE_IS_KEEPALIVE) {
                std::cerr << "RFID scanning...\n";
            } else if (responseType == RESPONSE_IS_TAGFOUND) {
                RFIDTagRead tag;
                tag.rssi = g_rfid.getTagRSSI();
                tag.freq = g_rfid.getTagFreq();
                tag.timestamp = g_rfid.getTagTimestamp();
                tag.epc = g_rfid.getCurrentEPC();

                {
                    std::lock_guard<std::mutex> lock(shared_state.mtx);
                    shared_state.last_rfid_tag = tag.epc;
                    shared_state.last_rfid_time = std::chrono::steady_clock::now();
                    shared_state.rfid_seen = true;
                }

                std::cerr << "RFID TAG epc[" << tag.epc
                          << "] rssi[" << tag.rssi
                          << "] freq[" << tag.freq
                          << "] time[" << tag.timestamp
                          << "]\n";
            } else if (responseType == ERROR_CORRUPT_RESPONSE) {
                std::cerr << "RFID Bad CRC\n";
            } else if (responseType == RESPONSE_IS_HIGHRETURNLOSS) {
                std::cerr << "RFID High return loss, check antenna!\n";
            } else {
                std::cerr << "RFID Unknown response type: " << (int)responseType << "\n";
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}
