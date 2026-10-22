#!/usr/bin/env python3
"""Plots built from results/benchmarks.csv (written by scripts/bench.sh).

Output (PNG files in results/):
  strong_scaling.png    speedup and efficiency of life_hybrid, fixed n
  weak_scaling.png      efficiency of life_hybrid, work per unit constant
  versions_speedup.png  speedup of each version relative to its own 1-unit time
  final_times.png       time of every version at 8 units (rows tagged 'final')
  final_speedup.png     speedup vs the serial version, all sizes (rows 'final')

Times of different sessions are never compared: every curve is computed
from rows of a single date, relative to the 1-unit time of the same date.
"""
import csv
import os
import sys
from collections import defaultdict

try:
    import matplotlib
    matplotlib.use("Agg")  # no display needed, we only write files
    import matplotlib.pyplot as plt
except ImportError:
    sys.exit("matplotlib is not installed (see README, section Grafici)")

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CSV_PATH = os.path.join(ROOT, "results", "benchmarks.csv")
OUT_DIR = os.path.join(ROOT, "results")

# weak scaling: grid size n for each number of units (n^2 / units is constant)
WEAK_N = {1: 1024, 2: 1448, 4: 2048, 8: 2896}
INF = float("inf")


def load_rows():
    rows = []
    with open(CSV_PATH, newline="") as f:
        for r in csv.DictReader(f):
            rows.append({
                "date": r["data"],
                "tappa": r["tappa"],
                "version": r["versione"],
                "n": int(r["n"]),
                "procs": int(r["processi"]),
                "threads": int(r["thread"]),
                "time": float(r["tempo_min"]),
            })
    return rows


def series(rows, version, n):
    """Best time per (procs, threads) config of one version at size n.

    Only the latest date that contains the 1-unit baseline is used, so that
    all the times of a curve come from the same day.
    """
    by_date = defaultdict(list)
    for r in rows:
        if r["version"] == version and r["n"] == n:
            by_date[r["date"]].append(r)
    for date in sorted(by_date, reverse=True):
        cfg = {}
        for r in by_date[date]:
            key = (r["procs"], r["threads"])
            cfg[key] = min(cfg.get(key, INF), r["time"])
        if (1, 1) in cfg:
            return date, cfg
    return None, {}


def best_per_units(cfg):
    """{units: (time, procs, threads)} keeping the fastest config for each units."""
    best = {}
    for (p, t), tm in cfg.items():
        u = p * t
        if u not in best or tm < best[u][0]:
            best[u] = (tm, p, t)
    return best


def plot_strong(rows):
    fig, (ax_speed, ax_eff) = plt.subplots(1, 2, figsize=(11, 4.5))
    for n, color in ((2048, "tab:blue"), (4096, "tab:red")):
        date, cfg = series(rows, "life_hybrid", n)
        if not cfg:
            continue
        base = cfg[(1, 1)]
        for (p, t), tm in cfg.items():  # every config as a faint dot
            ax_speed.scatter(p * t, base / tm, s=16, color=color, alpha=0.4)
        best = best_per_units(cfg)
        units = sorted(best)
        ax_speed.plot(units, [base / best[u][0] for u in units], "-o",
                      color=color, label=f"n={n} (best config)")
        ax_eff.plot(units, [base / best[u][0] / u for u in units], "-o",
                    color=color, label=f"n={n}")
        for u in units:
            if u == 1:  # the baseline point needs no label
                continue
            _, p, t = best[u]
            ax_speed.annotate(f"{p}x{t}", (u, base / best[u][0]),
                              textcoords="offset points", xytext=(4, -11),
                              fontsize=7, color=color)
    ax_speed.plot([1, 8], [1, 8], "k--", linewidth=1, label="ideal")
    ax_eff.axhline(1.0, color="k", linestyle="--", linewidth=1, label="ideal")
    ax_speed.set(title="Strong scaling: speedup (life_hybrid)",
                 xlabel="units = processes x threads", ylabel="speedup vs 1x1")
    ax_eff.set(title="Strong scaling: efficiency", xlabel="units = processes x threads",
               ylabel="speedup / units", ylim=(0, 1.1))
    for ax in (ax_speed, ax_eff):
        ax.set_xticks([1, 2, 4, 8])
        ax.grid(alpha=0.3)
        ax.legend()
    fig.tight_layout()
    path = os.path.join(OUT_DIR, "strong_scaling.png")
    fig.savefig(path, dpi=150)
    plt.close(fig)
    return path


def plot_weak(rows):
    # for each config keep the LAST row (most recent measurement) at its weak n
    last = {}
    for r in rows:
        u = r["procs"] * r["threads"]
        if r["version"] == "life_hybrid" and WEAK_N.get(u) == r["n"]:
            last[(r["procs"], r["threads"])] = r["time"]
    if (1, 1) not in last:
        return None
    base = last[(1, 1)]
    families = {
        "MPI only (P x 1)": lambda p, t: t == 1,
        "OpenMP only (1 x T)": lambda p, t: p == 1,
        "hybrid (P x T)": lambda p, t: p > 1 and t > 1,
    }
    fig, ax = plt.subplots(figsize=(6.5, 4.5))
    for name, belongs in families.items():
        pts = {1: base}  # the 1x1 run is the starting point of every family
        for (p, t), tm in last.items():
            if p * t > 1 and belongs(p, t):
                pts[p * t] = min(pts.get(p * t, INF), tm)
        units = sorted(pts)
        ax.plot(units, [base / pts[u] for u in units], "-o", label=name)
    ax.axhline(1.0, color="k", linestyle="--", linewidth=1, label="ideal")
    ax.set(title="Weak scaling (life_hybrid, n^2/units constant)",
           xlabel="units = processes x threads", ylabel="T(1 unit) / T(units)",
           ylim=(0, 1.1))
    ax.set_xticks(sorted(WEAK_N))
    ax.set_xticklabels([f"{u}\n(n={WEAK_N[u]})" for u in sorted(WEAK_N)])
    ax.grid(alpha=0.3)
    ax.legend()
    fig.tight_layout()
    path = os.path.join(OUT_DIR, "weak_scaling.png")
    fig.savefig(path, dpi=150)
    plt.close(fig)
    return path


def plot_versions(rows, n=4096):
    versions = [
        ("life_omp", "OpenMP (threads)"),
        ("life_mpi", "MPI strips"),
        ("life_mpi_2d", "MPI 2D blocks"),
        ("life_mpi_2d_overlap", "MPI 2D + overlap"),
        ("life_hybrid", "hybrid MPI+OpenMP (best config)"),
    ]
    fig, ax = plt.subplots(figsize=(6.5, 4.5))
    for version, label in versions:
        date, cfg = series(rows, version, n)
        if not cfg:
            continue
        base = cfg[(1, 1)]
        best = best_per_units(cfg)
        units = sorted(best)
        ax.plot(units, [base / best[u][0] for u in units], "-o",
                label=f"{label} [{date}]")
    ax.plot([1, 8], [1, 8], "k--", linewidth=1, label="ideal")
    ax.set(title=f"Speedup vs own 1-unit time (n={n})",
           xlabel="units (threads or processes or both)", ylabel="speedup")
    ax.set_xticks([1, 2, 4, 8])
    ax.grid(alpha=0.3)
    ax.legend(fontsize=7)
    fig.text(0.5, 0.005, "each curve from a single session: absolute times are not comparable",
             ha="center", fontsize=7)
    fig.tight_layout(rect=(0, 0.02, 1, 1))
    path = os.path.join(OUT_DIR, "versions_speedup.png")
    fig.savefig(path, dpi=150)
    plt.close(fig)
    return path


FINAL_VERSIONS = [
    ("life_serial", "serial"),
    ("life_omp", "OpenMP"),
    ("life_mpi", "MPI strips"),
    ("life_mpi_2d", "MPI 2D"),
    ("life_mpi_2d_overlap", "MPI 2D + overlap"),
    ("life_hybrid", "hybrid"),
]
FINAL_SIZES = (512, 1024, 2048, 4096)


def final_times(rows):
    """{(version, n): {units: (time, procs, threads)}} from the rows tagged 'final'.

    All of them come from one session (scripts/final_comparison.sh). If a
    config was measured twice, the last row wins. For each units value the
    fastest config is kept.
    """
    last = {}
    for r in rows:
        if r["tappa"] == "final":
            last[(r["version"], r["n"], r["procs"], r["threads"])] = r["time"]
    out = defaultdict(dict)
    for (version, n, p, t), tm in last.items():
        u = p * t
        entry = out[(version, n)].get(u)
        if entry is None or tm < entry[0]:
            out[(version, n)][u] = (tm, p, t)
    return out


def print_final_tables(rows):
    data = final_times(rows)
    if not data:
        return
    for n in FINAL_SIZES:
        serial = data.get(("life_serial", n), {}).get(1)
        if not serial:
            continue
        print(f"\nn={n}: time (s) and speedup vs serial ({serial[0]:.3f} s)")
        print("| version | 1 | 2 | 4 | 8 |")
        print("|---|---|---|---|---|")
        for version, label in FINAL_VERSIONS[1:]:
            cells = []
            for u in (1, 2, 4, 8):
                e = data.get((version, n), {}).get(u)
                cells.append(f"{e[0]:.3f} ({serial[0] / e[0]:.2f}x, {e[1]}x{e[2]})" if e else "-")
            print(f"| {label} | " + " | ".join(cells) + " |")


def plot_final_bars(rows, n=4096):
    data = final_times(rows)
    serial = data.get(("life_serial", n), {}).get(1)
    if not serial:
        return None
    labels, times = [], []
    for version, label in FINAL_VERSIONS:
        entry = serial if version == "life_serial" else data.get((version, n), {}).get(8)
        if entry:
            labels.append(label + ("\n(1 unit)" if version == "life_serial" else "\n(8 units)"))
            times.append(entry[0])
    fig, ax = plt.subplots(figsize=(9, 4.8))
    bars = ax.bar(labels, times, color="tab:blue")
    for bar, tm in zip(bars, times):
        ax.annotate(f"{tm:.2f} s\n{serial[0] / tm:.1f}x", (bar.get_x() + bar.get_width() / 2, tm),
                    ha="center", va="bottom", fontsize=8)
    ax.set(title=f"Final comparison, n={n}, 100 steps (one session)",
           ylabel="time (s), lower is better", ylim=(0, serial[0] * 1.2))
    ax.grid(axis="y", alpha=0.3)
    fig.tight_layout()
    path = os.path.join(OUT_DIR, "final_times.png")
    fig.savefig(path, dpi=150)
    plt.close(fig)
    return path


def plot_final_speedup(rows):
    data = final_times(rows)
    if not data:
        return None
    fig, axes = plt.subplots(2, 2, figsize=(10, 8), sharex=True)
    for ax, n in zip(axes.flat, FINAL_SIZES):
        serial = data.get(("life_serial", n), {}).get(1)
        if not serial:
            continue
        for version, label in FINAL_VERSIONS[1:]:
            best = data.get((version, n), {})
            units = sorted(best)
            ax.plot(units, [serial[0] / best[u][0] for u in units], "-o", label=label)
        ax.plot([1, 8], [1, 8], "k--", linewidth=1, label="ideal (vs 1 unit)")
        ax.set(title=f"n={n}", ylabel="speedup vs serial")
        ax.set_xticks([1, 2, 4, 8])
        ax.grid(alpha=0.3)
    for ax in axes[1]:
        ax.set_xlabel("units (threads or processes or both)")
    axes[0][0].legend(fontsize=8)
    fig.suptitle("Final comparison: speedup vs the serial version (same session)")
    fig.tight_layout()
    path = os.path.join(OUT_DIR, "final_speedup.png")
    fig.savefig(path, dpi=150)
    plt.close(fig)
    return path


def main():
    rows = load_rows()
    for plot in (plot_strong, plot_weak, plot_versions, plot_final_bars, plot_final_speedup):
        path = plot(rows)
        print("written" if path else "skipped (missing data)", plot.__name__, path or "")
    print_final_tables(rows)


if __name__ == "__main__":
    main()
