// main.cpp
#include <thread>

void start_lidar();

int main() {
    std::thread lidar_thread(start_lidar);
    lidar_thread.detach();

    // Later: start web server here

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
