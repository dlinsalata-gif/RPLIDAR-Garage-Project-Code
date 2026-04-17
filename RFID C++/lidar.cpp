#include "config.h"
#include "state.h"
#include "rfid.h"
#include "rfid_utils.h"

#include <iostream>
#include <chrono>
#include <thread>
#include <cmath>
#include <cstdlib>

// ===== DEBUG SWITCH =====
const bool DEBUG_LIDAR = true;

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

        // ===== DEBUG PRINT =====
        if (DEBUG_LIDAR) {
            if (distance > 0 && distance < 300) { // filter noise
                std::cout << "[LIDAR] angle=" << angle
                          << " distance=" << distance << std::endl;
            }
        }

        if (distance <= 0 || distance > Config::TRIGGER_DISTANCE) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        auto now = std::chrono::steady_clock::now();

        // ===== ENTRY =====
        if (std::fabs(angle - Config::ENTRY_LINE) < Config::ANGLE_WINDOW) {
            if (!entry_active) {
                if (std::chrono::duration_cast<std::chrono::seconds>(now - last_entry_event).count()
                    >= Config::EVENT_COOLDOWN_SECONDS) {

                    bool ignore_teacher = recent_teacher_detected();

                    std::lock_guard<std::mutex> lock(shared_state.mtx);

                    if (ignore_teacher) {
                        shared_state.last_event = "ENTER_IGNORED_TEACHER";
                        shared_state.last_event_time = "NOW";
                        std::cout << "[EVENT] ENTER ignored (teacher)" << std::endl;
                    } else {
                        if (shared_state.car_count < Config::MAX_SPOTS) {
                            shared_state.car_count++;
                        }
                        shared_state.last_event = "ENTER";
                        shared_state.last_event_time = "NOW";
                        std::cout << "[EVENT] ENTER -> " << shared_state.car_count << std::endl;
                    }

                    last_entry_event = now;
                }
                entry_active = true;
            }
        } else {
            entry_active = false;
        }

        // ===== EXIT =====
        if (std::fabs(angle - Config::EXIT_LINE) < Config::ANGLE_WINDOW) {
            if (!exit_active) {
                if (std::chrono::duration_cast<std::chrono::seconds>(now - last_exit_event).count()
                    >= Config::EVENT_COOLDOWN_SECONDS) {

                    bool ignore_teacher = recent_teacher_detected();

                    std::lock_guard<std::mutex> lock(shared_state.mtx);

                    if (ignore_teacher) {
                        shared_state.last_event = "EXIT_IGNORED_TEACHER";
                        shared_state.last_event_time = "NOW";
                        std::cout << "[EVENT] EXIT ignored (teacher)" << std::endl;
                    } else {
                        if (shared_state.car_count > 0) {
                            shared_state.car_count--;
                        }
                        shared_state.last_event = "EXIT";
                        shared_state.last_event_time = "NOW";
                        std::cout << "[EVENT] EXIT -> " << shared_state.car_count << std::endl;
                    }

                    last_exit_event = now;
                }
                exit_active = true;
            }
        } else {
            exit_active = false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
