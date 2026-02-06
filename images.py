import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from scipy.signal import detrend, welch

# ====== CONFIG ======
CSV_PATH = "spg_data14.csv"   # đổi tên file của bạn
OUT_DIR  = "Images"          # đúng thư mục bạn đang dùng trong Overleaf
WARMUP_S = 5.0               # đúng với code main.cpp: warmup ~5s
WIN_SEC  = 15.0              # cửa sổ tìm đoạn đẹp (15s)
STEP_SEC = 2.0               # bước trượt

# ====== LOAD ======
df = pd.read_csv(CSV_PATH)

# Đúng theo code: 2 cột time_s, K_filt
t = df["time_s"].to_numpy(dtype=float)
x = df["K_filt"].to_numpy(dtype=float)

# ====== DROP WARMUP (khớp firmware: warmup 5s không in dữ liệu) ======
# (Thực tế file bạn lưu đã không có warmup, nhưng nếu bạn ghép file/đổi code thì cứ cắt cho chắc.)
mask = t >= (t[0] + WARMUP_S)
t2 = t[mask]
x2 = x[mask]

# ====== SAMPLING STATS from timestamp ======
dt = np.diff(t2)
dt_med = np.median(dt)
dt_mean = np.mean(dt)
dt_std = np.std(dt)
dt_min = np.min(dt)
dt_max = np.max(dt)
fs = 1.0 / dt_med

# Outlier rule (dropped-sample heuristic)
outlier = dt > 1.5 * dt_med
out_rate = 100.0 * np.mean(outlier)

# ====== FIG 1: dt timeseries ======
plt.figure(figsize=(10,3.2))
plt.plot(dt, linewidth=1.0)
plt.xlabel("Sample index")
plt.ylabel("Δt (s)")
plt.title("Timestamp difference Δt (jitter / dropped-sample check)")
plt.tight_layout()
plt.savefig(f"{OUT_DIR}/dt_timeseries.png", dpi=200)
plt.show()

# ====== FIG 2: dt histogram ======
plt.figure(figsize=(6.8,3.2))
plt.hist(dt, bins=30)
plt.xlabel("Δt (s)")
plt.ylabel("Count")
plt.title("Histogram of Δt")
plt.tight_layout()
plt.savefig(f"{OUT_DIR}/dt_hist.png", dpi=200)
plt.show()

# ====== Find best segment (based on strongest heart-band PSD vs lowfreq) ======
# Heart band: 0.8–2.5 Hz (đúng báo cáo của bạn)
# Low freq band: 0.05–0.5 Hz (respiration/drift)
win = int(round(WIN_SEC * fs))
step = int(round(STEP_SEC * fs))

scores, starts = [], []
x2_d = detrend(x2, type="linear")

for s in range(0, len(x2_d) - win, step):
    seg = x2_d[s:s+win]
    f, Pxx = welch(seg, fs=fs, nperseg=min(256, len(seg)), noverlap=128)
    heart = (f >= 0.8) & (f <= 2.5)
    low   = (f >= 0.05) & (f <= 0.5)
    if heart.sum() < 3: 
        continue

    P_h = Pxx[heart]
    peak = np.max(P_h)
    heart_power = np.trapz(P_h, f[heart])
    low_power = np.trapz(Pxx[low], f[low]) if low.sum() else 1e-12
    # score: peak strong, lowfreq small, heart-power fraction reasonable
    score = (peak / (low_power + 1e-20)) * (heart_power / (np.trapz(Pxx, f) + 1e-20))
    scores.append(score)
    starts.append(s)

best_s = starts[int(np.argmax(scores))]
best_e = best_s + win

seg_t = t2[best_s:best_e]
seg_x = x2[best_s:best_e]
seg_xd = detrend(seg_x, type="linear")

# FFT (Hann window)
N = len(seg_xd)
freq = np.fft.rfftfreq(N, d=1/fs)
X = np.fft.rfft(seg_xd * np.hanning(N))
amp = np.abs(X)

hb = (freq >= 0.8) & (freq <= 2.5)
f_peak = freq[hb][np.argmax(amp[hb])]
HR = 60.0 * f_peak

# ====== FIG 3: best segment raw ======
plt.figure(figsize=(10,3.2))
plt.plot(seg_t, seg_x)
plt.xlabel("Time (s)")
plt.ylabel("K_filt")
plt.title(f"Best segment (raw K_filt) — {seg_t[0]:.2f}s to {seg_t[-1]:.2f}s")
plt.tight_layout()
plt.savefig(f"{OUT_DIR}/spg_best_segment_raw.png", dpi=200)
plt.show()

# ====== FIG 4: best segment FFT ======
plt.figure(figsize=(10,3.2))
plt.plot(freq, amp)
plt.xlim(0, 5)
plt.xlabel("Frequency (Hz)")
plt.ylabel("Amplitude")
plt.title(f"FFT of best segment — f_peak={f_peak:.3f} Hz → HR≈{HR:.1f} bpm")
plt.tight_layout()
plt.savefig(f"{OUT_DIR}/spg_best_segment_fft.png", dpi=200)
plt.show()

# ====== Print numbers to paste into LaTeX table ======
print("\n=== SAMPLING SUMMARY (from time_s, exactly as firmware outputs) ===")
print(f"N (samples)        = {len(t2)}")
print(f"Duration (s)       = {t2[-1]-t2[0]:.2f}")
print(f"median(dt) (s)     = {dt_med:.6f}")
print(f"mean(dt) (s)       = {dt_mean:.6f}")
print(f"std(dt) (s)        = {dt_std:.6f}")
print(f"dt_min / dt_max(s) = {dt_min:.6f} / {dt_max:.6f}")
print(f"fs ≈ 1/median(dt)  = {fs:.2f} Hz")
print(f"Outlier rate (%)   = {out_rate:.2f}")

print("\n=== BEST SEGMENT / HR ===")
print(f"Segment: {seg_t[0]:.2f}s → {seg_t[-1]:.2f}s  (len={len(seg_t)} samples)")
print(f"f_peak = {f_peak:.3f} Hz  →  HR ≈ {HR:.1f} bpm")
