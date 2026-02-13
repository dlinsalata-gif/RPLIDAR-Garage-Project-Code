import threading

class GarageState:
    def __init__(self):
        self.car_count = 0
        self.last_event = "None"
        self.last_event_time = "N/A"
        self.lock = threading.Lock()

# This instance is shared across your entire application
shared_state = GarageState()
