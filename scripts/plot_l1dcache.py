#!/usr/bin/env python3
"""Generate L1-dcache miss rate figure for SBBF paper.

Compares SBBF (with SFC sequential iteration) vs hash-based filters (BlockedBF, RegisterBF, GloBiMap)
to demonstrate the cache efficiency of SFC-based block indexing.

Usage:
    uv run scripts/plot_l1dcache.py --input sbbf_results/l1dcache/l1dcache_results.json --output sbbf_results/figures
"""

import argparse
import json
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
    'figure.figsize': (8, 5),
    'figure.dpi': 150,
    'savefig.bbox': 'tight',
    'savefig.pad_inches': 0.1,
})

# Series configuration: (filter, traversal) -> style
SERIES_CONFIG = {
    # SBBF with sequential SFC iteration (optimal - enables prefetching)
    ('SBBF-Hilbert', 'sfc_sequential'): {
        'color': '#ff7f0e',
        'marker': 's',
        'linestyle': '-',
        'linewidth': 2.5,
        'markersize': 10,
        'label': 'SBBF-Hilbert'
    },
    ('SBBF-Morton', 'sfc_sequential'): {
        'color': '#1f77b4',
        'marker': 'D',
        'linestyle': '-',
        'linewidth': 2.5,
        'markersize': 10,
        'label': 'SBBF-Morton'
    },
    # Hash-based filters (random block access)
    ('BlockedBF', 'row_major'): {
        'color': '#2ca02c',
        'marker': '^',
        'linestyle': '--',
        'linewidth': 2.5,
        'markersize': 10,
        'label': 'BlockedBF'
    },
    ('RegisterBF', 'row_major'): {
        'color': '#d62728',
        'marker': 'o',
        'linestyle': '--',
        'linewidth': 2.5,
        'markersize': 10,
        'label': 'RegisterBF'
    },
    ('GloBiMap', 'row_major'): {
        'color': '#9467bd',
        'marker': 'p',
        'linestyle': '--',
        'linewidth': 2.5,
        'markersize': 10,
        'label': 'Standard BF'
    },
    # Additional traversal patterns (for --all-series)
    ('SBBF-Hilbert', 'random'): {
        'color': '#ff7f0e',
        'marker': 's',
        'linestyle': ':',
        'linewidth': 1.5,
        'markersize': 8,
        'label': 'SBBF-Hilbert (random)'
    },
    ('BlockedBF', 'hilbert'): {
        'color': '#2ca02c',
        'marker': '^',
        'linestyle': ':',
        'linewidth': 1.5,
        'markersize': 8,
        'label': 'BlockedBF (Hilbert)'
    },
    ('BlockedBF', 'random'): {
        'color': '#2ca02c',
        'marker': 'v',
        'linestyle': ':',
        'linewidth': 1.5,
        'markersize': 8,
        'label': 'BlockedBF (random)'
    },
}


def load_results(input_path: str) -> dict:
    """Load benchmark results from JSON file."""
    with open(input_path) as f:
        return json.load(f)


def extract_series(results: list, filter_name: str, traversal: str, use_miss_rate: bool = True) -> tuple:
    """Extract data series for plotting.

    Args:
        results: List of benchmark results
        filter_name: Name of filter to extract
        traversal: Traversal pattern to extract
        use_miss_rate: If True, return miss rate; otherwise return hit rate

    Returns:
        Tuple of (grid_elements, rates) arrays
    """
    elements = []
    rates = []

    for r in results:
        if r['filter'] == filter_name and r['traversal'] == traversal:
            elements.append(r['grid_elements'])
            hit_rate = r['l1d_hit_rate_pct']
            if use_miss_rate:
                rates.append(100.0 - hit_rate)  # Convert to miss rate
            else:
                rates.append(hit_rate)

    # Sort by elements
    if elements:
        sorted_pairs = sorted(zip(elements, rates))
        elements, rates = zip(*sorted_pairs)

    return np.array(elements), np.array(rates)


def plot_miss_rate(data: dict, output_dir: Path, show_all_series: bool = False):
    """Plot L1-dcache miss rate vs grid size.

    Args:
        data: Loaded JSON data
        output_dir: Directory to save figures
        show_all_series: If True, show all traversal patterns; otherwise just main comparison
    """
    results = data.get('results', [])
    if not results:
        print("No results found in data")
        return

    fig, ax = plt.subplots(figsize=(10, 6))

    # Determine which series to plot
    if show_all_series:
        series_keys = list(SERIES_CONFIG.keys())
    else:
        # Main comparison: SBBF (SFC sequential) vs hash-based filters
        series_keys = [
            ('SBBF-Hilbert', 'sfc_sequential'),
            ('SBBF-Morton', 'sfc_sequential'),
            ('BlockedBF', 'row_major'),
            ('RegisterBF', 'row_major'),
            ('GloBiMap', 'row_major'),
        ]

    # Plot each series
    for filter_name, traversal in series_keys:
        elements, miss_rates = extract_series(results, filter_name, traversal, use_miss_rate=True)

        if len(elements) == 0:
            continue

        style = SERIES_CONFIG.get((filter_name, traversal), {
            'color': 'gray',
            'marker': 'o',
            'linestyle': '-',
            'linewidth': 2,
            'markersize': 8,
            'label': f'{filter_name} ({traversal})'
        })

        ax.plot(elements, miss_rates,
                color=style['color'],
                marker=style['marker'],
                linestyle=style['linestyle'],
                linewidth=style['linewidth'],
                markersize=style['markersize'],
                label=style['label'])

    # Configure axes
    ax.set_xlabel('Grid Elements')
    ax.set_ylabel('L1-dcache Miss Rate (%)')
    ax.set_xscale('log')

    # X-axis ticks: show actual grid sizes
    all_elements = sorted(set(r['grid_elements'] for r in results))
    ax.set_xticks(all_elements)
    ax.set_xticklabels([f"{int(e**(1/3))}³\n({e:,})" for e in all_elements])

    # Y-axis range: 0-10% to show the dramatic difference
    # Find max miss rate to set appropriate upper bound
    max_miss = max(100.0 - r['l1d_hit_rate_pct'] for r in results)
    y_max = min(15, max(10, np.ceil(max_miss * 1.1)))  # At least 10%, at most 15%
    ax.set_ylim(0, y_max)

    # Grid and legend
    ax.grid(True, alpha=0.3, linestyle='--')
    ax.legend(loc='upper left', framealpha=0.9)

    # Title
    ax.set_title('L1-dcache Miss Rate: SBBF vs Hash-Based Filters')

    # Save figures
    output_dir.mkdir(parents=True, exist_ok=True)

    suffix = '_all' if show_all_series else ''
    fig.savefig(output_dir / f'l1dcache_miss_rate{suffix}.pdf', format='pdf')
    fig.savefig(output_dir / f'l1dcache_miss_rate{suffix}.png', format='png', dpi=300)

    print(f"Saved: l1dcache_miss_rate{suffix}.pdf, l1dcache_miss_rate{suffix}.png")

    plt.close(fig)


def plot_combined(data: dict, output_dir: Path):
    """Plot both miss rate and latency in two panels."""
    results = data.get('results', [])
    if not results:
        print("No results found in data")
        return

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 5))

    # Series to plot: SBBF (SFC sequential) vs hash-based filters
    series_keys = [
        ('SBBF-Hilbert', 'sfc_sequential'),
        ('SBBF-Morton', 'sfc_sequential'),
        ('BlockedBF', 'row_major'),
        ('RegisterBF', 'row_major'),
        ('GloBiMap', 'row_major'),
    ]

    # Left panel: L1-dcache miss rate
    for filter_name, traversal in series_keys:
        elements, miss_rates = extract_series(results, filter_name, traversal, use_miss_rate=True)

        if len(elements) == 0:
            continue

        style = SERIES_CONFIG.get((filter_name, traversal), {
            'color': 'gray', 'marker': 'o', 'linestyle': '-',
            'linewidth': 2, 'markersize': 8, 'label': f'{filter_name} ({traversal})'
        })

        ax1.plot(elements, miss_rates,
                 color=style['color'], marker=style['marker'],
                 linestyle=style['linestyle'], linewidth=style['linewidth'],
                 markersize=style['markersize'], label=style['label'])

    ax1.set_xlabel('Grid Elements')
    ax1.set_ylabel('L1-dcache Miss Rate (%)')
    ax1.set_xscale('log')

    # Y-axis: 0-10% range to show dramatic difference
    max_miss = max(100.0 - r['l1d_hit_rate_pct'] for r in results)
    y_max = min(15, max(10, np.ceil(max_miss * 1.1)))
    ax1.set_ylim(0, y_max)

    all_elements = sorted(set(r['grid_elements'] for r in results))
    ax1.set_xticks(all_elements)
    ax1.set_xticklabels([f"{int(e**(1/3))}³" for e in all_elements])

    ax1.grid(True, alpha=0.3, linestyle='--')
    ax1.legend(loc='upper left', framealpha=0.9, fontsize=10)
    ax1.set_title('(a) L1-dcache Miss Rate')

    # Right panel: Query latency
    for filter_name, traversal in series_keys:
        elements = []
        latencies = []

        for r in results:
            if r['filter'] == filter_name and r['traversal'] == traversal:
                elements.append(r['grid_elements'])
                latencies.append(r['query_ns_per_op'])

        if not elements:
            continue

        sorted_pairs = sorted(zip(elements, latencies))
        elements, latencies = zip(*sorted_pairs)

        style = SERIES_CONFIG.get((filter_name, traversal), {
            'color': 'gray', 'marker': 'o', 'linestyle': '-',
            'linewidth': 2, 'markersize': 8, 'label': f'{filter_name} ({traversal})'
        })

        ax2.plot(elements, latencies,
                 color=style['color'], marker=style['marker'],
                 linestyle=style['linestyle'], linewidth=style['linewidth'],
                 markersize=style['markersize'], label=style['label'])

    ax2.set_xlabel('Grid Elements')
    ax2.set_ylabel('Query Latency (ns/op)')
    ax2.set_xscale('log')

    ax2.set_xticks(all_elements)
    ax2.set_xticklabels([f"{int(e**(1/3))}³" for e in all_elements])

    ax2.grid(True, alpha=0.3, linestyle='--')
    ax2.legend(loc='upper left', framealpha=0.9, fontsize=10)
    ax2.set_title('(b) Query Latency')

    plt.tight_layout()

    # Save figures
    output_dir.mkdir(parents=True, exist_ok=True)
    fig.savefig(output_dir / 'l1dcache_combined.pdf', format='pdf')
    fig.savefig(output_dir / 'l1dcache_combined.png', format='png', dpi=300)

    print("Saved: l1dcache_combined.pdf, l1dcache_combined.png")

    plt.close(fig)


def print_paper_values(data: dict):
    """Print values ready for the paper (using miss rate)."""
    results = data.get('results', [])

    print("\n" + "=" * 60)
    print("Paper-ready values (256³ grid) - MISS RATE (lower is better):")
    print("=" * 60)

    # Collect 256^3 results
    filter_data = {}
    for r in results:
        if r['grid_size'] == 256:
            key = (r['filter'], r['traversal'])
            miss_rate = 100.0 - r['l1d_hit_rate_pct']
            filter_data[key] = {
                'miss_rate': miss_rate,
                'latency': r['query_ns_per_op']
            }

    # Print each filter's results
    for (filter_name, traversal), data_vals in sorted(filter_data.items()):
        print(f"\n{filter_name} ({traversal}):")
        print(f"  L1-dcache miss rate: {data_vals['miss_rate']:.2f}%")
        print(f"  Query latency: {data_vals['latency']:.1f} ns/op")

    print("\n" + "-" * 60)
    print("Paper paragraph suggestion:")
    print("-" * 60)

    sbbf_data = filter_data.get(('SBBF-Hilbert', 'sfc_sequential'))
    bbf_data = filter_data.get(('BlockedBF', 'row_major'))
    reg_data = filter_data.get(('RegisterBF', 'row_major'))
    gbm_data = filter_data.get(('GloBiMap', 'row_major'))

    if sbbf_data and bbf_data:
        sbbf_miss = sbbf_data['miss_rate']
        bbf_miss = bbf_data['miss_rate']
        speedup = bbf_data['latency'] / sbbf_data['latency'] if sbbf_data['latency'] else 0
        miss_ratio = bbf_miss / sbbf_miss if sbbf_miss > 0 else float('inf')

        print(f"\n  \"In a 256³ voxel traversal, SBBF achieves a {sbbf_miss:.2f}%")
        print(f"   L1-dcache miss rate—{miss_ratio:.0f}× lower than BlockedBF's {bbf_miss:.1f}%—")
        print(f"   resulting in {speedup:.0f}× faster query latency ({sbbf_data['latency']:.1f} vs {bbf_data['latency']:.1f} ns/op).\"")

    # Summary table
    print("\n" + "-" * 60)
    print("Summary Table:")
    print("-" * 60)
    print(f"{'Filter':<20} {'Miss Rate':<12} {'Latency':<12}")
    print("-" * 44)
    for (filter_name, traversal), data_vals in sorted(filter_data.items()):
        if traversal in ('sfc_sequential', 'row_major'):
            print(f"{filter_name:<20} {data_vals['miss_rate']:>8.2f}%    {data_vals['latency']:>8.1f} ns")


def main():
    parser = argparse.ArgumentParser(
        description="Generate L1-dcache miss rate figure for SBBF paper"
    )
    parser.add_argument(
        '--input', '-i',
        required=True,
        help='Path to l1dcache_results.json'
    )
    parser.add_argument(
        '--output', '-o',
        default='sbbf_results/figures',
        help='Output directory for figures'
    )
    parser.add_argument(
        '--all-series',
        action='store_true',
        help='Show all traversal patterns (not just main comparison)'
    )
    parser.add_argument(
        '--combined',
        action='store_true',
        help='Generate combined miss-rate + latency figure'
    )

    args = parser.parse_args()

    # Load data
    print(f"Loading results from: {args.input}")
    data = load_results(args.input)

    output_dir = Path(args.output)

    # Generate figures
    plot_miss_rate(data, output_dir, show_all_series=args.all_series)

    if args.combined:
        plot_combined(data, output_dir)

    # Print paper values
    print_paper_values(data)


if __name__ == '__main__':
    main()
