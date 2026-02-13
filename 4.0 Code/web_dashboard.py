from flask import Flask, render_template, jsonify
from state import shared_state
from config import MAX_SPOTS

app = Flask(__name__)

@app.route("/")
def index():
    return render_template("index.html")

@app.route("/status")
def status():
    with shared_state.lock:
        open_spots = MAX_SPOTS - shared_state.car_count
        if open_spots < 0:
            open_spots = 0

        return jsonify(
            open_spots=open_spots,
            event=shared_state.last_event,
            event_time=shared_state.last_event_time
        )

def start_web():
    app.run(host="0.0.0.0", port=5000, threaded=True, use_reloader=False)
