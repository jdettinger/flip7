#!/usr/bin/env python3
"""Rank Flip 7 sweep output and render charts with Matplotlib."""

from __future__ import annotations

import argparse
import csv
import math
from pathlib import Path
import sys
from typing import Dict, List

import matplotlib

matplotlib.use("Agg")

import matplotlib.pyplot as plt

REQUIRED_COLUMNS = {"strategy", "parameters", "episodes", "mean_rounds"}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Rank Flip 7 strategies by fewest average rounds and plot with Matplotlib."
        )
    )
    parser.add_argument("results", type=Path, help="Input results CSV")
    parser.add_argument(
        "--top",
        type=int,
        default=20,
        help="Configurations shown in each chart (default: 20)",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        help="Output directory (default: <results-name>_analysis next to input)",
    )
    args = parser.parse_args()
    if args.top < 1:
        parser.error("--top must be at least 1")
    return args


def load_results(path: Path) -> List[Dict[str, str]]:
    try:
        with path.open("r", newline="", encoding="utf-8-sig") as source:
            reader = csv.DictReader(source)
            columns = reader.fieldnames
            if not columns:
                raise ValueError("CSV is empty or has no header")
            missing = REQUIRED_COLUMNS.difference(columns)
            if missing:
                raise ValueError(
                    "CSV is missing required columns: " + ", ".join(sorted(missing))
                )

            rows = []
            for line_number, row in enumerate(reader, start=2):
                if not row.get("strategy", "").strip():
                    raise ValueError(f"line {line_number}: strategy is empty")
                try:
                    episodes = int(row["episodes"])
                    if episodes <= 0:
                        raise ValueError("episodes must be positive")
                    mean_rounds = float(row["mean_rounds"])
                    if not math.isfinite(mean_rounds) or mean_rounds < 0:
                        raise ValueError("mean_rounds must be finite and nonnegative")
                except (KeyError, TypeError, ValueError) as error:
                    raise ValueError(f"line {line_number}: {error}") from error
                rows.append(row)

    except OSError as error:
        raise ValueError(f"cannot read {path}: {error}") from error

    if not rows:
        raise ValueError("CSV contains no result rows")
    return rows


def rank_rows(rows: List[Dict[str, str]]) -> List[Dict[str, str]]:
    return sorted(
        rows,
        key=lambda row: (
            float(row["mean_rounds"]),
            row["strategy"],
            row["parameters"],
        ),
    )


def write_ranking(
    path: Path,
    rows: List[Dict[str, str]],
) -> None:
    fieldnames = ["rank", "strategy", "parameters", "episodes", "mean_rounds"]
    try:
        with path.open("w", newline="", encoding="utf-8") as output:
            writer = csv.DictWriter(output, fieldnames=fieldnames)
            writer.writeheader()
            for rank, row in enumerate(rank_rows(rows), start=1):
                writer.writerow(
                    {
                        "rank": rank,
                        "strategy": row["strategy"],
                        "parameters": row["parameters"],
                        "episodes": row["episodes"],
                        "mean_rounds": row["mean_rounds"],
                    }
                )
    except OSError as error:
        raise ValueError(f"cannot write {path}: {error}") from error


def display_label(row: Dict[str, str]) -> str:
    parameters = row.get("parameters", "")
    label = row["strategy"]
    if parameters:
        label += "  " + parameters
    if len(label) > 58:
        label = label[:55] + "..."
    return label


def render_chart(
    path: Path,
    title: str,
    rows: List[Dict[str, str]],
    top: int,
) -> None:
    display_rows = rank_rows(rows)[:top]
    values = [float(row["mean_rounds"]) for row in display_rows]
    try:
        labels = [display_label(row) for row in display_rows]
        height = max(4.0, 0.38 * len(display_rows) + 1.8)
        figure, axis = plt.subplots(figsize=(12, height))
        axis.barh(labels, values, color="#2563eb")
        axis.invert_yaxis()
        axis.set_title(
            f"{title}\nTop {len(display_rows)} of {len(rows)} configurations · "
            "fewer rounds is better",
            loc="left",
            fontsize=13,
            pad=14,
        )
        axis.set_xlabel("Mean rounds per episode", labelpad=8)
        axis.grid(axis="x", color="#e5e7eb", linewidth=0.8)
        axis.set_axisbelow(True)
        axis.spines["top"].set_visible(False)
        axis.spines["right"].set_visible(False)
        axis.spines["left"].set_visible(False)
        axis.set_xlim(left=0)
        value_format = lambda value: f"{value:.2f}"

        axis.tick_params(axis="y", length=0, labelsize=9)
        axis.tick_params(axis="x", colors="#4b5563", labelsize=9)
        axis.bar_label(
            axis.containers[0],
            labels=[value_format(value) for value in values],
            padding=4,
            fontsize=8,
        )
        figure.tight_layout()
        figure.savefig(path, dpi=160, bbox_inches="tight")
        plt.close(figure)
    except (OSError, ValueError) as error:
        raise ValueError(f"cannot write {path}: {error}") from error


def main() -> int:
    args = parse_args()
    try:
        rows = load_results(args.results)
        output_dir = args.output_dir or (
            args.results.parent / f"{args.results.stem}_analysis"
        )
        output_dir.mkdir(parents=True, exist_ok=True)

        ranking_path = output_dir / "ranked_by_mean_rounds.csv"
        chart_path = output_dir / "mean_rounds_ranking.png"
        write_ranking(ranking_path, rows)
        render_chart(
            chart_path,
            "Fewest mean rounds per episode",
            rows,
            args.top,
        )

    except (OSError, ValueError) as error:
        print(f"rank_results: {error}", file=sys.stderr)
        return 2

    print(f"Ranked {len(rows)} configurations by fewest mean rounds: {ranking_path}")
    print(f"Created Matplotlib chart: {chart_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
