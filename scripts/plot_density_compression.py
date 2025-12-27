#!/usr/bin/env python3
"""Generate density vs compression figures for SBBF paper.

Creates figures showing SBBF memory/performance compared to:
- Dense voxel grid (bitset): 1 bit per cell, O(1) query
- Sparse coordinate list (COO): 64 bits per point, O(log n) query

Figures:
1. Memory ratio vs fill rate
2. Bits per element vs fill rate
3. Query performance vs fill rate
4. Combined 1x3 figure for paper

Usage:
    uv run scripts/plot_density_compression.py
"""

import argparse
import json
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

# Style configuration for academic papers (2x font sizes for readability)
plt.rcParams.update({
    'font.size': 28,
    'axes.labelsize': 32,
    'axes.titlesize': 36,
    'legend.fontsize': 22,
    'xtick.labelsize': 24,
    'ytick.labelsize': 24,
    'figure.figsize': (6, 4),
    'figure.dpi': 150,
    'savefig.bbox': 'tight',
    'savefig.pad_inches': 0.1,
})

# Color scheme
COLORS = {
    'sbbf': '#1f77b4',       # Blue - SBBF
    'dense': '#2ca02c',      # Green - Dense grid
    'sparse': '#ff7f0e',     # Orange - Sparse/COO
}

MARKERS = {
    'sbbf': 'o',
    'dense': '^',
    'sparse': 's',
}

LINESTYLES = {
    'vs_dense': '-',
    'vs_sparse': '--',
}


def load_results(input_path: str) -> dict:
    """Load benchmark results from JSON file."""
    path = Path(input_path)
    if not path.exists():
        raise FileNotFoundError(f"Results not found: {path}")
    with open(path) as f:
        return json.load(f)


def extract_metrics(results: dict) -> dict:
    """Extract metric arrays from results."""
    data = {
        'fill_rate': [],
        'num_elements': [],
        'sbbf_memory_bytes': [],
        'dense_memory_bytes': [],
        'sparse_memory_bytes': [],
        'memory_ratio_vs_dense': [],
        'memory_ratio_vs_sparse': [],
        'sbbf_bits_per_element': [],
        'sparse_bits_per_element': [],
        'dense_bits_per_element': [],
        'sbbf_query_ns': [],
        'dense_query_ns': [],
        'sparse_query_ns': [],
        'query_ratio_vs_dense': [],
        'query_ratio_vs_sparse': [],
        'fpr': [],
    }

    for entry in results.get('results', []):
        config = entry.get('config', {})
        metrics = entry.get('metrics', {})

        data['fill_rate'].append(config.get('fill_rate', 0))
        data['num_elements'].append(config.get('num_elements', 0))

        for key in list(data.keys())[2:]:
            data[key].append(metrics.get(key, 0))

    # Convert to numpy arrays
    for key in data:
        data[key] = np.array(data[key])

    return data


def plot_memory_ratio(data: dict, output_dir: Path):
    """Plot memory ratio vs fill rate."""
    fig, ax = plt.subplots(figsize=(10, 7))

    fill_pct = data['fill_rate'] * 100

    ax.loglog(fill_pct, data['memory_ratio_vs_dense'],
              'o-', color=COLORS['dense'], label='SBBF / Dense Grid',
              markersize=6, linewidth=2)
    ax.loglog(fill_pct, data['memory_ratio_vs_sparse'],
              's--', color=COLORS['sparse'], label='SBBF / Coord List',
              markersize=6, linewidth=2)

    # Parity line
    ax.axhline(y=1.0, color='gray', linestyle=':', alpha=0.7, linewidth=1.5)

    # Find crossover points
    crossover_sparse = None
    for i in range(len(data['memory_ratio_vs_sparse']) - 1):
        if (data['memory_ratio_vs_sparse'][i] > 1 and
            data['memory_ratio_vs_sparse'][i+1] < 1):
            crossover_sparse = fill_pct[i]
            break
        elif (data['memory_ratio_vs_sparse'][i] < 1 and
              data['memory_ratio_vs_sparse'][i+1] > 1):
            crossover_sparse = fill_pct[i+1]
            break

    if crossover_sparse:
        ax.axvline(x=crossover_sparse, color=COLORS['sparse'],
                   linestyle=':', alpha=0.5)
        ax.annotate(f'{crossover_sparse:.2f}%',
                    xy=(crossover_sparse, 1.0),
                    xytext=(crossover_sparse * 2, 0.3),
                    fontsize=20, color=COLORS['sparse'],
                    arrowprops=dict(arrowstyle='->', color=COLORS['sparse'],
                                    alpha=0.7))

    ax.set_xlabel('Fill Rate (%)')
    ax.set_ylabel('Memory Ratio')
    ax.set_title('Memory Compression vs Density')
    ax.legend(loc='best')
    ax.grid(True, alpha=0.3, which='both')
    ax.set_xlim(fill_pct.min() * 0.5, fill_pct.max() * 2)

    # Add region labels
    ax.text(0.003, 0.02, 'SBBF smaller\nthan dense', fontsize=18,
            color=COLORS['dense'], alpha=0.8)

    plt.tight_layout()

    for ext in ['pdf', 'png']:
        filepath = output_dir / f"density_compression_memory_3d.{ext}"
        fig.savefig(filepath)
        print(f"Saved: {filepath}")
    plt.close(fig)


def plot_bits_per_element(data: dict, output_dir: Path):
    """Plot bits per element vs fill rate."""
    fig, ax = plt.subplots(figsize=(10, 7))

    fill_pct = data['fill_rate'] * 100

    ax.semilogx(fill_pct, data['sbbf_bits_per_element'],
                'o-', color=COLORS['sbbf'], label='SBBF (~10 bpe)',
                markersize=6, linewidth=2)
    ax.semilogx(fill_pct, data['sparse_bits_per_element'],
                's--', color=COLORS['sparse'], label='Coord List (64 bpe)',
                markersize=6, linewidth=2)
    ax.semilogx(fill_pct, data['dense_bits_per_element'],
                '^:', color=COLORS['dense'], label='Dense Grid (1/fill)',
                markersize=6, linewidth=2)

    ax.set_xlabel('Fill Rate (%)')
    ax.set_ylabel('Bits per Element')
    ax.set_title('Storage Efficiency vs Density')
    ax.legend(loc='best')
    ax.grid(True, alpha=0.3, which='both')
    ax.set_xlim(fill_pct.min() * 0.5, fill_pct.max() * 2)
    ax.set_ylim(0, max(data['dense_bits_per_element'].max() * 1.1, 200))

    plt.tight_layout()

    for ext in ['pdf', 'png']:
        filepath = output_dir / f"density_compression_bpe_3d.{ext}"
        fig.savefig(filepath)
        print(f"Saved: {filepath}")
    plt.close(fig)


def plot_query_performance(data: dict, output_dir: Path):
    """Plot query performance ratio vs fill rate."""
    fig, ax = plt.subplots(figsize=(10, 7))

    fill_pct = data['fill_rate'] * 100

    ax.semilogx(fill_pct, data['query_ratio_vs_dense'],
                'o-', color=COLORS['dense'], label='SBBF / Dense Grid',
                markersize=6, linewidth=2)
    ax.semilogx(fill_pct, data['query_ratio_vs_sparse'],
                's--', color=COLORS['sparse'], label='SBBF / Coord List',
                markersize=6, linewidth=2)

    # Parity line
    ax.axhline(y=1.0, color='gray', linestyle=':', alpha=0.7, linewidth=1.5)

    ax.set_xlabel('Fill Rate (%)')
    ax.set_ylabel('Query Time Ratio')
    ax.set_title('Query Performance vs Density')
    ax.legend(loc='best')
    ax.grid(True, alpha=0.3, which='both')
    ax.set_xlim(fill_pct.min() * 0.5, fill_pct.max() * 2)

    # Add interpretation labels
    ax.text(0.003, 0.3, 'SBBF faster', fontsize=18, color='gray', alpha=0.8)
    ax.text(0.003, 3, 'SBBF slower', fontsize=18, color='gray', alpha=0.8)

    plt.tight_layout()

    for ext in ['pdf', 'png']:
        filepath = output_dir / f"density_compression_query_3d.{ext}"
        fig.savefig(filepath)
        print(f"Saved: {filepath}")
    plt.close(fig)


def plot_combined(data: dict, output_dir: Path):
    """Plot combined 1x3 figure for paper."""
    fig, axes = plt.subplots(1, 3, figsize=(24, 7))

    fill_pct = data['fill_rate'] * 100

    # Panel (a): Memory ratio
    ax = axes[0]
    ax.loglog(fill_pct, data['memory_ratio_vs_dense'],
              'o-', color=COLORS['dense'], label='SBBF / Dense',
              markersize=5, linewidth=1.5)
    ax.loglog(fill_pct, data['memory_ratio_vs_sparse'],
              's--', color=COLORS['sparse'], label='SBBF / Sparse',
              markersize=5, linewidth=1.5)
    ax.axhline(y=1.0, color='gray', linestyle=':', alpha=0.7, linewidth=1)
    ax.set_xlabel('Fill Rate (%)')
    ax.set_ylabel('Memory Ratio')
    ax.set_title('(a) Memory Compression')
    ax.legend(loc='lower right', fontsize=18)
    ax.grid(True, alpha=0.3, which='both')
    ax.set_xlim(fill_pct.min() * 0.5, fill_pct.max() * 2)

    # Panel (b): Bits per element
    ax = axes[1]
    ax.semilogx(fill_pct, data['sbbf_bits_per_element'],
                'o-', color=COLORS['sbbf'], label='SBBF',
                markersize=5, linewidth=1.5)
    ax.semilogx(fill_pct, data['sparse_bits_per_element'],
                's--', color=COLORS['sparse'], label='Coord List',
                markersize=5, linewidth=1.5)
    ax.semilogx(fill_pct, data['dense_bits_per_element'],
                '^:', color=COLORS['dense'], label='Dense Grid',
                markersize=5, linewidth=1.5)
    ax.set_xlabel('Fill Rate (%)')
    ax.set_ylabel('Bits per Element')
    ax.set_title('(b) Storage Efficiency')
    ax.legend(loc='upper right', fontsize=18)
    ax.grid(True, alpha=0.3, which='both')
    ax.set_xlim(fill_pct.min() * 0.5, fill_pct.max() * 2)
    ax.set_ylim(0, min(data['dense_bits_per_element'].max() * 0.5, 500))

    # Panel (c): Query time ratio
    ax = axes[2]
    ax.semilogx(fill_pct, data['query_ratio_vs_dense'],
                'o-', color=COLORS['dense'], label='SBBF / Dense',
                markersize=5, linewidth=1.5)
    ax.semilogx(fill_pct, data['query_ratio_vs_sparse'],
                's--', color=COLORS['sparse'], label='SBBF / Sparse',
                markersize=5, linewidth=1.5)
    ax.axhline(y=1.0, color='gray', linestyle=':', alpha=0.7, linewidth=1)
    ax.set_xlabel('Fill Rate (%)')
    ax.set_ylabel('Query Time Ratio')
    ax.set_title('(c) Query Performance')
    ax.legend(loc='best', fontsize=18)
    ax.grid(True, alpha=0.3, which='both')
    ax.set_xlim(fill_pct.min() * 0.5, fill_pct.max() * 2)

    plt.tight_layout()

    for ext in ['pdf', 'png']:
        filepath = output_dir / f"density_compression_combined.{ext}"
        fig.savefig(filepath)
        print(f"Saved: {filepath}")
    plt.close(fig)


def print_summary(data: dict, results: dict):
    """Print summary statistics."""
    config = results.get('config', {})

    print("\n" + "=" * 60)
    print("Density Compression Analysis Summary")
    print("=" * 60)
    print(f"Grid: {config.get('grid_dim', 256)}^3 "
          f"({config.get('grid_dim', 256)**3:,} cells)")
    print(f"SBBF: {config.get('sbbf_config', {}).get('sfc_type', 'N/A')}, "
          f"k={config.get('hash_k', 'N/A')}")
    print()

    # Find key statistics
    dense_mem = data['dense_memory_bytes'][0] / 1024  # KB
    print(f"Dense grid memory: {dense_mem:.1f} KB (constant)")

    # SBBF vs Dense
    min_ratio_dense = data['memory_ratio_vs_dense'].min()
    max_ratio_dense = data['memory_ratio_vs_dense'].max()
    print(f"SBBF/Dense ratio: {min_ratio_dense:.4f}x to {max_ratio_dense:.2f}x")

    # SBBF vs Sparse crossover
    crossover_idx = None
    for i in range(len(data['memory_ratio_vs_sparse']) - 1):
        if (data['memory_ratio_vs_sparse'][i] > 1 and
            data['memory_ratio_vs_sparse'][i+1] < 1):
            crossover_idx = i
            break

    if crossover_idx:
        crossover_fill = data['fill_rate'][crossover_idx] * 100
        print(f"SBBF beats Sparse at: ~{crossover_fill:.3f}% fill rate")

    # Query performance
    avg_query_ratio_dense = data['query_ratio_vs_dense'].mean()
    avg_query_ratio_sparse = data['query_ratio_vs_sparse'].mean()
    print(f"Avg query time vs Dense: {avg_query_ratio_dense:.2f}x")
    print(f"Avg query time vs Sparse: {avg_query_ratio_sparse:.2f}x")

    # FPR
    max_fpr = data['fpr'].max() * 100
    print(f"Max FPR: {max_fpr:.2f}%")

    print("=" * 60)


def main():
    parser = argparse.ArgumentParser(
        description='Generate density vs compression figures for SBBF paper')
    parser.add_argument('--input', '-i', type=str,
                        default='./sbbf_results/density/uniform_3d.json',
                        help='Input JSON file path')
    parser.add_argument('--output', '-o', type=str,
                        default='./sbbf_results/figures',
                        help='Output directory for figures')
    args = parser.parse_args()

    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)

    print(f"Loading results from: {args.input}")
    results = load_results(args.input)
    data = extract_metrics(results)

    print(f"Generating figures in: {output_dir}")

    # Generate individual figures
    plot_memory_ratio(data, output_dir)
    plot_bits_per_element(data, output_dir)
    plot_query_performance(data, output_dir)

    # Generate combined figure for paper
    plot_combined(data, output_dir)

    # Print summary
    print_summary(data, results)

    print("\nDone!")


if __name__ == '__main__':
    main()
