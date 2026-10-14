#!/usr/bin/env python3
"""plot.py — radar_demo'nun urettigi CSV dosyalarindan grafik uretir (sadece cizim).

Kullanim:  python scripts/plot.py [output_dir] [images_dir]
Gerekli:   numpy, matplotlib

Uretilen dosyalar (docs/images/):
  range_doppler.png  range-Doppler isi haritasi + gercek hedefler + CA/OS tespitleri
  pd_vs_snr.png      senaryo basina Pd-SNR egrileri (CA-CFAR vs OS-CFAR)
  pfa.png            tasarim Pfa'si ile gerceklesen Pfa
  timing.png         asama basina ms/cerceve
"""
import csv
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

out_dir = Path(sys.argv[1] if len(sys.argv) > 1 else "output")
img_dir = Path(sys.argv[2] if len(sys.argv) > 2 else "docs/images")
img_dir.mkdir(parents=True, exist_ok=True)

# Iki seri: renk koru icin ayri renk + farkli cizgi/isaret kullanilir.
CA_COLOR, OS_COLOR = "#2a6fdb", "#d9660a"
plt.rcParams.update({"font.size": 10, "axes.spines.top": False, "axes.spines.right": False,
                     "axes.grid": True, "grid.alpha": 0.25})


def read_rows(name):
    with open(out_dir / name, newline="") as f:
        return list(csv.DictReader(f))


meta = read_rows("meta.csv")[0]
range_bin = float(meta["range_bin_m"])
vel_res = float(meta["velocity_res_mps"])
num_pulses = int(meta["num_pulses"])

# ---- 1) Range-Doppler isi haritasi ------------------------------------------------
rd = np.loadtxt(out_dir / "rd_map_db.csv", delimiter=",")
max_bin = 300  # pencerenin sonu (yankinin sarildigi bolge) gosterilmez
rd = rd[:, :max_bin]
ranges = np.arange(max_bin) * range_bin
vels = (np.arange(num_pulses) - num_pulses / 2) * vel_res
extent = [ranges[0] - range_bin / 2, ranges[-1] + range_bin / 2,
          vels[0] - vel_res / 2, vels[-1] + vel_res / 2]

fig, ax = plt.subplots(figsize=(9, 5))
noise_floor = np.median(rd)
im = ax.imshow(rd, origin="lower", aspect="auto", extent=extent, cmap="viridis",
               vmin=noise_floor - 5, vmax=rd.max())
cb = fig.colorbar(im, ax=ax)
cb.set_label("Guc [dB]")
ax.grid(False)

truth = read_rows("truth.csv")
ax.scatter([float(t["range_m"]) for t in truth], [float(t["velocity_mps"]) for t in truth],
           s=260, facecolors="none", edgecolors="white", linewidths=1.5, label="gercek hedef")
for typ, marker, color, size in (("CA", "o", CA_COLOR, 60), ("OS", "x", OS_COLOR, 55)):
    det = [d for d in read_rows("detections.csv") if d["type"] == typ and
           float(d["range_m"]) <= ranges[-1]]
    ax.scatter([float(d["range_m"]) for d in det], [float(d["velocity_mps"]) for d in det],
               s=size, marker=marker, color=color, edgecolors="white" if marker == "o" else None,
               linewidths=0.8, label=f"{typ}-CFAR tespiti ({len(det)})")
ax.set_xlabel("Menzil [m]")
ax.set_ylabel("Hiz [m/s]  (+ = yaklasan)")
ax.set_title("Range-Doppler haritasi ve CFAR tespitleri")
ax.legend(loc="upper right", framealpha=0.9)
fig.tight_layout()
fig.savefig(img_dir / "range_doppler.png", dpi=150)
plt.close(fig)

# ---- 2) Pd - SNR ----------------------------------------------------------------------
pd_path = out_dir / "pd_snr.csv"
if pd_path.exists():
    rows = read_rows("pd_snr.csv")
    scenarios = list(dict.fromkeys(r["scenario"] for r in rows))
    titles = {"homogeneous": "Homojen gurultu", "clutter_edge": "Clutter kenari",
              "interfering_target": "Yakin guclu hedef"}
    fig, axes = plt.subplots(1, len(scenarios), figsize=(4.2 * len(scenarios), 3.8), sharey=True)
    axes = np.atleast_1d(axes)
    for ax, sc in zip(axes, scenarios):
        sel = [r for r in rows if r["scenario"] == sc]
        snr = [float(r["snr_db"]) for r in sel]
        ax.plot(snr, [float(r["pd_ca"]) for r in sel], "-o", color=CA_COLOR, label="CA-CFAR",
                markersize=5, linewidth=2)
        ax.plot(snr, [float(r["pd_os"]) for r in sel], "--s", color=OS_COLOR, label="OS-CFAR",
                markersize=5, linewidth=2)
        ax.set_title(titles.get(sc, sc))
        ax.set_xlabel("SNR (butunlestirme sonrasi) [dB]")
        ax.set_ylim(-0.02, 1.02)
    axes[0].set_ylabel("Tespit olasiligi Pd")
    axes[0].legend(loc="lower right")
    fig.suptitle("Pd - SNR  (tasarim Pfa = 1e-6)", y=1.0)
    fig.tight_layout()
    fig.savefig(img_dir / "pd_vs_snr.png", dpi=150)
    plt.close(fig)

# ---- 3) Pfa ------------------------------------------------------------------------------
pfa_path = out_dir / "pfa.csv"
if pfa_path.exists():
    rows = read_rows("pfa.csv")
    design = np.array([float(r["design_pfa"]) for r in rows])
    fig, ax = plt.subplots(figsize=(5, 4))
    ax.loglog(design, design, ":", color="gray", label="ideal (olculen = tasarim)")
    ax.loglog(design, [float(r["pfa_ca"]) for r in rows], "-o", color=CA_COLOR, label="CA-CFAR")
    ax.loglog(design, [float(r["pfa_os"]) for r in rows], "--s", color=OS_COLOR, label="OS-CFAR")
    ax.set_xlabel("Tasarim Pfa")
    ax.set_ylabel("Gerceklesen Pfa")
    ax.set_title("Yalniz gurultu: gerceklesen yanlis alarm orani")
    ax.legend()
    fig.tight_layout()
    fig.savefig(img_dir / "pfa.png", dpi=150)
    plt.close(fig)

# ---- 4) Sure ---------------------------------------------------------------------------
rows = read_rows("timing.csv")
fig, ax = plt.subplots(figsize=(6, 3.5))
names = [r["stage"].replace("_", " ") for r in rows]
vals = [float(r["ms_per_frame"]) for r in rows]
ax.barh(names, vals, color="#5b6b7f")
for i, v in enumerate(vals):
    ax.text(v, i, f" {v:.1f}", va="center")
ax.invert_yaxis()
ax.set_xlabel("ms / cerceve (64 darbe x 512 ornek)")
ax.set_title("Asama basina islem suresi")
ax.grid(axis="y", visible=False)
fig.tight_layout()
fig.savefig(img_dir / "timing.png", dpi=150)
plt.close(fig)

print("Grafikler yazildi:", img_dir)
