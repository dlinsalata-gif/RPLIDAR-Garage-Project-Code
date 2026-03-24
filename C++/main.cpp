#include "web.h"
#include <thread>

void start_lidar();

int main() {
    std::thread lidar_thread(start_lidar);
    lidar_thread.detach();

    start_web();  // 👈 THIS MUST RUN

    return 0;
}
