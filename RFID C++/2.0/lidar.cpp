#include "config.h"
#include "state.h"
#include "rfid.h"

#include <iostream>
#include <chrono>
#include <thread>
#include <cmath>

#include "sl_lidar.h"
#include "sl_lidar_driver.h"

using namespace sl;

static const int LIDAR_BAUD = 115200;
static const bool DEBUG_LIDAR = true;

static float get_angle(const sl_lidar_response_measurement_node_hq_t& node) {
    return node.angle_z_q14 * 90.0f / (1 << 14);
}

static float get_distance(const sl_lidar_response_measurement_node_hq_t& node) {
    return node.dist_mm_q2 / 4.0f;
}

void start_lidar() {
    std::cout << "Lidar running (RPLIDAR A1 real SDK)" << std::endl;

    ILidarDriver* drv = *createLidarDriver();
    if (!drv) {
        std::cerr << "Failed to create lidar driver\n";
        return;
    }

    IChannel* channel = *createSerialPortChannel(Config::LIDAR_PORT, LIDAR_BAUD);
    if (!channel) {
        std::cerr << "Failed to create lidar channel\n";
        delete drv;
        return;
    }

    if (SL_IS_FAIL(drv->connect(channel))) {
        std::cerr << "Failed to connect to lidar on " << Config::LIDAR_PORT << "\n";
        delete drv;
        return;
    }

    drv->setMotorSpeed();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    if (SL_IS_FAIL(drv->startScan(0, 1))) {
        std::cerr << "Failed to start lidar scan\n";
        drv->setMotorSpeed(0);
        drv->disconnect();
        delete drv;
        return;
    }

    bool entry_active = false;
    bool exit_active = false;

    auto last_entry_event = std::chrono::steady_clock::now() - std::chrono::seconds(Config::EVENT_COOLDOWN_SECONDS);
    auto last_exit_event  = std::chrono::steady_clock::now() - std::chrono::seconds(Config::EVENT_COOLDOWN_SECONDS);

    while (true) {
        sl_lidar_response_measurement_node_hq_t nodes[8192];
        size_t count = sizeof(nodes) / sizeof(nodes[0]);

        if (SL_IS_FAIL(drv->grabScanDataHq(nodes, count))) {
            std::cerr << "Failed to grab lidar scan\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        drv->ascendScanData(nodes, count);

        for (size_t i = 0; i < count; i++) {
            float angle = get_angle(nodes[i]);
            float distance = get_distance(nodes[i]);
            int quality = nodes[i].quality;

            if (quality == 0 || distance <= 0 || distance > Config::TRIGGER_DISTANCE) {
                continue;
            }

            if (DEBUG_LIDAR) {
                std::cout << "[LIDAR] angle=" << angle
                          << " distance=" << distance << std::endl;
            }

            auto now = std::chrono::steady_clock::now();

            // ===== ENTRY LINE =====
            if (std::fabs(angle - Config::ENTRY_LINE) < Config::ANGLE_WINDOW) {
                if (!entry_active) {
                    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_entry_event).count()
                        >= Config::EVENT_COOLDOWN_SECONDS) {

                        bool ignore_teacher = recent_teacher_detected();

                        std::lock_guard<std::mutex> lock(shared_state.mtx);

                        if (ignore_teacher) {
                            shared_state.last_event = "ENTER_IGNORED_TEACHER";
                            shared_state.last_event_time = "NOW";
                            std::cout << "[EVENT] ENTER ignored (teacher)\n";
                        } else {
                            if (shared_state.car_count < Config::MAX_SPOTS) {
                                shared_state.car_count++;
                            }

                            shared_state.last_event = "ENTER";
                            shared_state.last_event_time = "NOW";

                            std::cout << "[EVENT] ENTER -> "
                                      << shared_state.car_count
                                      << " angle=" << angle
                                      << " distance=" << distance
                                      << std::endl;
                        }

                        last_entry_event = now;
                    }

                    entry_active = true;
                }
            } else {
                entry_active = false;
            }

            // ===== EXIT LINE =====
            if (std::fabs(angle - Config::EXIT_LINE) < Config::ANGLE_WINDOW) {
                if (!exit_active) {
                    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_exit_event).count()
                        >= Config::EVENT_COOLDOWN_SECONDS) {

                        bool ignore_teacher = recent_teacher_detected();

                        std::lock_guard<std::mutex> lock(shared_state.mtx);

                        if (ignore_teacher) {
                            shared_state.last_event = "EXIT_IGNORED_TEACHER";
                            shared_state.last_event_time = "NOW";
                            std::cout << "[EVENT] EXIT ignored (teacher)\n";
                        } else {
                            if (shared_state.car_count > 0) {
                                shared_state.car_count--;
                            }

                            shared_state.last_event = "EXIT";
                            shared_state.last_event_time = "NOW";

                            std::cout << "[EVENT] EXIT -> "
                                      << shared_state.car_count
                                      << " angle=" << angle
                                      << " distance=" << distance
                                      << std::endl;
                        }

                        last_exit_event = now;
                    }

                    exit_active = true;
                }
            } else {
                exit_active = false;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    drv->stop();
    drv->setMotorSpeed(0);
    drv->disconnect();
    delete drv;
}
