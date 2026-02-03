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



from flask import Flask, render_template, jsonify
from lidar_counter import car_count, last_event, last_event_time, lock
from config import MAX_SPOTS

app = Flask(__name__)


@app.route("/")
def index():
    return render_template("index.html")


@app.route("/status")
def status():
    with lock:
        open_spots = MAX_SPOTS - car_count
        if open_spots < 0:
            open_spots = 0

        return jsonify(
            open_spots=open_spots,
            event=last_event,
            event_time=last_event_time
        )


def start_web():
    app.run(host="0.0.0.0", port=5000, threaded=True)
