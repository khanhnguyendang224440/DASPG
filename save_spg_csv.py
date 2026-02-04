import serial
import csv
import time
import os

# ===============================
# CẤU HÌNH
# ===============================
PORT = "COM8"          # đổi nếu COM khác
BAUD = 115200

OUTPUT_DIR = "TestCSV"
os.makedirs(OUTPUT_DIR, exist_ok=True)
FILENAME = os.path.join(OUTPUT_DIR, "spg_data16.csv")

# ===============================
# MỞ SERIAL (KHÔNG RESET ESP32)
# ===============================
ser = serial.Serial(
    PORT,
    BAUD,
    timeout=1,
    dsrdtr=False,
    rtscts=False
)

# TẮT DTR / RTS để tránh reset ESP32-S3
ser.setDTR(False)
ser.setRTS(False)

# Chờ ESP32 boot xong (đã có delay trong setup)
time.sleep(2)

print("Recording SPG...")
print("Press Ctrl+C to stop\n")

# ===============================
# GHI FILE CSV
# ===============================
with open(FILENAME, "w", newline="") as f:
    writer = csv.writer(f)
    writer.writerow(["time_s", "K_filt"])

    try:
        while True:
            line = ser.readline().decode("utf-8", errors="ignore").strip()
            if not line:
                continue

            # ✅ CHỈ IN RAW
            print("RAW:", line)

            # bỏ header
            if line.startswith("time"):
                continue
            if line.startswith("ESP32"):
                continue

            parts = line.split(",")
            if len(parts) != 2:
                continue

            try:
                t = float(parts[0])
                k = float(parts[1])
            except ValueError:
                continue

            # ✅ LƯU CSV
            writer.writerow([t, k])

    except KeyboardInterrupt:
        print("\nStopped recording.")

# ===============================
# ĐÓNG SERIAL
# ===============================
ser.close()
print(f"Saved to {FILENAME}")
