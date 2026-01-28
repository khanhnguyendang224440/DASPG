import csv
import numpy as np
import matplotlib.pyplot as plt
from scipy.signal import detrend
import os

FS = 50.0
LOW_F = 0.8
HIGH_F = 2.5

# Load CSV
t, spg = [], []
with open(os.path.join("TestCSV", "spg_data9.csv")) as f:
    r = csv.reader(f)
    next(r)
    for row in r:
        t.append(float(row[0]))
        spg.append(float(row[1]))

spg = np.array(spg)

# ===== XỬ LÝ BẮT BUỘC =====
spg = detrend(spg)
spg = (spg - np.mean(spg)) / np.std(spg)

# FFT
N = len(spg)
fft = np.fft.rfft(spg)
freq = np.fft.rfftfreq(N, 1/FS)
amp = np.abs(fft)

# Chỉ giữ vùng tim
mask = (freq >= LOW_F) & (freq <= HIGH_F)
freq_hr = freq[mask]
amp_hr = amp[mask]

idx = np.argmax(amp_hr)
f_hr = freq_hr[idx]
HR = f_hr * 60

# Plot
plt.figure(figsize=(10,4))
plt.plot(freq_hr, amp_hr)
plt.axvline(f_hr, color='r', linestyle='--', label=f"HR ≈ {HR:.1f} bpm")
plt.legend()
plt.grid()
plt.title("SPG FFT (Heart Band)")
plt.xlabel("Frequency (Hz)")
plt.ylabel("Amplitude")
plt.show()

print(f"Heart rate ≈ {HR:.1f} bpm")
