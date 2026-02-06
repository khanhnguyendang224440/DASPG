import csv
import numpy as np
import matplotlib.pyplot as plt
import os

CSV = os.path.join("TestCSV", "spg_data25.csv")

time = []
spg  = []

with open(CSV, "r", newline="") as f:
    reader = csv.reader(f)
    header = next(reader, None)  # bỏ header (nếu có)
    for row in reader:
        if not row or len(row) < 2:
            continue
        try:
            time.append(float(row[0]))
            spg.append(float(row[1]))
        except ValueError:
            # bỏ qua dòng lỗi
            continue

if len(time) < 2:
    raise ValueError("CSV rỗng hoặc không đủ dữ liệu.")

time = np.array(time, dtype=float)
spg  = np.array(spg, dtype=float)

# Nếu time không tăng đều, sort lại (an toàn)
if np.any(np.diff(time) <= 0):
    idx = np.argsort(time)
    time = time[idx]
    spg  = spg[idx]
    print("⚠️ time_s không tăng đơn điệu, đã sort lại theo thời gian.")

# ---- Trim 10s đầu + 10s cuối theo TIME ----
TRIM_S = 10.0
trim_head = time[0] + TRIM_S
trim_tail = time[-1] - TRIM_S

mask = (time >= trim_head) & (time <= trim_tail)

# Nếu dữ liệu quá ngắn, fallback
if np.sum(mask) < 2:
    time_trim, spg_trim = time, spg
    print("⚠️ Dữ liệu không đủ dài để bỏ 10s đầu/cuối, đang vẽ toàn bộ.")
else:
    time_trim, spg_trim = time[mask], spg[mask]

# Ước lượng fs để bạn tiện báo cáo
dt = np.diff(time_trim)
fs_est = 1.0 / np.median(dt) if len(dt) > 0 else float("nan")

print(f"N_trim = {len(time_trim)} samples | duration ≈ {time_trim[-1]-time_trim[0]:.2f} s | fs ≈ {fs_est:.2f} Hz")

plt.figure(figsize=(10,4))
plt.plot(time_trim, spg_trim, linewidth=1)
plt.xlabel("Time (s)")
plt.ylabel("SPG (filtered)")
plt.title("SPG Signal (Time Domain) - Trim 10s head/tail")
plt.grid(True, alpha=0.4)
plt.tight_layout()
plt.show()
