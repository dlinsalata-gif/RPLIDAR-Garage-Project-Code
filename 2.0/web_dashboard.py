from lidar_counter import car_count, last_event, last_event_time, lock
from config import MAX_SPOTS

app = Flask(__name__)


@app.route("/")
def index():
    with lock:
        open_spots = MAX_SPOTS - car_count
        event = last_event
        event_time = last_event_time

    return render_template(
        "index.html",
        open_spots=open_spots,
        event=event,
        event_time=event_time
    )
