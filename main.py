from lidar_counter import start_lidar
from web_dashboard import start_web
import threading


def main():
    threading.Thread(target=start_lidar, daemon=True).start()
    start_web()


if __name__ == "__main__":
    main()
