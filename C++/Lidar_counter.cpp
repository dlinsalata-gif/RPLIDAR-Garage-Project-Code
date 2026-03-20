// lidar.cpp
#include "config.h"
#include "state.h"
#include <iostream>
#include <chrono>
#include <thread>

// Placeholder — will connect to real SDK
float fake_read_angle() { return rand() % 360; }
float fake_read_distance() { return rand() % 4000; }

void start_lidar() {
    std::cout << "Lidar running" << std::endl;

    bool entry_active = false;
    bool exit_active = false;

    auto last_entry_event = std::chrono::steady_clock::now();
    auto last_exit_event  = std::chrono::steady_clock::now();

    while (true) {
        float angle = fake_read_angle();
        float distance = fake_read_distance();

        if (distance <= 0 || distance > Config::TRIGGER_DISTANCE)
            continue;

        auto now = std::chrono::steady_clock::now();

        // ===== ENTRY =====
        if (fabs(angle - Config::ENTRY_LINE) < Config::ANGLE_WINDOW) {
            if (!entry_active) {
                if (std::chrono::duration_cast<std::chrono::seconds>(now - last_entry_event).count()
                    >= Config::EVENT_COOLDOWN_SECONDS) {

                    std::lock_guard<std::mutex> lock(shared_state.mtx);

                    if (shared_state.car_count < Config::MAX_SPOTS) {
                        shared_state.car_count++;
                        shared_state.last_event = "ENTER";
                        shared_state.last_event_time = "NOW";
                    }

                    last_entry_event = now;
                    std::cout << "ENTER → " << shared_state.car_count << std::endl;
                }
                entry_active = true;
            }
        } else {
            entry_active = false;
        }

        // ===== EXIT =====
        if (fabs(angle - Config::EXIT_LINE) < Config::ANGLE_WINDOW) {
            if (!exit_active) {
                if (std::chrono::duration_cast<std::chrono::seconds>(now - last_exit_event).count()
                    >= Config::EVENT_COOLDOWN_SECONDS) {

                    std::lock_guard<std::mutex> lock(shared_state.mtx);

                    if (shared_state.car_count > 0) {
                        shared_state.car_count--;
                        shared_state.last_event = "EXIT";
                        shared_state.last_event_time = "NOW";
                    }

                    last_exit_event = now;
                    std::cout << "EXIT → " << shared_state.car_count << std::endl;
                }
                exit_active = true;
            }
        } else {
            exit_active = false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
