#include "rfid.h"
#include <thread>
#include <iostream>

int main() {
    std::cout << "Starting RFID test...\n";

    std::thread rfid_thread(start_rfid);
    rfid_thread.join(); // run forever

    return 0;
}
