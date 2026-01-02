#!/usr/bin/env python3
"""
Generate figures for k-parameter sensitivity analysis.

Usage:
    uv run reports/plot_k_sensitivity.py [--input PATH] [--output DIR]

Reads results from results/k_sensitivity/compare_k_sensitivity.json
Outputs figures to reports/figures/k_sensitivity/
"""

import json
import argparse
from pathlib import Path
import matplotlib.pyplot as plt
import numpy as np


def load_results(input_path: Path) -> dict:
    """Load k-sensitivity results from JSON file."""
    with open(input_path) as f:
        return json.load(f)


def pivot_results(results: list) -> dict:
    """Pivot results by implementation."""
    by_impl = {}
    for r in results:
        impl = r["implementation"]
        if impl not in by_impl:
            by_impl[impl] = {"k": [], "throughput": [], "mean_error": [], "max_error": [], "memory": []}
        by_impl[impl]["k"].append(r["k"])
        by_impl[impl]["throughput"].append(r["inserts_per_sec"] / 1e6)  # Convert to M ops/sec
        by_impl[impl]["mean_error"].append(r["mean_error_pct"])
        by_impl[impl]["max_error"].append(r["max_error_pct"])
        by_impl[impl]["memory"].append(r["memory_bytes"] / 1024)  # Convert to KB
    return by_impl


def plot_throughput_vs_k(by_impl: dict, output_dir: Path):
    """Plot insert throughput vs k parameter."""
    fig, ax = plt.subplots(figsize=(10, 6))

    colors = {
        "Spectral BF (MI)": "#2ecc71",
        "Count-Min Sketch": "#e74c3c",
        "CascadeCBF (MI)": "#9b59b6",
    }
    markers = {
        "Spectral BF (MI)": "o",
        "Count-Min Sketch": "s",
        "CascadeCBF (MI)": "D",
    }

    for impl, data in by_impl.items():
        ax.plot(data["k"], data["throughput"],
                label=impl, color=colors.get(impl, "gray"),
                marker=markers.get(impl, "o"), markersize=4, linewidth=1.5)

    ax.set_xlabel("k (number of hash functions)", fontsize=12)
    ax.set_ylabel("Insert Throughput (M ops/sec)", fontsize=12)
    ax.set_title("Insert Throughput vs k Parameter", fontsize=14)
    ax.legend(loc="upper right", fontsize=10)
    ax.grid(True, alpha=0.3)
    ax.set_xlim(0, 33)
    ax.set_ylim(0, None)

    plt.tight_layout()
    output_path = output_dir / "k_sensitivity_throughput.png"
    plt.savefig(output_path, dpi=150)
    plt.savefig(output_dir / "k_sensitivity_throughput.pdf")
    print(f"Saved: {output_path}")
    plt.close()


def plot_accuracy_vs_k(by_impl: dict, output_dir: Path):
    """Plot mean error vs k parameter (zoom on 0-50% range)."""
    fig, ax = plt.subplots(figsize=(10, 6))

    colors = {
        "Spectral BF (MI)": "#2ecc71",
        "Count-Min Sketch": "#e74c3c",
        "CascadeCBF (MI)": "#9b59b6",
    }
    markers = {
        "Spectral BF (MI)": "o",
        "Count-Min Sketch": "s",
        "CascadeCBF (MI)": "D",
    }

    # Plot all implementations, clamping very high values for visualization
    for impl, data in by_impl.items():
        clamped_error = [min(e, 50) for e in data["mean_error"]]
        ax.plot(data["k"], clamped_error,
                label=impl, color=colors.get(impl, "gray"),
                marker=markers.get(impl, "o"), markersize=4, linewidth=1.5)

    ax.set_xlabel("k (number of hash functions)", fontsize=12)
    ax.set_ylabel("Mean Error (%)", fontsize=12)
    ax.set_title("Query Accuracy vs k Parameter (Zipfian α=1.5)", fontsize=14)
    ax.legend(loc="upper right", fontsize=10)
    ax.grid(True, alpha=0.3)
    ax.set_xlim(0, 33)
    ax.set_ylim(-1, 50)  # Focus on 0-50% range

    # Add annotations
    ax.axhline(y=0, color='gray', linestyle='--', alpha=0.5)
    ax.annotate("CascadeCBF: 0% at k>=4", xy=(15, 2), fontsize=10, color='#9b59b6')
    ax.annotate("CMS: 0% at k>=11", xy=(15, 5), fontsize=10, color='#e74c3c')

    plt.tight_layout()
    output_path = output_dir / "k_sensitivity_accuracy.png"
    plt.savefig(output_path, dpi=150)
    plt.savefig(output_dir / "k_sensitivity_accuracy.pdf")
    print(f"Saved: {output_path}")
    plt.close()


def plot_combined(by_impl: dict, output_dir: Path):
    """Create a combined 2-panel figure with all implementations."""
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 5))

    colors = {
        "Spectral BF (MI)": "#2ecc71",
        "Count-Min Sketch": "#e74c3c",
        "CascadeCBF (MI)": "#9b59b6",
    }
    markers = {
        "Spectral BF (MI)": "o",
        "Count-Min Sketch": "s",
        "CascadeCBF (MI)": "D",
    }

    # Panel 1: Throughput (all implementations)
    for impl, data in by_impl.items():
        ax1.plot(data["k"], data["throughput"],
                label=impl, color=colors.get(impl, "gray"),
                marker=markers.get(impl, "o"), markersize=4, linewidth=1.5)

    ax1.set_xlabel("k (number of hash functions)", fontsize=12)
    ax1.set_ylabel("Insert Throughput (M ops/sec)", fontsize=12)
    ax1.set_title("(a) Insert Throughput", fontsize=12)
    ax1.legend(loc="upper right", fontsize=9)
    ax1.grid(True, alpha=0.3)
    ax1.set_xlim(0, 33)

    # Panel 2: Accuracy (all implementations, clamped)
    for impl, data in by_impl.items():
        clamped_error = [min(e, 50) for e in data["mean_error"]]
        ax2.plot(data["k"], clamped_error,
                label=impl, color=colors.get(impl, "gray"),
                marker=markers.get(impl, "o"), markersize=4, linewidth=1.5)

    ax2.set_xlabel("k (number of hash functions)", fontsize=12)
    ax2.set_ylabel("Mean Error (%)", fontsize=12)
    ax2.set_title("(b) Query Accuracy", fontsize=12)
    ax2.legend(loc="upper right", fontsize=9)
    ax2.grid(True, alpha=0.3)
    ax2.set_xlim(0, 33)
    ax2.set_ylim(-1, 50)
    ax2.axhline(y=0, color='gray', linestyle='--', alpha=0.5)

    plt.tight_layout()
    output_path = output_dir / "k_sensitivity_combined.png"
    plt.savefig(output_path, dpi=150)
    plt.savefig(output_dir / "k_sensitivity_combined.pdf")
    print(f"Saved: {output_path}")
    plt.close()


def plot_convergence_comparison(by_impl: dict, output_dir: Path):
    """Plot comparing when each implementation converges to 0% error."""
    fig, ax = plt.subplots(figsize=(10, 6))

    colors = {
        "Spectral BF (MI)": "#2ecc71",
        "Count-Min Sketch": "#e74c3c",
        "CascadeCBF (MI)": "#9b59b6",
    }

    # Find k where error drops to 0 for each implementation
    convergence = {}
    for impl, data in by_impl.items():
        for k, err in zip(data["k"], data["mean_error"]):
            if err < 0.01:  # Effectively 0
                convergence[impl] = k
                break
        else:
            convergence[impl] = 33  # Didn't converge

    # Bar chart
    impls = list(convergence.keys())
    k_vals = [convergence[impl] for impl in impls]
    bar_colors = [colors.get(impl, "gray") for impl in impls]

    bars = ax.bar(impls, k_vals, color=bar_colors, alpha=0.8)
    ax.set_ylabel("k required for 0% error", fontsize=12)
    ax.set_title("Hash Functions Required for Perfect Accuracy", fontsize=14)
    ax.set_ylim(0, 15)
    ax.grid(True, alpha=0.3, axis='y')

    # Add value labels
    for bar, k in zip(bars, k_vals):
        ax.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.2,
                f'k={k}', ha='center', va='bottom', fontsize=11, fontweight='bold')

    plt.tight_layout()
    output_path = output_dir / "k_sensitivity_convergence.png"
    plt.savefig(output_path, dpi=150)
    plt.savefig(output_dir / "k_sensitivity_convergence.pdf")
    print(f"Saved: {output_path}")
    plt.close()


def main():
    parser = argparse.ArgumentParser(description="Generate k-sensitivity figures")
    parser.add_argument("--input", type=Path,
                        default=Path("results/k_sensitivity/compare_k_sensitivity.json"),
                        help="Input JSON file path")
    parser.add_argument("--output", type=Path,
                        default=Path("reports/figures/k_sensitivity"),
                        help="Output directory for figures")
    args = parser.parse_args()

    # Handle relative paths from build directory
    if not args.input.exists():
        # Try from project root
        project_root = Path(__file__).parent.parent
        args.input = project_root / args.input

    if not args.input.exists():
        # Try from build directory
        args.input = Path("build") / "results/k_sensitivity/compare_k_sensitivity.json"

    if not args.input.exists():
        print(f"Error: Input file not found: {args.input}")
        print("Run the benchmark first: ./build/globimap_test_k_compare")
        return 1

    # Create output directory
    args.output.mkdir(parents=True, exist_ok=True)

    print(f"Loading results from: {args.input}")
    data = load_results(args.input)
    by_impl = pivot_results(data["results"])

    print(f"Generating figures in: {args.output}")
    plot_throughput_vs_k(by_impl, args.output)
    plot_accuracy_vs_k(by_impl, args.output)
    plot_combined(by_impl, args.output)
    plot_convergence_comparison(by_impl, args.output)

    print("\nDone!")
    return 0


if __name__ == "__main__":
    exit(main())
