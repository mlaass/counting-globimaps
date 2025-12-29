#!/usr/bin/env python3
"""Generate scalability figures for SBBF paper.

Usage:
    uv run scripts/plot_scalability.py --input sbbf_results/scalability --output sbbf_results/figures
"""

import argparse
import json
import glob
from pathlib import Path
from collections import defaultdict

import matplotlib.pyplot as plt
import numpy as np

# Style configuration for academic papers
plt.rcParams.update({
    'font.size': 14,
    'axes.labelsize': 16,
    'axes.titlesize': 18,
    'legend.fontsize': 12,
    'xtick.labelsize': 12,
    'ytick.labelsize': 12,
    'figure.figsize': (6, 4),
    'figure.dpi': 150,
    'savefig.bbox': 'tight',
    'savefig.pad_inches': 0.1,
})

# Series configuration: (filter_pattern, strategy) -> style
# strategy=None means match any strategy (for baselines)
SERIES_CONFIG = [
    # SBBF variants - same color per SFC, different linestyle per strategy
    ('SBBF-Morton', 'double_hash',    {'color': '#1f77b4', 'marker': 'o', 'linestyle': '-',  'label': 'SBBF-Morton-DH'}),
    ('SBBF-Morton', 'pattern_lookup', {'color': '#1f77b4', 'marker': 'o', 'linestyle': '--', 'label': 'SBBF-Morton-PL'}),
    ('SBBF-Hilbert', 'double_hash',   {'color': '#ff7f0e', 'marker': 's', 'linestyle': '-',  'label': 'SBBF-Hilbert-DH'}),
    ('SBBF-Hilbert', 'pattern_lookup',{'color': '#ff7f0e', 'marker': 's', 'linestyle': '--', 'label': 'SBBF-Hilbert-PL'}),
    # Baselines - match any strategy
    ('BlockedBF', None,               {'color': '#2ca02c', 'marker': '^', 'linestyle': '-',  'label': 'BlockedBF'}),
    ('RegisterBF', None,              {'color': '#d62728', 'marker': 'v', 'linestyle': '-',  'label': 'RegisterBF'}),
]


def load_results(results_dir: str) -> list:
    """Load all scalability JSON results."""
    data = []
    files = sorted(glob.glob(f"{results_dir}/scale_*.json"))
    if not files:
        raise FileNotFoundError(f"No scale_*.json files found in {results_dir}")

    for f in files:
        with open(f) as fp:
            result = json.load(fp)
            data.append(result)

    print(f"Loaded {len(data)} result files")
    return data


def extract_series(results: list, filter_pattern: str, dim: int, metric: str,
                   strategy: str = None) -> tuple:
    """Extract data series for plotting.

    Args:
        results: List of benchmark results
        filter_pattern: Pattern to match filter name (e.g., 'SBBF-Morton')
        dim: Dimension (2 or 3)
        metric: Metric name (e.g., 'insert_ns', 'query_ns', 'fpr')
        strategy: Strategy to match (None = any strategy)

    Returns:
        Tuple of (elements, values) arrays
    """
    elements = []
    values = []

    for result in results:
        for entry in result.get('results', []):
            config = entry.get('config', {})
            metrics_data = entry.get('metrics', {})

            name = config.get('name', '')
            entry_dim = config.get('dimensions', 2)
            entry_strategy = config.get('strategy', '')

            # Check filter pattern match
            if filter_pattern not in name:
                continue

            # Check dimension match
            if entry_dim != dim:
                continue

            # Check strategy match (None means any)
            if strategy is not None and entry_strategy != strategy:
                continue

            n = config.get('num_elements', 0)
            v = metrics_data.get(metric, None)
            if n > 0 and v is not None:
                elements.append(n)
                values.append(v)

    # Sort by elements
    if elements:
        sorted_pairs = sorted(zip(elements, values))
        elements, values = zip(*sorted_pairs)

    return np.array(elements), np.array(values)


def plot_latency_scalability(results: list, output_dir: str, dim: int, metric: str, title: str, ylabel: str):
    """Plot latency vs element count for a given dimension and metric."""
    fig, ax = plt.subplots()

    has_data = False

    for filter_pattern, strategy, style in SERIES_CONFIG:
        elements, values = extract_series(results, filter_pattern, dim, metric, strategy)

        if len(elements) > 0:
            has_data = True
            ax.plot(elements, values,
                   marker=style['marker'], color=style['color'],
                   linestyle=style['linestyle'], label=style['label'],
                   markersize=6, linewidth=1.5)

    if not has_data:
        print(f"  Warning: No data for {dim}D {metric}")
        plt.close()
        return

    ax.set_xscale('log')
    ax.set_xlabel('Number of Elements')
    ax.set_ylabel(ylabel)
    ax.set_title(title)
    ax.legend(loc='best')
    ax.grid(True, alpha=0.3)

    # Format x-axis with readable labels
    ax.xaxis.set_major_formatter(plt.FuncFormatter(lambda x, _: f'{x/1e6:.0f}M' if x >= 1e6 else f'{x/1e3:.0f}K'))

    # Save
    base_name = f"{metric}_{dim}d"
    for ext in ['pdf', 'png']:
        filepath = Path(output_dir) / f"{base_name}.{ext}"
        fig.savefig(filepath, dpi=150 if ext == 'png' else None)
        print(f"  Saved: {filepath}")

    plt.close()


def plot_fpr_scalability(results: list, output_dir: str, dim: int):
    """Plot FPR vs element count."""
    fig, ax = plt.subplots()

    has_data = False

    for filter_pattern, strategy, style in SERIES_CONFIG:
        elements, values = extract_series(results, filter_pattern, dim, 'fpr', strategy)

        if len(elements) > 0:
            has_data = True
            # Convert to percentage
            ax.plot(elements, values * 100,
                   marker=style['marker'], color=style['color'],
                   linestyle=style['linestyle'], label=style['label'],
                   markersize=6, linewidth=1.5)

    if not has_data:
        print(f"  Warning: No FPR data for {dim}D")
        plt.close()
        return

    ax.set_xscale('log')
    ax.set_xlabel('Number of Elements')
    ax.set_ylabel('False Positive Rate (%)')
    ax.set_title(f'{dim}D Filter False Positive Rate vs Scale')
    ax.legend(loc='best')
    ax.grid(True, alpha=0.3)

    ax.xaxis.set_major_formatter(plt.FuncFormatter(lambda x, _: f'{x/1e6:.0f}M' if x >= 1e6 else f'{x/1e3:.0f}K'))

    base_name = f"fpr_{dim}d"
    for ext in ['pdf', 'png']:
        filepath = Path(output_dir) / f"{base_name}.{ext}"
        fig.savefig(filepath, dpi=150 if ext == 'png' else None)
        print(f"  Saved: {filepath}")

    plt.close()


def plot_memory_scalability(results: list, output_dir: str, dim: int):
    """Plot memory usage vs element count."""
    fig, ax = plt.subplots()

    has_data = False

    for filter_pattern, strategy, style in SERIES_CONFIG:
        elements, values = extract_series(results, filter_pattern, dim, 'memory_bytes', strategy)

        if len(elements) > 0:
            has_data = True
            # Convert to MB
            ax.plot(elements, values / (1024 * 1024),
                   marker=style['marker'], color=style['color'],
                   linestyle=style['linestyle'], label=style['label'],
                   markersize=6, linewidth=1.5)

    if not has_data:
        print(f"  Warning: No memory data for {dim}D")
        plt.close()
        return

    ax.set_xscale('log')
    ax.set_yscale('log')
    ax.set_xlabel('Number of Elements')
    ax.set_ylabel('Memory Usage (MB)')
    ax.set_title(f'{dim}D Filter Memory Usage vs Scale')
    ax.legend(loc='best')
    ax.grid(True, alpha=0.3)

    ax.xaxis.set_major_formatter(plt.FuncFormatter(lambda x, _: f'{x/1e6:.0f}M' if x >= 1e6 else f'{x/1e3:.0f}K'))

    base_name = f"memory_{dim}d"
    for ext in ['pdf', 'png']:
        filepath = Path(output_dir) / f"{base_name}.{ext}"
        fig.savefig(filepath, dpi=150 if ext == 'png' else None)
        print(f"  Saved: {filepath}")

    plt.close()


def plot_combined_latency(results: list, output_dir: str, dim: int):
    """Create a combined figure with insert and query latency."""
    fig, axes = plt.subplots(1, 2, figsize=(10, 4))

    metrics = [
        ('insert_ns', 'Insert Latency (ns)', axes[0]),
        ('query_ns', 'Query Latency (ns)', axes[1]),
    ]

    for metric, ylabel, ax in metrics:
        has_data = False

        for filter_pattern, strategy, style in SERIES_CONFIG:
            elements, values = extract_series(results, filter_pattern, dim, metric, strategy)

            if len(elements) > 0:
                has_data = True
                ax.plot(elements, values,
                       marker=style['marker'], color=style['color'],
                       linestyle=style['linestyle'], label=style['label'],
                       markersize=5, linewidth=1.5)

        if has_data:
            ax.set_xscale('log')
            ax.set_xlabel('Number of Elements')
            ax.set_ylabel(ylabel)
            ax.legend(loc='best', fontsize=7)
            ax.grid(True, alpha=0.3)
            ax.xaxis.set_major_formatter(plt.FuncFormatter(
                lambda x, _: f'{x/1e6:.0f}M' if x >= 1e6 else f'{x/1e3:.0f}K'))

    fig.suptitle(f'{dim}D Filter Performance vs Scale', fontsize=12)
    fig.tight_layout()

    base_name = f"latency_combined_{dim}d"
    for ext in ['pdf', 'png']:
        filepath = Path(output_dir) / f"{base_name}.{ext}"
        fig.savefig(filepath, dpi=150 if ext == 'png' else None)
        print(f"  Saved: {filepath}")

    plt.close()


def plot_query_neighbor_combined(results: list, output_dir: str, dim: int):
    """Create a combined figure with query and neighbor latency."""
    fig, axes = plt.subplots(1, 2, figsize=(10, 4))

    metrics = [
        ('query_ns', 'Query Latency (ns)', axes[0]),
        ('neighbor_ns', 'Neighbor Latency (ns)', axes[1]),
    ]

    for metric, ylabel, ax in metrics:
        has_data = False

        for filter_pattern, strategy, style in SERIES_CONFIG:
            elements, values = extract_series(results, filter_pattern, dim, metric, strategy)

            if len(elements) > 0:
                has_data = True
                ax.plot(elements, values,
                       marker=style['marker'], color=style['color'],
                       linestyle=style['linestyle'], label=style['label'],
                       markersize=5, linewidth=1.5)

        if has_data:
            ax.set_xscale('log')
            ax.set_xlabel('Number of Elements')
            ax.set_ylabel(ylabel)
            ax.legend(loc='best', fontsize=7)
            ax.grid(True, alpha=0.3)
            ax.xaxis.set_major_formatter(plt.FuncFormatter(
                lambda x, _: f'{x/1e6:.0f}M' if x >= 1e6 else f'{x/1e3:.0f}K'))

    fig.suptitle(f'{dim}D Query Performance vs Scale', fontsize=12)
    fig.tight_layout()

    base_name = f"query_neighbor_combined_{dim}d"
    for ext in ['pdf', 'png']:
        filepath = Path(output_dir) / f"{base_name}.{ext}"
        fig.savefig(filepath, dpi=150 if ext == 'png' else None)
        print(f"  Saved: {filepath}")

    plt.close()


def plot_scalability_3panel(results: list, output_dir: str, dim: int):
    """Create a 3-panel figure: Insert, Query, Neighbor latency."""
    fig, axes = plt.subplots(1, 3, figsize=(14, 3.5))

    metrics = [
        ('insert_ns', 'Insert Latency (ns)', axes[0]),
        ('query_ns', 'Query Latency (ns)', axes[1]),
        ('neighbor_ns', 'Neighbor Latency (ns)', axes[2]),
    ]

    for metric, ylabel, ax in metrics:
        has_data = False

        for filter_pattern, strategy, style in SERIES_CONFIG:
            elements, values = extract_series(results, filter_pattern, dim, metric, strategy)

            if len(elements) > 0:
                has_data = True
                ax.plot(elements, values,
                       marker=style['marker'], color=style['color'],
                       linestyle=style['linestyle'], label=style['label'],
                       markersize=5, linewidth=1.5)

        if has_data:
            ax.set_xscale('log')
            ax.set_xlabel('Number of Elements')
            ax.set_ylabel(ylabel)
            ax.legend(loc='best', fontsize=7)
            ax.grid(True, alpha=0.3)
            ax.xaxis.set_major_formatter(plt.FuncFormatter(
                lambda x, _: f'{x/1e6:.0f}M' if x >= 1e6 else f'{x/1e3:.0f}K'))

    fig.tight_layout()

    base_name = f"scalability_3panel_{dim}d"
    for ext in ['pdf', 'png']:
        filepath = Path(output_dir) / f"{base_name}.{ext}"
        fig.savefig(filepath, dpi=150 if ext == 'png' else None)
        print(f"  Saved: {filepath}")

    plt.close()


def plot_throughput_scalability(results: list, output_dir: str, dim: int):
    """Plot throughput (ops/sec) vs element count."""
    fig, axes = plt.subplots(1, 2, figsize=(10, 4))

    for idx, (metric, title, ax) in enumerate([
        ('insert_ns', 'Insert Throughput', axes[0]),
        ('query_ns', 'Query Throughput', axes[1]),
    ]):
        has_data = False

        for filter_pattern, strategy, style in SERIES_CONFIG:
            elements, values = extract_series(results, filter_pattern, dim, metric, strategy)

            if len(elements) > 0 and np.all(values > 0):
                has_data = True
                # Convert ns to ops/sec (1e9 ns per second)
                throughput = 1e9 / values / 1e6  # Million ops/sec
                ax.plot(elements, throughput,
                       marker=style['marker'], color=style['color'],
                       linestyle=style['linestyle'], label=style['label'],
                       markersize=5, linewidth=1.5)

        if has_data:
            ax.set_xscale('log')
            ax.set_xlabel('Number of Elements')
            ax.set_ylabel('Throughput (M ops/sec)')
            ax.set_title(title)
            ax.legend(loc='best', fontsize=7)
            ax.grid(True, alpha=0.3)
            ax.xaxis.set_major_formatter(plt.FuncFormatter(
                lambda x, _: f'{x/1e6:.0f}M' if x >= 1e6 else f'{x/1e3:.0f}K'))

    fig.suptitle(f'{dim}D Filter Throughput vs Scale', fontsize=12)
    fig.tight_layout()

    base_name = f"throughput_{dim}d"
    for ext in ['pdf', 'png']:
        filepath = Path(output_dir) / f"{base_name}.{ext}"
        fig.savefig(filepath, dpi=150 if ext == 'png' else None)
        print(f"  Saved: {filepath}")

    plt.close()


def generate_summary_table(results: list, output_dir: str):
    """Generate a summary table of all configurations."""
    # Collect all data
    data = defaultdict(list)

    for result in results:
        for entry in result.get('results', []):
            config = entry.get('config', {})
            metrics = entry.get('metrics', {})

            name = config.get('name', 'Unknown')
            strategy = config.get('strategy', '')
            n = config.get('num_elements', 0)
            dim = config.get('dimensions', 2)

            # Include strategy in the key
            key = (name, strategy, dim)
            data[key].append({
                'elements': n,
                'insert_ns': metrics.get('insert_ns', 0),
                'query_ns': metrics.get('query_ns', 0),
                'fpr': metrics.get('fpr', 0),
                'memory_mb': metrics.get('memory_bytes', 0) / (1024 * 1024),
            })

    # Write summary
    summary_path = Path(output_dir) / "summary.txt"
    with open(summary_path, 'w') as f:
        f.write("SBBF Scalability Benchmark Summary\n")
        f.write("=" * 70 + "\n\n")

        for (name, strategy, dim), entries in sorted(data.items()):
            label = f"{name}" + (f" [{strategy}]" if strategy else "")
            f.write(f"\n{label} ({dim}D)\n")
            f.write("-" * 50 + "\n")
            f.write(f"{'Elements':>12} {'Insert(ns)':>12} {'Query(ns)':>12} {'FPR(%)':>10} {'Memory(MB)':>12}\n")

            for e in sorted(entries, key=lambda x: x['elements']):
                f.write(f"{e['elements']:>12,} {e['insert_ns']:>12.2f} {e['query_ns']:>12.2f} "
                       f"{e['fpr']*100:>10.3f} {e['memory_mb']:>12.2f}\n")

    print(f"  Saved: {summary_path}")


def main():
    parser = argparse.ArgumentParser(description='Generate SBBF scalability figures')
    parser.add_argument('--input', '-i', default='sbbf_results/scalability',
                       help='Input directory containing scale_*.json files')
    parser.add_argument('--output', '-o', default='sbbf_results/figures',
                       help='Output directory for figures')
    args = parser.parse_args()

    # Create output directory
    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)

    print(f"Loading results from: {args.input}")
    results = load_results(args.input)

    print(f"\nGenerating figures to: {args.output}")

    # Generate plots for both 2D and 3D
    for dim in [2, 3]:
        print(f"\n{dim}D Plots:")

        # Combined plots
        plot_scalability_3panel(results, args.output, dim)          # insert + query + neighbor (3-panel)
        plot_throughput_scalability(results, args.output, dim)      # insert + query throughput
        plot_combined_latency(results, args.output, dim)            # insert + query latency
        plot_query_neighbor_combined(results, args.output, dim)     # query + neighbor latency

        # FPR and memory
        plot_fpr_scalability(results, args.output, dim)
        plot_memory_scalability(results, args.output, dim)

    # Summary table
    print("\nGenerating summary:")
    generate_summary_table(results, args.output)

    print("\nDone!")


if __name__ == '__main__':
    main()
