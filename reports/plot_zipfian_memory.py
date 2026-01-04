#!/usr/bin/env python3
"""
Generate figures for Zipfian memory efficiency analysis.

Usage:
    uv run reports/plot_zipfian_memory.py [--input PATH] [--output DIR]

Reads results from results/zipfian_memory/compare_zipfian_memory.json
Outputs figures to reports/figures/zipfian_memory/
"""

import json
import argparse
from pathlib import Path
import matplotlib.pyplot as plt
import numpy as np


def load_results(input_path: Path) -> dict:
    """Load Zipfian memory results from JSON file."""
    with open(input_path) as f:
        return json.load(f)


def pivot_results(results: list) -> dict:
    """Pivot results by (implementation, alpha)."""
    by_impl_alpha = {}
    for r in results:
        impl = r["implementation"]
        alpha = r["alpha"]
        key = (impl, alpha)
        if key not in by_impl_alpha:
            by_impl_alpha[key] = {"memory_kb": [], "mean_error": [], "max_error": [], "throughput": []}
        by_impl_alpha[key]["memory_kb"].append(r["memory_kb"])
        by_impl_alpha[key]["mean_error"].append(r["mean_error_pct"])
        by_impl_alpha[key]["max_error"].append(r["max_error_pct"])
        by_impl_alpha[key]["throughput"].append(r["inserts_per_sec"] / 1e6)
    return by_impl_alpha


# Implementation colors and styles
IMPL_COLORS = {
    # CascadeCBF variants - purple spectrum
    "CascadeCBF-16 (1L)": "#d4b3e0",     # Light purple
    "CascadeCBF-16 MI": "#b87dcc",       # Medium purple
    "CascadeCBF-16 IH": "#a855c4",       # Brighter purple
    "CascadeCBF-16 MI+IH": "#9b59b6",    # Purple (main)
    "CascadeCBF-16-16 MI+IH": "#7d3c98", # Dark purple (2-layer)
    "CascadeCBF (MI)": "#9b59b6",        # Legacy name
    # Other implementations
    "Spectral BF (MI)": "#2ecc71",  # Green
    "Count-Min Sketch": "#e74c3c",  # Red
    "d-Left CBF": "#3498db",  # Blue
    "VI-CBF": "#95a5a6",  # Gray
}

IMPL_MARKERS = {
    "CascadeCBF-16 (1L)": "v",
    "CascadeCBF-16 MI": "D",
    "CascadeCBF-16 IH": "p",
    "CascadeCBF-16 MI+IH": "D",
    "CascadeCBF-16-16 MI+IH": "*",
    "CascadeCBF (MI)": "D",
    "Spectral BF (MI)": "o",
    "Count-Min Sketch": "s",
    "d-Left CBF": "^",
    "VI-CBF": "x",
}

# Short names for legend
IMPL_NAMES = {
    "CascadeCBF-16 (1L)": "CCBF (std)",
    "CascadeCBF-16 MI": "CCBF (MI)",
    "CascadeCBF-16 IH": "CCBF (IH)",
    "CascadeCBF-16 MI+IH": "CCBF (MI+IH)",
    "CascadeCBF-16-16 MI+IH": "CCBF 2L",
    "CascadeCBF (MI)": "CascadeCBF",
    "Spectral BF (MI)": "Spectral BF",
    "Count-Min Sketch": "Count-Min",
    "d-Left CBF": "d-Left",
    "VI-CBF": "VI-CBF",
}


def plot_error_vs_memory_3panel(by_impl_alpha: dict, alphas: list, output_dir: Path):
    """Create a 3-panel figure showing error vs memory for each alpha."""
    fig, axes = plt.subplots(1, 3, figsize=(14, 4), sharey=True)

    # Show key CascadeCBF variants and competitors (exclude VI-CBF due to extreme error)
    impls_to_show = ["CascadeCBF-16 MI+IH", "CascadeCBF-16-16 MI+IH",
                     "Spectral BF (MI)", "Count-Min Sketch", "d-Left CBF"]

    for ax, alpha in zip(axes, alphas):
        for impl in impls_to_show:
            key = (impl, alpha)
            if key in by_impl_alpha:
                data = by_impl_alpha[key]
                ax.plot(data["memory_kb"], data["mean_error"],
                       label=IMPL_NAMES.get(impl, impl),
                       color=IMPL_COLORS.get(impl, "gray"),
                       marker=IMPL_MARKERS.get(impl, "o"),
                       markersize=5, linewidth=1.5)

        ax.set_xlabel("Memory (KB)", fontsize=11)
        ax.set_xscale("log")
        ax.set_title(f"$\\alpha$ = {alpha:.1f}", fontsize=12)
        ax.grid(True, alpha=0.3)
        ax.set_xlim(1, 500)
        ax.axhline(y=0, color='gray', linestyle='--', alpha=0.5)

    axes[0].set_ylabel("Mean Error (%)", fontsize=11)
    axes[0].set_ylim(-0.5, 10)  # Zoom in to show differences

    # Single legend for all panels
    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc='upper center', ncol=4, fontsize=10,
               bbox_to_anchor=(0.5, 1.02))

    plt.tight_layout()
    plt.subplots_adjust(top=0.85)

    output_path = output_dir / "memory_efficiency_3panel.png"
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.savefig(output_dir / "memory_efficiency_3panel.pdf", bbox_inches='tight')
    print(f"Saved: {output_path}")
    plt.close()


def plot_error_vs_memory_combined(by_impl_alpha: dict, output_dir: Path):
    """Create a combined figure with alpha=1.5 (main focus for paper)."""
    fig, ax = plt.subplots(figsize=(8.2 / 2.54 * 1.2, 6 / 2.54 * 1.2))  # AGILE column width

    alpha = 1.5
    impls_to_show = ["CascadeCBF-16 MI+IH", "CascadeCBF-16-16 MI+IH",
                     "Spectral BF (MI)", "Count-Min Sketch", "d-Left CBF"]

    for impl in impls_to_show:
        key = (impl, alpha)
        if key in by_impl_alpha:
            data = by_impl_alpha[key]
            ax.plot(data["memory_kb"], data["mean_error"],
                   label=IMPL_NAMES.get(impl, impl),
                   color=IMPL_COLORS.get(impl, "gray"),
                   marker=IMPL_MARKERS.get(impl, "o"),
                   markersize=5, linewidth=1.5)

    ax.set_xlabel("Memory (KB)", fontsize=10)
    ax.set_ylabel("Mean Error (%)", fontsize=10)
    ax.set_xscale("log")
    ax.set_title(f"Zipfian $\\alpha$ = {alpha}", fontsize=11)
    ax.grid(True, alpha=0.3)
    ax.set_xlim(2, 500)
    ax.set_ylim(-0.5, 10)  # Zoom in to show differences
    ax.axhline(y=0, color='gray', linestyle='--', alpha=0.5)
    ax.legend(loc='upper right', fontsize=8)

    plt.tight_layout()
    output_path = output_dir / "memory_efficiency.png"
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.savefig(output_dir / "memory_efficiency.pdf", bbox_inches='tight')
    print(f"Saved: {output_path}")
    plt.close()


def plot_memory_at_zero_error(by_impl_alpha: dict, alphas: list, output_dir: Path):
    """Bar chart showing memory required to achieve <1% error for each implementation."""
    fig, ax = plt.subplots(figsize=(10, 5))

    impls_to_show = ["CascadeCBF-16 MI+IH", "CascadeCBF-16-16 MI+IH",
                     "Spectral BF (MI)", "Count-Min Sketch"]

    # Find memory at which error drops below 1%
    memory_for_zero = {impl: {alpha: None for alpha in alphas} for impl in impls_to_show}

    for (impl, alpha), data in by_impl_alpha.items():
        if impl not in impls_to_show:
            continue
        for mem, err in zip(data["memory_kb"], data["mean_error"]):
            if err < 1.0:  # <1% error threshold
                memory_for_zero[impl][alpha] = mem
                break

    # Create grouped bar chart
    x = np.arange(len(alphas))
    width = 0.2

    for i, impl in enumerate(impls_to_show):
        memories = [memory_for_zero[impl][a] if memory_for_zero[impl][a] else 500 for a in alphas]
        ax.bar(x + i * width, memories, width,
               label=IMPL_NAMES.get(impl, impl),
               color=IMPL_COLORS.get(impl, "gray"),
               alpha=0.8)

    ax.set_xlabel("Zipfian $\\alpha$", fontsize=12)
    ax.set_ylabel("Memory for <1% Error (KB)", fontsize=12)
    ax.set_title("Memory Required to Achieve <1% Mean Error", fontsize=14)
    ax.set_xticks(x + width * 1.5)
    ax.set_xticklabels([f"$\\alpha$ = {a}" for a in alphas])
    ax.legend(loc='upper right', fontsize=10)
    ax.grid(True, alpha=0.3, axis='y')
    ax.set_yscale("log")

    plt.tight_layout()
    output_path = output_dir / "memory_at_zero_error.png"
    plt.savefig(output_path, dpi=150)
    plt.savefig(output_dir / "memory_at_zero_error.pdf")
    print(f"Saved: {output_path}")
    plt.close()


def plot_paper_figure(by_impl_alpha: dict, alphas: list, output_dir: Path):
    """Create the main figure for the shortpaper - compact 3-panel layout."""
    # Set up figure with AGILE column width (8.2cm)
    fig, axes = plt.subplots(1, 3, figsize=(8.2 / 2.54 * 1.5, 5 / 2.54 * 1.2), sharey=True)

    # Show best CascadeCBF variant (single-layer MI+IH) against main competitors
    impls_to_show = ["CascadeCBF-16 MI+IH", "CascadeCBF-16-16 MI+IH",
                     "Spectral BF (MI)", "Count-Min Sketch"]

    for ax, alpha in zip(axes, alphas):
        for impl in impls_to_show:
            key = (impl, alpha)
            if key in by_impl_alpha:
                data = by_impl_alpha[key]
                # Clamp error at 100% for readability
                clamped_error = [min(e, 100) for e in data["mean_error"]]
                ax.plot(data["memory_kb"], clamped_error,
                       label=IMPL_NAMES.get(impl, impl),
                       color=IMPL_COLORS.get(impl, "gray"),
                       marker=IMPL_MARKERS.get(impl, "o"),
                       markersize=4, linewidth=1.2)

        ax.set_xlabel("Memory (KB)", fontsize=9)
        ax.set_xscale("log")
        ax.set_title(f"$\\alpha$ = {alpha:.1f}", fontsize=10)
        ax.grid(True, alpha=0.3)
        ax.set_xlim(2, 400)
        ax.axhline(y=1, color='gray', linestyle='--', alpha=0.5, linewidth=0.8)
        ax.tick_params(labelsize=8)

    axes[0].set_ylabel("Mean Error (%)", fontsize=9)
    axes[0].set_ylim(-0.5, 10)  # Zoom in to show differences

    # Single legend below
    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc='upper center', ncol=4, fontsize=8,
               bbox_to_anchor=(0.5, 1.0), frameon=False)

    plt.tight_layout()
    plt.subplots_adjust(top=0.82, wspace=0.08)

    # Save for paper
    output_path = output_dir / "zipfian_memory_paper.pdf"
    plt.savefig(output_path, bbox_inches='tight')
    plt.savefig(output_dir / "zipfian_memory_paper.png", dpi=300, bbox_inches='tight')
    print(f"Saved: {output_path}")

    # Also copy to shortpaper figures directory
    shortpaper_fig = Path("cbf-shortpaper/figures/memory_efficiency.pdf")
    if shortpaper_fig.parent.exists():
        import shutil
        shutil.copy(output_path, shortpaper_fig)
        print(f"Copied to: {shortpaper_fig}")

    plt.close()


def print_summary_table(by_impl_alpha: dict, alphas: list):
    """Print a summary table showing key findings."""
    print("\n" + "=" * 80)
    print("SUMMARY: Memory required for <1% error")
    print("=" * 80)

    impls = ["CascadeCBF-16 MI+IH", "CascadeCBF-16-16 MI+IH",
             "Spectral BF (MI)", "Count-Min Sketch"]

    # Header
    print(f"{'Implementation':<20}", end="")
    for alpha in alphas:
        print(f"  alpha={alpha:<8}", end="")
    print()
    print("-" * 80)

    for impl in impls:
        print(f"{IMPL_NAMES.get(impl, impl):<20}", end="")
        for alpha in alphas:
            key = (impl, alpha)
            if key in by_impl_alpha:
                data = by_impl_alpha[key]
                # Find first memory where error < 1%
                mem_zero = None
                for mem, err in zip(data["memory_kb"], data["mean_error"]):
                    if err < 1.0:
                        mem_zero = mem
                        break
                if mem_zero:
                    print(f"  {mem_zero:>6.0f} KB  ", end="")
                else:
                    print(f"  {'> 384 KB':>10}", end="")
            else:
                print(f"  {'N/A':>10}", end="")
        print()
    print()


def main():
    parser = argparse.ArgumentParser(description="Generate Zipfian memory efficiency figures")
    parser.add_argument("--input", type=Path,
                        default=Path("results/zipfian_memory/compare_zipfian_memory.json"),
                        help="Input JSON file path")
    parser.add_argument("--output", type=Path,
                        default=Path("reports/figures/zipfian_memory"),
                        help="Output directory for figures")
    args = parser.parse_args()

    # Handle relative paths
    if not args.input.exists():
        project_root = Path(__file__).parent.parent
        args.input = project_root / args.input

    if not args.input.exists():
        print(f"Error: Input file not found: {args.input}")
        print("Run the benchmark first: ./build/globimap_test_zipfian_memory")
        return 1

    # Create output directory
    args.output.mkdir(parents=True, exist_ok=True)

    print(f"Loading results from: {args.input}")
    data = load_results(args.input)
    by_impl_alpha = pivot_results(data["results"])
    alphas = data["parameters"]["alphas"]

    print(f"Generating figures in: {args.output}")

    # Generate all figures
    plot_error_vs_memory_3panel(by_impl_alpha, alphas, args.output)
    plot_error_vs_memory_combined(by_impl_alpha, args.output)
    plot_memory_at_zero_error(by_impl_alpha, alphas, args.output)
    plot_paper_figure(by_impl_alpha, alphas, args.output)

    # Print summary
    print_summary_table(by_impl_alpha, alphas)

    print("\nDone!")
    return 0


if __name__ == "__main__":
    exit(main())
