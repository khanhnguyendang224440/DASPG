import csv
import matplotlib.pyplot as plt

CSV = "spg_data7.csv"

time = []
spg  = []

with open(CSV, "r") as f:
    reader = csv.reader(f)
    header = next(reader)   # bỏ header
    for row in reader:
        time.append(float(row[0]))
        spg.append(float(row[1]))

plt.figure(figsize=(10,4))
plt.plot(time, spg, linewidth=1)
plt.xlabel("Time (s)")
plt.ylabel("SPG (filtered)")
plt.title("SPG Signal (Time Domain)")
plt.grid(True)
plt.tight_layout()
plt.show()
