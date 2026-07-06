#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 Nicolai Buchwitz
"""Collect and plot pibridge cycle time data.

Collect: pibridge-cycles.py collect [-n SAMPLES] [-d SECONDS] [-o FILE]
Plot:    pibridge-cycles.py plot <csv_file> [csv_file ...] [-o output.png]

CSV format: timestamp,cycle_time_us[,rx_err]
"""

from __future__ import annotations

import argparse
import csv
import fcntl
import os
import statistics
import sys
import time
from collections import deque
from datetime import datetime
from pathlib import Path


SYSFS_CYCLE = "/sys/class/piControl/piControl0/last_cycle"
SYSFS_RX_ERR = "/sys/bus/serial/drivers/pi-bridge/stats/stat_rx_err"
SYSFS_CYCLE_DURATION = "/sys/devices/virtual/piControl/piControl0/cycle_duration"
PICONTROL_DEVICE = "/dev/piControl0"

# ioctl number for PICONTROL_WAIT_FOR_CYCLE: _IO('K', 51)
# _IO is defined as: ((type << 8) | nr). Available in piControl
# >= 2.7 (devel/nbuchwitz/pibridge-baudrate-negotiation). Older
# kernels return ENOTTY and we fall back to sysfs polling.
PICONTROL_WAIT_FOR_CYCLE = (ord("K") << 8) | 51


def _read_sysfs(path: str) -> int:
    """Read an integer from a sysfs file."""
    with open(path) as f:
        return int(f.read().strip())


class _RollingMean:
    """O(1) rolling mean over a fixed-size window."""

    def __init__(self, window: int) -> None:
        self._samples: deque[int] = deque(maxlen=window)
        self._sum = 0

    def add(self, val: int) -> None:
        if len(self._samples) == self._samples.maxlen:
            self._sum -= self._samples[0]  # evicted on append below
        self._samples.append(val)
        self._sum += val

    @property
    def value(self) -> float:
        return self._sum / len(self._samples) if self._samples else 0.0


class _CycleTrigger:
    """Blocks until the next piControl cycle completes.

    Uses the PICONTROL_WAIT_FOR_CYCLE ioctl when available, otherwise
    falls back to timed sysfs polling. Use as a context manager.
    """

    def __init__(self, cycle_target_us: int | None) -> None:
        self._pi_fd: int | None = None
        self._use_ioctl = False
        self._last_sample_mono = 0.0

        # Polling parameters (used only in fallback path)
        self._poll_interval = 0.001
        self._min_gap = 0.0
        if cycle_target_us and cycle_target_us >= 500:
            self._poll_interval = max(0.0005, (cycle_target_us / 1e6) * 0.3)
            self._min_gap = (cycle_target_us / 1e6) * 0.8

        # Try the ioctl once to probe support
        try:
            self._pi_fd = os.open(PICONTROL_DEVICE, os.O_RDONLY)
            fcntl.ioctl(self._pi_fd, PICONTROL_WAIT_FOR_CYCLE)
            self._use_ioctl = True
        except OSError:
            if self._pi_fd is not None:
                try:
                    os.close(self._pi_fd)
                except OSError:
                    pass
                self._pi_fd = None

    @property
    def mode(self) -> str:
        return "WAIT_FOR_CYCLE ioctl" if self._use_ioctl else "sysfs polling"

    def wait(self) -> None:
        """Block until the next new cycle is available to read."""
        if self._use_ioctl:
            fcntl.ioctl(self._pi_fd, PICONTROL_WAIT_FOR_CYCLE)
            return

        # Polling fallback: sleep until min_gap has passed since the last
        # recorded sample. Accepts identical consecutive values (which
        # happen at steady state) instead of silently dropping them.
        while True:
            now = time.monotonic()
            if now - self._last_sample_mono >= self._min_gap:
                self._last_sample_mono = now
                return
            time.sleep(self._poll_interval)

    def close(self) -> None:
        if self._pi_fd is not None:
            try:
                os.close(self._pi_fd)
            except OSError:
                pass
            self._pi_fd = None

    def __enter__(self) -> "_CycleTrigger":
        return self

    def __exit__(self, *_) -> None:
        self.close()


def _probe_environment() -> tuple[int | None, bool]:
    """Return (cycle_target_us, has_err_stats)."""
    cycle_target: int | None = None
    if Path(SYSFS_CYCLE_DURATION).exists():
        cycle_target = _read_sysfs(SYSFS_CYCLE_DURATION)
    has_err_stats = Path(SYSFS_RX_ERR).exists()
    return cycle_target, has_err_stats


def _print_period_stats(
    period_samples: list[int], mean: float, outlier_threshold: float
) -> None:
    """Print a summary line for the samples collected in the last interval."""
    outliers = sum(1 for s in period_samples if s > mean * outlier_threshold)
    print(
        f"[{datetime.now():%H:%M:%S}] "
        f"n={len(period_samples)} "
        f"min={min(period_samples)} "
        f"max={max(period_samples)} "
        f"mean={statistics.mean(period_samples):.1f} "
        f"stdev={statistics.stdev(period_samples):.1f} "
        f"outliers={outliers}"
    )


def cmd_collect(args: argparse.Namespace) -> None:
    """Collect cycle time samples from piControl and write them to CSV."""
    logfile = Path(
        args.output or f"picontrol_cycles_{datetime.now():%Y%m%d_%H%M%S}.csv"
    )
    cycle_target, has_err_stats = _probe_environment()
    last_rx_err = _read_sysfs(SYSFS_RX_ERR) if has_err_stats else 0

    rolling = _RollingMean(window=100)
    period_samples: list[int] = []
    sample_count = 0
    last_stats_time = time.monotonic()
    outlier_threshold = 1.5
    stats_interval = 15.0

    deadline = time.monotonic() + args.duration if args.duration else None
    sample_limit = args.samples

    with _CycleTrigger(cycle_target) as trigger, open(
        logfile, "w", newline=""
    ) as csvfile:

        print(f"Logging to: {logfile}")
        if cycle_target is not None:
            print(f"Cycle target: {cycle_target} us")
        print(f"Trigger: {trigger.mode}")
        if has_err_stats:
            print(f"Error stats: {SYSFS_RX_ERR}")
        limit_str = (
            f"{sample_limit} samples"
            if sample_limit
            else f"{args.duration}s" if args.duration else "unlimited"
        )
        print(f"Collecting {limit_str}... Ctrl+C to stop\n")

        writer = csv.writer(csvfile)
        writer.writerow(["timestamp", "cycle_time_us", "rx_err"])

        try:
            while True:
                if sample_limit and sample_count >= sample_limit:
                    break
                if deadline and time.monotonic() >= deadline:
                    break

                trigger.wait()
                val = _read_sysfs(SYSFS_CYCLE)

                if has_err_stats:
                    rx_err = _read_sysfs(SYSFS_RX_ERR)
                    err_delta = rx_err - last_rx_err
                    last_rx_err = rx_err
                else:
                    err_delta = 0

                rolling.add(val)
                period_samples.append(val)

                writer.writerow([f"{time.time():.6f}", val, err_delta])
                sample_count += 1
                if sample_count % 50 == 0:
                    csvfile.flush()

                if val > rolling.value * outlier_threshold:
                    print(f"OUTLIER: {val} us (mean: {rolling.value:.1f})")
                if err_delta > 0:
                    print(f"RX_ERR: +{err_delta} (total: {rx_err})")

                now = time.monotonic()
                if now - last_stats_time >= stats_interval and period_samples:
                    _print_period_stats(
                        period_samples, rolling.value, outlier_threshold
                    )
                    period_samples.clear()
                    last_stats_time = now

        except KeyboardInterrupt:
            pass

        csvfile.flush()
        print(f"\nSaved {sample_count} samples to {logfile}")


# Cycle times are small integers, so the whole distribution fits in a
# bincount of this size. 65 ms is far above any real cycle.
_HIST_SIZE = 1 << 16


def load_csv(path: str | Path, n_points: int = 4000):
    """Load cycle time data, aggregated to bounded memory.

    The file is read in chunks. The cycle time distribution is kept as a
    bincount histogram and the time series is downsampled to an envelope of
    at most ``n_points`` min/max/mean blocks, so memory does not grow with
    the file size.

    Parameters
    ----------
    path : str or Path
        Path to CSV with ``timestamp``, ``cycle_time_us`` and optional
        ``rx_err`` columns.
    n_points : int
        Maximum number of time-series envelope blocks.

    Returns
    -------
    dict
        Envelope arrays, histogram, error samples and precomputed stats.
    """
    import numpy as np
    import pandas as pd

    with open(path) as f:
        header = f.readline().strip().split(",")
    has_err = "rx_err" in header
    usecols = [0, 1, 2] if has_err else [0, 1]

    hist = np.zeros(_HIST_SIZE, dtype=np.int64)
    trel: list[float] = []
    cmin: list[int] = []
    cmax: list[int] = []
    cmean: list[float] = []
    err_trel: list = []
    err_c: list = []
    err_total = 0
    t0 = None

    # Estimate the row count from the file size to pick an envelope block
    # size that yields about n_points blocks.
    est_rows = max(1, os.path.getsize(path) // 26)
    rpb = max(1, est_rows // n_points)
    left_c = np.empty(0, dtype=np.int64)
    left_t = np.empty(0, dtype=np.float64)

    for df in pd.read_csv(
        path, header=0, usecols=usecols, dtype=np.float64, chunksize=1_000_000
    ):
        a = df.to_numpy()
        ts = a[:, 0].astype(np.float64)
        cyc = a[:, 1].astype(np.int64)
        np.clip(cyc, 0, _HIST_SIZE - 1, out=cyc)
        if t0 is None:
            t0 = ts[0]

        bc = np.bincount(cyc)
        hist[: bc.size] += bc

        if has_err:
            err = a[:, 2].astype(np.int64)
            err_total += int(err.sum())
            m = err > 0
            if m.any():
                err_trel.append(ts[m] - t0)
                err_c.append(cyc[m])

        # Aggregate full rpb-sized blocks; keep the remainder for next chunk.
        c = np.concatenate((left_c, cyc))
        t = np.concatenate((left_t, ts))
        nb = c.size // rpb
        if nb:
            cc = c[: nb * rpb].reshape(nb, rpb)
            tt = t[: nb * rpb].reshape(nb, rpb)
            cmin.append(cc.min(axis=1))
            cmax.append(cc.max(axis=1))
            cmean.append(cc.mean(axis=1))
            trel.append(tt.mean(axis=1) - t0)
        left_c = c[nb * rpb :]
        left_t = t[nb * rpb :]

    if left_c.size:
        cmin.append(np.array([left_c.min()]))
        cmax.append(np.array([left_c.max()]))
        cmean.append(np.array([left_c.mean()]))
        trel.append(np.array([left_t.mean() - t0]))

    total = int(hist.sum())
    nz = np.nonzero(hist)[0]
    w = hist[nz]
    mean = float((nz * w).sum() / total)
    std = float(np.sqrt((w * (nz - mean) ** 2).sum() / total))
    p99 = int(nz[np.searchsorted(np.cumsum(w), 0.99 * total)])

    return {
        "trel": np.concatenate(trel) if trel else np.empty(0),
        "cmin": np.concatenate(cmin) if cmin else np.empty(0),
        "cmax": np.concatenate(cmax) if cmax else np.empty(0),
        "cmean": np.concatenate(cmean) if cmean else np.empty(0),
        "hist": hist,
        "err_trel": np.concatenate(err_trel) if err_trel else np.empty(0),
        "err_c": np.concatenate(err_c) if err_c else np.empty(0),
        "stats": {
            "mean": mean,
            "std": std,
            "min": int(nz[0]),
            "max": int(nz[-1]),
            "p99": p99,
            "err_total": err_total,
            "count": total,
        },
    }


def _plot_single(
    ax_ts,
    ax_hist,
    data,
    title: str | None = None,
    cycle_target: int | None = None,
) -> None:
    """Plot the time series envelope and histogram for one dataset.

    Parameters
    ----------
    ax_ts : matplotlib.axes.Axes
        Axes for the time series plot.
    ax_hist : matplotlib.axes.Axes
        Axes for the histogram.
    data : dict
        Aggregated dataset from load_csv().
    title : str or None
        Plot title.
    cycle_target : int or None
        Target cycle time in microseconds (drawn as green line).
    """
    import numpy as np

    st = data["stats"]
    trel = data["trel"]
    has_errors = st["err_total"] > 0

    # Time series envelope: min-max band plus per-block mean.
    ax_ts.fill_between(
        trel,
        data["cmin"],
        data["cmax"],
        color="#2196F3",
        alpha=0.25,
        linewidth=0,
        label="min-max",
    )
    ax_ts.plot(trel, data["cmean"], linewidth=0.8, color="#1976D2", label="mean")
    ax_ts.axhline(
        st["mean"],
        color="#F44336",
        linestyle="--",
        linewidth=1,
        label=f"Mean: {st['mean']:.0f} us",
    )
    ax_ts.axhline(
        st["p99"],
        color="#FF9800",
        linestyle=":",
        linewidth=1,
        label=f"P99: {st['p99']:.0f} us",
    )

    if cycle_target and cycle_target > 500:
        ax_ts.axhline(
            cycle_target,
            color="#4CAF50",
            linestyle="-.",
            linewidth=1,
            label=f"Target: {cycle_target} us",
        )

    if has_errors and data["err_trel"].size:
        ax_ts.scatter(
            data["err_trel"],
            data["err_c"],
            color="#F44336",
            s=20,
            zorder=5,
            label=f"RX errors ({st['err_total']} total)",
        )

    if title:
        ax_ts.set_title(title)
    ax_ts.set_xlabel("Time (s)")
    ax_ts.set_ylabel("Cycle time (us)")
    ax_ts.legend(loc="upper right")
    ax_ts.grid(True, alpha=0.3)

    # Histogram straight from the exact bincount.
    hist = data["hist"]
    lo = max(st["min"] - 10, 0)
    hi = st["max"] + 1
    vals = np.arange(lo, hi)
    ax_hist.fill_between(
        vals, hist[lo:hi], step="mid", color="#2196F3", alpha=0.7
    )
    ax_hist.axvline(st["mean"], color="#F44336", linestyle="--", linewidth=1)
    ax_hist.axvline(st["p99"], color="#FF9800", linestyle=":", linewidth=1)
    ax_hist.set_xlabel("Cycle time (us)")
    ax_hist.set_ylabel("Count")
    err_str = f", errors: {st['err_total']}" if has_errors else ""
    ax_hist.set_title(
        f"Distribution\n(std: {st['std']:.0f} us, "
        f"min: {st['min']}, max: {st['max']}{err_str})"
    )
    ax_hist.grid(True, alpha=0.3)


def cmd_plot(args: argparse.Namespace) -> None:
    """Plot cycle time data from one or more CSV files.

    Parameters
    ----------
    args : argparse.Namespace
        Parsed arguments with ``files``, ``output`` and ``title``.
    """
    try:
        import matplotlib

        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print(
            "matplotlib, numpy and pandas required: "
            "pip install matplotlib numpy pandas",
            file=sys.stderr,
        )
        sys.exit(1)

    n = len(args.files)
    output = args.output or "jitter_plot.png"

    labels = args.labels or [Path(p).stem for p in args.files]
    if len(labels) != n:
        print(f"Expected {n} labels, got {len(labels)}", file=sys.stderr)
        sys.exit(1)

    # Load all data once up front so we don't re-read files for the summary
    print(f"Loading {n} file(s)...")
    t_load_start = time.monotonic()
    datasets = [load_csv(p) for p in args.files]
    all_stats = [d["stats"] for d in datasets]
    print(f"Loaded in {time.monotonic() - t_load_start:.1f}s")

    if n == 1:
        data = datasets[0]

        fig, (ax_ts, ax_hist) = plt.subplots(
            2, 1, figsize=(14, 8), gridspec_kw={"height_ratios": [3, 1]}
        )

        title = args.title or f"PiBridge Cycle Time ({all_stats[0]['count']} samples)"
        _plot_single(ax_ts, ax_hist, data, title, args.cycle_target)
    else:
        fig, axes = plt.subplots(
            n,
            2,
            figsize=(16, 4 * n),
            gridspec_kw={"width_ratios": [3, 1]},
            squeeze=False,
        )
        title = args.title or "PiBridge Cycle Time Comparison"
        fig.suptitle(title, fontsize=14)

        for i, (data, label, stats) in enumerate(
            zip(datasets, labels, all_stats)
        ):
            _plot_single(
                axes[i][0],
                axes[i][1],
                data,
                f"{label} ({stats['count']} samples)",
                args.cycle_target,
            )

    plt.tight_layout()
    print("Saving...")
    t_save_start = time.monotonic()
    plt.savefig(output, dpi=100)
    print(f"Saved to {output} in {time.monotonic() - t_save_start:.1f}s")

    for label, stats in zip(labels, all_stats):
        print(
            f"  {label}: mean={stats['mean']:.0f} "
            f"std={stats['std']:.0f} "
            f"min={stats['min']} max={stats['max']} "
            f"p99={stats['p99']:.0f} "
            f"rx_err={stats['err_total']}"
        )


def main() -> None:
    """Parse arguments and dispatch to collect or plot subcommand."""
    parser = argparse.ArgumentParser(
        description="Collect and plot pibridge cycle time data"
    )
    sub = parser.add_subparsers(dest="command", required=True)

    p_collect = sub.add_parser("collect", help="Collect cycle time samples")
    p_collect.add_argument(
        "-n",
        "--samples",
        type=int,
        default=None,
        help="Number of samples (default: unlimited)",
    )
    p_collect.add_argument(
        "-d",
        "--duration",
        type=float,
        default=None,
        help="Collection duration in seconds",
    )
    p_collect.add_argument("-o", "--output", default=None, help="Output CSV file")

    p_plot = sub.add_parser("plot", help="Plot cycle time data")
    p_plot.add_argument("files", nargs="+", help="CSV file(s)")
    p_plot.add_argument("-o", "--output", default=None, help="Output PNG file")
    p_plot.add_argument("-t", "--title", default=None, help="Plot title")
    p_plot.add_argument(
        "-l",
        "--labels",
        nargs="+",
        default=None,
        help="Custom labels for each CSV file",
    )
    p_plot.add_argument(
        "-c",
        "--cycle-target",
        type=int,
        default=None,
        help="Target cycle time in microseconds (drawn as green line)",
    )

    args = parser.parse_args()

    if args.command == "collect":
        cmd_collect(args)
    elif args.command == "plot":
        cmd_plot(args)


if __name__ == "__main__":
    main()
