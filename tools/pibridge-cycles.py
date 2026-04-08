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


def load_csv(path: str | Path):
    """Load cycle time data from a CSV file into numpy arrays.

    Parameters
    ----------
    path : str or Path
        Path to CSV with ``timestamp``, ``cycle_time_us`` and optional
        ``rx_err`` columns.

    Returns
    -------
    tuple[numpy.ndarray, numpy.ndarray, numpy.ndarray]
        Timestamps, cycle times, and per-sample error deltas.
    """
    import numpy as np

    # Peek at the header to determine column count
    with open(path) as f:
        header = f.readline().strip().split(",")

    has_err = "rx_err" in header
    usecols = (0, 1, 2) if has_err else (0, 1)

    data = np.loadtxt(
        path,
        delimiter=",",
        skiprows=1,
        usecols=usecols,
        dtype=np.float64,
    )

    ts = data[:, 0]
    cycles = data[:, 1].astype(np.int64)
    errors = (
        data[:, 2].astype(np.int64) if has_err else np.zeros(len(ts), dtype=np.int64)
    )
    return ts, cycles, errors


def _compute_stats(cycles, errors):
    """Compute summary statistics for a cycle dataset.

    Returns a dict with mean, p99, std, min, max, and error totals.
    Computed once and reused for plot labels and final summary.
    """
    import numpy as np

    return {
        "mean": float(np.mean(cycles)),
        "p99": float(np.percentile(cycles, 99)),
        "std": float(np.std(cycles)),
        "min": int(cycles.min()),
        "max": int(cycles.max()),
        "err_total": int(errors.sum()),
    }


def _plot_single(
    ax_ts,
    ax_hist,
    ts,
    cycles,
    errors,
    stats,
    title: str | None = None,
    cycle_target: int | None = None,
) -> None:
    """Plot time series and histogram for a single dataset.

    Parameters
    ----------
    ax_ts : matplotlib.axes.Axes
        Axes for the time series plot.
    ax_hist : matplotlib.axes.Axes
        Axes for the histogram.
    ts, cycles, errors : numpy.ndarray
        Data arrays.
    stats : dict
        Pre-computed statistics from _compute_stats().
    title : str or None
        Plot title.
    cycle_target : int or None
        Target cycle time in microseconds (drawn as green line).
    """
    import numpy as np

    t_rel = ts - ts[0]
    has_errors = stats["err_total"] > 0

    # Rasterize the time series when there are many samples. This renders
    # the line as a bitmap instead of vector paths, which is orders of
    # magnitude faster for both drawing and saving.
    ax_ts.plot(
        t_rel,
        cycles,
        linewidth=0.5,
        alpha=0.7,
        color="#2196F3",
        rasterized=len(cycles) > 10000,
    )
    ax_ts.axhline(
        stats["mean"],
        color="#F44336",
        linestyle="--",
        linewidth=1,
        label=f"Mean: {stats['mean']:.0f} us",
    )
    ax_ts.axhline(
        stats["p99"],
        color="#FF9800",
        linestyle=":",
        linewidth=1,
        label=f"P99: {stats['p99']:.0f} us",
    )

    if cycle_target and cycle_target > 500:
        ax_ts.axhline(
            cycle_target,
            color="#4CAF50",
            linestyle="-.",
            linewidth=1,
            label=f"Target: {cycle_target} us",
        )

    if has_errors:
        err_mask = errors > 0
        ax_ts.scatter(
            t_rel[err_mask],
            cycles[err_mask],
            color="#F44336",
            s=20,
            zorder=5,
            label=f"RX errors ({stats['err_total']} total)",
        )

    if title:
        ax_ts.set_title(title)
    ax_ts.set_xlabel("Time (s)")
    ax_ts.set_ylabel("Cycle time (us)")
    ax_ts.legend(loc="upper right")
    ax_ts.grid(True, alpha=0.3)

    bins = np.arange(stats["min"] - 10, stats["max"] + 50, 10)
    ax_hist.hist(
        cycles,
        bins=bins,
        color="#2196F3",
        alpha=0.7,
        edgecolor="white",
        linewidth=0.3,
    )
    ax_hist.axvline(stats["mean"], color="#F44336", linestyle="--", linewidth=1)
    ax_hist.axvline(stats["p99"], color="#FF9800", linestyle=":", linewidth=1)
    ax_hist.set_xlabel("Cycle time (us)")
    ax_hist.set_ylabel("Count")
    err_str = f", errors: {stats['err_total']}" if has_errors else ""
    ax_hist.set_title(
        f"Distribution\n(std: {stats['std']:.0f} us, "
        f"min: {stats['min']}, max: {stats['max']}{err_str})"
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
        import numpy as np
    except ImportError:
        print(
            "matplotlib and numpy required: pip install matplotlib numpy",
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
    print(f"Loaded in {time.monotonic() - t_load_start:.1f}s")

    # Compute stats once per dataset and reuse for plot and final summary
    all_stats = [_compute_stats(c, e) for (_, c, e) in datasets]

    if n == 1:
        ts, cycles, errors = datasets[0]

        fig, (ax_ts, ax_hist) = plt.subplots(
            2, 1, figsize=(14, 8), gridspec_kw={"height_ratios": [3, 1]}
        )

        title = args.title or f"PiBridge Cycle Time ({len(cycles)} samples)"
        _plot_single(
            ax_ts, ax_hist, ts, cycles, errors, all_stats[0], title, args.cycle_target
        )
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

        for i, ((ts, cycles, errors), label, stats) in enumerate(
            zip(datasets, labels, all_stats)
        ):
            _plot_single(
                axes[i][0],
                axes[i][1],
                ts,
                cycles,
                errors,
                stats,
                f"{label} ({len(cycles)} samples)",
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
