import csv
import numpy as np
import matplotlib.pyplot as plt
from scipy.signal import detrend
import os

LOW_F = 0.8
HIGH_F = 2.5
TRIM_S = 10.0

# ====== CHỌN 1 TRONG 2 ======
USE_FS_FROM_TIMESTAMP = True   # khuyến nghị
FS_FIXED = 50.0                # nếu muốn ép FS (khi USE_FS_FROM_TIMESTAMP = False)

# Load CSV
t, spg = [], []
with open(os.path.join("TestCSV", "spg_data14.csv")) as f:
    r = csv.reader(f)
    next(r)
    for row in r:
        t.append(float(row[0]))
        spg.append(float(row[1]))

t = np.array(t)
spg = np.array(spg)

# ---- BỎ 10s ĐẦU + 10s CUỐI (theo time_s) ----
t_start = t[0] + TRIM_S
t_end   = t[-1] - TRIM_S
mask_t = (t >= t_start) & (t <= t_end)

# nếu dữ liệu quá ngắn thì dùng hết
if np.sum(mask_t) > 10:
    t = t[mask_t]
    spg = spg[mask_t]

# ===== FS =====
if USE_FS_FROM_TIMESTAMP:
    dt = np.diff(t)
    FS = 1.0 / np.median(dt)   # ổn định hơn mean
else:
    FS = FS_FIXED

# ===== XỬ LÝ BẮT BUỘC (giống bạn) =====
spg = detrend(spg)
spg = (spg - np.mean(spg)) / (np.std(spg) + 1e-12)

# FFT
N = len(spg)
X = np.fft.rfft(spg)
freq = np.fft.rfftfreq(N, d=1.0/FS)
amp = np.abs(X)

# Chỉ giữ vùng tim
mask = (freq >= LOW_F) & (freq <= HIGH_F)
freq_hr = freq[mask]
amp_hr = amp[mask]

idx = np.argmax(amp_hr)
f_hr = freq_hr[idx]
HR = f_hr * 60.0

# Plot (chỉ vẽ heart-band)
plt.figure(figsize=(10,4))
plt.plot(freq_hr, amp_hr)
plt.axvline(f_hr, color='r', linestyle='--', label=f"Peak: {f_hr:.3f} Hz → HR ≈ {HR:.1f} bpm")
plt.legend()
plt.grid(True, alpha=0.4)
plt.title("SPG FFT (Heart Band 0.8–2.5 Hz) after trim 10s + detrend + normalize")
plt.xlabel("Frequency (Hz)")
plt.ylabel("Amplitude")
plt.tight_layout()
plt.show()

print(f"FS used ≈ {FS:.2f} Hz | N = {N} samples | HR ≈ {HR:.1f} bpm")
