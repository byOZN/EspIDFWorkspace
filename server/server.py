"""
ESP32 CSV receiver server
Запуск: python server.py
POST http://<ip>:8080/api/upload  — приём CSV от ESP32
GET  http://<ip>:8080/            — веб-интерфейс (templates/index.html)
"""

import csv
import io
import os
from datetime import datetime
from flask import Flask, request, jsonify, render_template

app = Flask(__name__)

UPLOAD_DIR = "uploads"
os.makedirs(UPLOAD_DIR, exist_ok=True)


# ──────────────────────────────────────────────────────────────────────────────
# Утилиты
# ──────────────────────────────────────────────────────────────────────────────

def parse_csv(text: str):
    """Парсит CSV (index,value) → (list[int] values, list[dict] rows)."""
    reader = csv.DictReader(io.StringIO(text))
    rows, values = [], []
    for i, row in enumerate(reader):
        try:
            idx = int(row.get("index", i))
            val = int(row.get("value", 0))
            values.append(val)
            rows.append({"row_num": i + 1, "index": idx, "value": val})
        except (ValueError, KeyError):
            pass
    return values, rows


def file_stats(filename: str):
    path = os.path.join(UPLOAD_DIR, filename)
    text = open(path, encoding="utf-8").read()
    values, rows = parse_csv(text)
    if not values:
        return None
    return {
        "filename": filename,
        "received": datetime.fromtimestamp(os.path.getmtime(path))
                    .strftime("%Y-%m-%d %H:%M:%S"),
        "count":    len(values),
        "min":      min(values),
        "max":      max(values),
        "avg":      sum(values) / len(values),
        "values":   values,
        "rows":     rows,
    }


def list_files():
    return sorted(
        [f for f in os.listdir(UPLOAD_DIR) if f.endswith(".csv")],
        key=lambda f: os.path.getmtime(os.path.join(UPLOAD_DIR, f))
    )


# ──────────────────────────────────────────────────────────────────────────────
# Маршруты
# ──────────────────────────────────────────────────────────────────────────────

@app.route("/api/upload", methods=["POST"])
def upload():
    data = request.get_data(as_text=True)
    if not data:
        return jsonify({"error": "empty body"}), 400

    ts       = datetime.now().strftime("%Y%m%d_%H%M%S")
    filename = f"adc_{ts}.csv"
    path     = os.path.join(UPLOAD_DIR, filename)

    with open(path, "w", encoding="utf-8") as f:
        f.write(data)

    values, _ = parse_csv(data)
    print(f"\n[{datetime.now():%H:%M:%S}] Received {filename}")
    print(f"  Samples : {len(values)}")
    if values:
        print(f"  Min/Max : {min(values)} / {max(values)}")
        print(f"  Average : {sum(values) / len(values):.1f}")
    print(f"  Size    : {len(data)} bytes")

    return jsonify({"status": "ok", "file": filename, "samples": len(values)}), 200


@app.route("/api/data/<filename>")
def data(filename):
    filename = os.path.basename(filename)  # защита от path traversal
    if filename not in list_files():
        return jsonify({"error": "not found"}), 404
    stats = file_stats(filename)
    if not stats:
        return jsonify({"error": "parse error"}), 500
    return jsonify(stats)


@app.route("/")
def index():
    return render_template("index.html", files=list_files())


# ──────────────────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    print("=" * 50)
    print("  ESP32 ADC Server")
    print("  POST http://0.0.0.0:8080/api/upload")
    print("  GET  http://0.0.0.0:8080/")
    print(f"  Uploads : {os.path.abspath(UPLOAD_DIR)}/")
    print("=" * 50)
    app.run(host="0.0.0.0", port=8080, debug=False)
