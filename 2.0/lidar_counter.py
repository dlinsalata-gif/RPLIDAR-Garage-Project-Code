from rplidar import RPLidar
from config import *
import time
import threading

car_count = 0
last_event = "None"
last_event_time = "N/A"
lock = threading.Lock()


def start_lidar():
    global car_count, last_event, last_event_time

    lidar = RPLidar(LIDAR_PORT)
    lidar.start_motor()
    print("Lidar running")

    entry_active = False
    exit_active = False

    try:
        for scan in lidar.iter_scans():
            for _, angle, distance in scan:

                if distance <= 0 or distance > TRIGGER_DISTANCE:
                    continue

                # ===== ENTRY LINE =====
                if abs(angle - ENTRY_LINE) < ANGLE_WINDOW:
                    if not entry_active:
                        with lock:
                            if car_count < MAX_SPOTS:
                                car_count += 1
                                last_event = "ENTER"
                                last_event_time = time.strftime("%I:%M %p")
                        entry_active = True
                        print("ENTER →", car_count)
                else:
                    entry_active = False

                # ===== EXIT LINE =====
                if abs(angle - EXIT_LINE) < ANGLE_WINDOW:
                    if not exit_active:
                        with lock:
                            if car_count > 0:
                                car_count -= 1
                                last_event = "EXIT"
                                last_event_time = time.strftime("%I:%M %p")
                        exit_active = True
                        print("EXIT →", car_count)
                else:
                    exit_active = False

    except Exception as e:
        print("Lidar error:", e)

    finally:
        lidar.stop_motor()
        lidar.stop()
        lidar.disconnect()
