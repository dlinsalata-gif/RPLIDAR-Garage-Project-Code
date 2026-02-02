from flask import Flask, render_template
from lidar_counter import car_count, lock

app = Flask(__name__)


@app.route("/")
def index():
    with lock:
        count = car_count
    return render_template("index.html", count=count)


def start_web():
    app.run(host="0.0.0.0", port=5000)
