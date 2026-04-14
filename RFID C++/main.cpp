#include "web.h"
#include "rfid.h"

#include <thread>
#include <iostream>

void start_lidar();

int main() {
    std::cout << "MAIN STARTED\n";

    std::thread lidar_thread(start_lidar);
    lidar_thread.detach();

    std::thread rfid_thread(start_rfid);
    rfid_thread.detach();

    std::cout << "STARTING WEB\n";
    start_web();

    return 0;
}
