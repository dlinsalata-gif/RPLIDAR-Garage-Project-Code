from rplidar import RPLidar
from config import *
import time
from state import shared_state

def start_lidar():
    lidar = RPLidar(LIDAR_PORT)
    lidar.start_motor()
    print("Lidar running")

    entry_active = False
    exit_active = False

    # Cooldown timers (use monotonic time so it won't break if system clock changes)
    last_entry_event_t = 0.0
    last_exit_event_t = 0.0

    try:
        for scan in lidar.iter_scans():
            now = time.monotonic()

            for _, angle, distance in scan:

                if distance <= 0 or distance > TRIGGER_DISTANCE:
                    continue

                # ===== ENTRY LINE =====
                if abs(angle - ENTRY_LINE) < ANGLE_WINDOW:
                    if not entry_active:
                        # 3-second cooldown
                        if (now - last_entry_event_t) >= EVENT_COOLDOWN_SECONDS:
                            with shared_state.lock:
                                if shared_state.car_count < MAX_SPOTS:
                                    shared_state.car_count += 1
                                    shared_state.last_event = "ENTER"
                                    shared_state.last_event_time = time.strftime("%I:%M %p")
                            last_entry_event_t = now
                            print(f"ENTER → {shared_state.car_count}")
                        entry_active = True
                else:
                    entry_active = False

                # ===== EXIT LINE =====
                if abs(angle - EXIT_LINE) < ANGLE_WINDOW:
                    if not exit_active:
                        # 3-second cooldown
                        if (now - last_exit_event_t) >= EVENT_COOLDOWN_SECONDS:
                            with shared_state.lock:
                                if shared_state.car_count > 0:
                                    shared_state.car_count -= 1
                                    shared_state.last_event = "EXIT"
                                    shared_state.last_event_time = time.strftime("%I:%M %p")
                            last_exit_event_t = now
                            print(f"EXIT → {shared_state.car_count}")
                        exit_active = True
                else:
                    exit_active = False

    except Exception as e:
        print("Lidar error:", e)

    finally:
        lidar.stop_motor()
        lidar.stop()
        lidar.disconnect()
