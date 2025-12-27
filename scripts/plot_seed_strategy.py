#!/usr/bin/env python3
"""Generate seed strategy comparison figures for SBBF paper.

Creates figures comparing XOR vs MULTIPLY_SHIFT seed strategies across:
- Synthetic 2D/3D
- GDELT 2D
- Voxelized meshes

Usage:
    uv run scripts/plot_seed_strategy.py
"""

import argparse
import json
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

# Style configuration for academic papers
plt.rcParams.update({
    'font.size': 14,
    'axes.labelsize': 16,
    'axes.titlesize': 18,
    'legend.fontsize': 10,
    'xtick.labelsize': 12,
    'ytick.labelsize': 12,
    'figure.figsize': (10, 4),
    'figure.dpi': 150,
    'savefig.bbox': 'tight',
    'savefig.pad_inches': 0.1,
})

# Color scheme: SFC type determines color
COLORS = {
    'MORTON': '#1f77b4',    # Blue
    'HILBERT': '#ff7f0e',   # Orange
}

# Hatching: seed strategy
HATCHES = {
    'XOR': '',
    'MULTIPLY_SHIFT': '///',
}

# Linestyles
LINESTYLES = {
    'XOR': '-',
    'MULTIPLY_SHIFT': '--',
}


def load_json(path: Path) -> dict | None:
    """Load JSON file if it exists."""
    if not path.exists():
        return None
    with open(path) as f:
        return json.load(f)


def extract_fpr_data(results: list) -> dict:
    """Extract FPR data organized by configuration."""
    data = {}
    for entry in results:
        config = entry.get('config', {})
        metrics = entry.get('metrics', {})

        sfc = config.get('sfc_type', '')
        intra = config.get('intra_strategy', '')
        seed = config.get('seed_strategy', '')

        key = (sfc, intra, seed)
        data[key] = {
            'fpr': metrics.get('fpr', 0),
            'insert_ns': metrics.get('insert_ns', 0),
            'query_ns': metrics.get('query_ns', 0),
        }
    return data


def plot_fpr_bars(ax, data: dict, title: str):
    """Plot FPR comparison as grouped bar chart."""
    # Group by (SFC, intra) pairs
    groups = {}
    for (sfc, intra, seed), metrics in data.items():
        key = (sfc, intra)
        if key not in groups:
            groups[key] = {}
        groups[key][seed] = metrics['fpr']

    # Sort groups
    group_keys = sorted(groups.keys())
    n_groups = len(group_keys)

    if n_groups == 0:
        ax.text(0.5, 0.5, 'No data', ha='center', va='center', transform=ax.transAxes)
        return

    x = np.arange(n_groups)
    width = 0.35

    # Plot bars for each seed strategy
    xor_fprs = []
    ms_fprs = []
    for key in group_keys:
        xor_fprs.append(groups[key].get('XOR', 0) * 100)
        ms_fprs.append(groups[key].get('MULTIPLY_SHIFT', 0) * 100)

    bars1 = ax.bar(x - width/2, xor_fprs, width, label='XOR',
                   color='#2ca02c', edgecolor='black', linewidth=0.5)
    bars2 = ax.bar(x + width/2, ms_fprs, width, label='MULTIPLY_SHIFT',
                   color='#d62728', hatch='///', edgecolor='black', linewidth=0.5)

    # Labels
    labels = []
    for sfc, intra in group_keys:
        sfc_short = sfc.replace('_2D', '').replace('_3D', '').title()
        intra_short = 'DH' if intra == 'double_hash' else 'PL'
        labels.append(f'{sfc_short}\n{intra_short}')

    ax.set_xticks(x)
    ax.set_xticklabels(labels, fontsize=10)
    ax.set_ylabel('FPR (%)')
    ax.set_title(title)
    ax.legend(loc='upper right', fontsize=9)
    ax.grid(True, alpha=0.3, axis='y')

    # Add value labels on bars
    for bar in bars1:
        height = bar.get_height()
        if height > 0:
            ax.annotate(f'{height:.2f}',
                       xy=(bar.get_x() + bar.get_width()/2, height),
                       xytext=(0, 2), textcoords='offset points',
                       ha='center', va='bottom', fontsize=8)
    for bar in bars2:
        height = bar.get_height()
        if height > 0:
            ax.annotate(f'{height:.2f}',
                       xy=(bar.get_x() + bar.get_width()/2, height),
                       xytext=(0, 2), textcoords='offset points',
                       ha='center', va='bottom', fontsize=8)


def plot_overview(synthetic_2d: dict, synthetic_3d: dict, clustered_2d: dict, clustered_3d: dict,
                  gdelt: dict, voxel: dict, output_dir: Path):
    """Create 2x3 overview figure with uniform, clustered, and real-world data."""
    fig, axes = plt.subplots(2, 3, figsize=(18, 12))

    # Row 0: Uniform 2D, Clustered 2D, GDELT 2D
    # Row 1: Uniform 3D, Clustered 3D, Voxel 3D

    # Uniform 2D
    if synthetic_2d:
        data = extract_fpr_data(synthetic_2d.get('results', []))
        plot_fpr_bars(axes[0, 0], data, 'Uniform 2D')
    else:
        axes[0, 0].text(0.5, 0.5, 'No data', ha='center', va='center', transform=axes[0, 0].transAxes)
        axes[0, 0].set_title('Uniform 2D')

    # Clustered 2D
    if clustered_2d:
        data = extract_fpr_data(clustered_2d.get('results', []))
        plot_fpr_bars(axes[0, 1], data, 'Clustered 2D')
    else:
        axes[0, 1].text(0.5, 0.5, 'No data', ha='center', va='center', transform=axes[0, 1].transAxes)
        axes[0, 1].set_title('Clustered 2D')

    # GDELT 2D
    if gdelt:
        data = extract_fpr_data(gdelt.get('results', []))
        plot_fpr_bars(axes[0, 2], data, 'GDELT 2D (1.9M events)')
    else:
        axes[0, 2].text(0.5, 0.5, 'No data', ha='center', va='center', transform=axes[0, 2].transAxes)
        axes[0, 2].set_title('GDELT 2D')

    # Uniform 3D
    if synthetic_3d:
        data = extract_fpr_data(synthetic_3d.get('results', []))
        plot_fpr_bars(axes[1, 0], data, 'Uniform 3D')
    else:
        axes[1, 0].text(0.5, 0.5, 'No data', ha='center', va='center', transform=axes[1, 0].transAxes)
        axes[1, 0].set_title('Uniform 3D')

    # Clustered 3D
    if clustered_3d:
        data = extract_fpr_data(clustered_3d.get('results', []))
        plot_fpr_bars(axes[1, 1], data, 'Clustered 3D')
    else:
        axes[1, 1].text(0.5, 0.5, 'No data', ha='center', va='center', transform=axes[1, 1].transAxes)
        axes[1, 1].set_title('Clustered 3D')

    # Voxel 3D (bunny only for overview)
    if voxel:
        results = voxel.get('results', [])
        bunny_results = [r for r in results if r.get('config', {}).get('mesh') == 'bunny']
        data = extract_fpr_data(bunny_results)
        plot_fpr_bars(axes[1, 2], data, 'Voxel 3D (Stanford Bunny)')
    else:
        axes[1, 2].text(0.5, 0.5, 'No data', ha='center', va='center', transform=axes[1, 2].transAxes)
        axes[1, 2].set_title('Voxel 3D')

    plt.tight_layout()

    for ext in ['pdf', 'png']:
        filepath = output_dir / f'seed_strategy_overview.{ext}'
        fig.savefig(filepath, dpi=150 if ext == 'png' else None)
        print(f'Saved: {filepath}')

    plt.close()


def plot_single_dataset(data: dict, title: str, filename: str, output_dir: Path):
    """Plot detailed comparison for a single dataset."""
    if not data:
        print(f'Skipping {filename}: no data')
        return

    fig, ax = plt.subplots(figsize=(10, 6))

    results = extract_fpr_data(data.get('results', []))
    plot_fpr_bars(ax, results, title)

    plt.tight_layout()

    for ext in ['pdf', 'png']:
        filepath = output_dir / f'{filename}.{ext}'
        fig.savefig(filepath, dpi=150 if ext == 'png' else None)
        print(f'Saved: {filepath}')

    plt.close()


def plot_voxel_all_meshes(voxel: dict, output_dir: Path):
    """Plot comparison across all voxel meshes."""
    if not voxel:
        print('Skipping voxel_all: no data')
        return

    results = voxel.get('results', [])
    meshes = sorted(set(r.get('config', {}).get('mesh', '') for r in results))

    if not meshes:
        return

    n_meshes = len(meshes)
    fig, axes = plt.subplots(1, n_meshes, figsize=(4 * n_meshes, 5))

    if n_meshes == 1:
        axes = [axes]

    for ax, mesh in zip(axes, meshes):
        mesh_results = [r for r in results if r.get('config', {}).get('mesh') == mesh]
        data = extract_fpr_data(mesh_results)
        plot_fpr_bars(ax, data, mesh.title())

    plt.tight_layout()

    for ext in ['pdf', 'png']:
        filepath = output_dir / f'seed_strategy_voxel_all.{ext}'
        fig.savefig(filepath, dpi=150 if ext == 'png' else None)
        print(f'Saved: {filepath}')

    plt.close()


def plot_latency_comparison(synthetic_2d: dict, synthetic_3d: dict, clustered_2d: dict, clustered_3d: dict,
                            gdelt: dict, voxel: dict, output_dir: Path):
    """Plot latency comparison (insert/query ns)."""
    fig, axes = plt.subplots(2, 3, figsize=(18, 12))

    datasets = [
        (synthetic_2d, 'Uniform 2D', axes[0, 0]),
        (clustered_2d, 'Clustered 2D', axes[0, 1]),
        (gdelt, 'GDELT 2D', axes[0, 2]),
        (synthetic_3d, 'Uniform 3D', axes[1, 0]),
        (clustered_3d, 'Clustered 3D', axes[1, 1]),
        (voxel, 'Voxel 3D (bunny)', axes[1, 2]),
    ]

    for data, title, ax in datasets:
        if not data:
            ax.text(0.5, 0.5, 'No data', ha='center', va='center', transform=ax.transAxes)
            ax.set_title(title)
            continue

        results = data.get('results', [])

        # For voxel, only use bunny
        if 'voxel' in title.lower():
            results = [r for r in results if r.get('config', {}).get('mesh') == 'bunny']

        metrics_data = extract_fpr_data(results)

        # Group by (SFC, intra)
        groups = {}
        for (sfc, intra, seed), metrics in metrics_data.items():
            key = (sfc, intra)
            if key not in groups:
                groups[key] = {}
            groups[key][seed] = (metrics['insert_ns'], metrics['query_ns'])

        group_keys = sorted(groups.keys())
        n_groups = len(group_keys)

        if n_groups == 0:
            ax.text(0.5, 0.5, 'No data', ha='center', va='center', transform=ax.transAxes)
            ax.set_title(title)
            continue

        x = np.arange(n_groups)
        width = 0.2

        # Insert latency
        xor_insert = [groups[k].get('XOR', (0, 0))[0] for k in group_keys]
        ms_insert = [groups[k].get('MULTIPLY_SHIFT', (0, 0))[0] for k in group_keys]

        # Query latency
        xor_query = [groups[k].get('XOR', (0, 0))[1] for k in group_keys]
        ms_query = [groups[k].get('MULTIPLY_SHIFT', (0, 0))[1] for k in group_keys]

        ax.bar(x - 1.5*width, xor_insert, width, label='XOR Insert', color='#1f77b4')
        ax.bar(x - 0.5*width, xor_query, width, label='XOR Query', color='#1f77b4', alpha=0.6)
        ax.bar(x + 0.5*width, ms_insert, width, label='MS Insert', color='#ff7f0e', hatch='///')
        ax.bar(x + 1.5*width, ms_query, width, label='MS Query', color='#ff7f0e', alpha=0.6, hatch='///')

        labels = []
        for sfc, intra in group_keys:
            sfc_short = sfc.replace('_2D', '').replace('_3D', '').title()
            intra_short = 'DH' if intra == 'double_hash' else 'PL'
            labels.append(f'{sfc_short}\n{intra_short}')

        ax.set_xticks(x)
        ax.set_xticklabels(labels, fontsize=10)
        ax.set_ylabel('Latency (ns/op)')
        ax.set_title(title)
        ax.legend(loc='upper right', fontsize=8, ncol=2)
        ax.grid(True, alpha=0.3, axis='y')

    plt.tight_layout()

    for ext in ['pdf', 'png']:
        filepath = output_dir / f'seed_strategy_latency.{ext}'
        fig.savefig(filepath, dpi=150 if ext == 'png' else None)
        print(f'Saved: {filepath}')

    plt.close()


def print_summary_table(synthetic_2d: dict, synthetic_3d: dict, clustered_2d: dict, clustered_3d: dict,
                        gdelt: dict, voxel: dict):
    """Print summary table of results."""
    print("\n" + "=" * 80)
    print("SUMMARY: XOR vs MULTIPLY_SHIFT Seed Strategy Comparison")
    print("=" * 80)

    datasets = [
        ('Uniform 2D', synthetic_2d),
        ('Uniform 3D', synthetic_3d),
        ('Clustered 2D', clustered_2d),
        ('Clustered 3D', clustered_3d),
        ('GDELT 2D', gdelt),
    ]

    for name, data in datasets:
        if not data:
            continue

        print(f"\n{name}:")
        print("-" * 60)
        print(f"{'Configuration':<30} {'XOR FPR':<15} {'MS FPR':<15} {'Winner':<10}")
        print("-" * 60)

        results = extract_fpr_data(data.get('results', []))

        # Group by (SFC, intra)
        groups = {}
        for (sfc, intra, seed), metrics in results.items():
            key = (sfc, intra)
            if key not in groups:
                groups[key] = {}
            groups[key][seed] = metrics['fpr']

        for (sfc, intra), seeds in sorted(groups.items()):
            xor_fpr = seeds.get('XOR', 0)
            ms_fpr = seeds.get('MULTIPLY_SHIFT', 0)
            winner = 'XOR' if xor_fpr <= ms_fpr else 'MS'
            sfc_short = sfc.replace('_2D', '').replace('_3D', '')
            config = f"{sfc_short} + {intra}"
            print(f"{config:<30} {xor_fpr*100:>12.4f}% {ms_fpr*100:>12.4f}% {winner:<10}")

    if voxel:
        print(f"\nVoxel 3D (by mesh):")
        print("-" * 60)
        print(f"{'Mesh':<15} {'Configuration':<25} {'XOR FPR':<12} {'MS FPR':<12} {'Winner':<8}")
        print("-" * 60)

        results = voxel.get('results', [])
        meshes = sorted(set(r.get('config', {}).get('mesh', '') for r in results))

        for mesh in meshes:
            mesh_results = [r for r in results if r.get('config', {}).get('mesh') == mesh]
            data = extract_fpr_data(mesh_results)

            groups = {}
            for (sfc, intra, seed), metrics in data.items():
                key = (sfc, intra)
                if key not in groups:
                    groups[key] = {}
                groups[key][seed] = metrics['fpr']

            for (sfc, intra), seeds in sorted(groups.items()):
                xor_fpr = seeds.get('XOR', 0)
                ms_fpr = seeds.get('MULTIPLY_SHIFT', 0)
                winner = 'XOR' if xor_fpr <= ms_fpr else 'MS'
                sfc_short = sfc.replace('_3D', '')
                config = f"{sfc_short}+{intra[:2]}"
                print(f"{mesh:<15} {config:<25} {xor_fpr*100:>9.4f}% {ms_fpr*100:>9.4f}% {winner:<8}")

    print("=" * 80)


def main():
    parser = argparse.ArgumentParser(description='Generate seed strategy comparison figures')
    parser.add_argument('--input', '-i', default='sbbf_results/seed_strategy',
                        help='Input directory with JSON results')
    parser.add_argument('--output', '-o', default='sbbf_results/figures',
                        help='Output directory for figures')
    args = parser.parse_args()

    input_dir = Path(args.input)
    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)

    print("=" * 60)
    print("Generating Seed Strategy Comparison Figures")
    print("=" * 60)
    print(f"Input: {input_dir}")
    print(f"Output: {output_dir}")

    # Load all datasets
    synthetic_2d = load_json(input_dir / 'synthetic_2d.json')
    synthetic_3d = load_json(input_dir / 'synthetic_3d.json')
    clustered_2d = load_json(input_dir / 'clustered_2d.json')
    clustered_3d = load_json(input_dir / 'clustered_3d.json')
    gdelt = load_json(input_dir / 'gdelt_2d.json')
    voxel = load_json(input_dir / 'voxel_3d.json')

    print(f"\nLoaded datasets:")
    print(f"  Synthetic 2D: {'Yes' if synthetic_2d else 'No'}")
    print(f"  Synthetic 3D: {'Yes' if synthetic_3d else 'No'}")
    print(f"  Clustered 2D: {'Yes' if clustered_2d else 'No'}")
    print(f"  Clustered 3D: {'Yes' if clustered_3d else 'No'}")
    print(f"  GDELT 2D: {'Yes' if gdelt else 'No'}")
    print(f"  Voxel 3D: {'Yes' if voxel else 'No'}")

    # Generate figures
    print("\nGenerating figures...")

    # Compact 2x3 overview
    plot_overview(synthetic_2d, synthetic_3d, clustered_2d, clustered_3d, gdelt, voxel, output_dir)

    # Separate per-dataset figures
    plot_single_dataset(synthetic_2d, 'Seed Strategy: Uniform 2D (100K points)',
                        'seed_strategy_synthetic_2d', output_dir)
    plot_single_dataset(synthetic_3d, 'Seed Strategy: Uniform 3D (100K points)',
                        'seed_strategy_synthetic_3d', output_dir)
    plot_single_dataset(clustered_2d, 'Seed Strategy: Clustered 2D (100K points, 50 clusters)',
                        'seed_strategy_clustered_2d', output_dir)
    plot_single_dataset(clustered_3d, 'Seed Strategy: Clustered 3D (100K points, 50 clusters)',
                        'seed_strategy_clustered_3d', output_dir)
    plot_single_dataset(gdelt, 'Seed Strategy: GDELT 2D (1.9M events)',
                        'seed_strategy_gdelt', output_dir)

    # All voxel meshes
    plot_voxel_all_meshes(voxel, output_dir)

    # Latency comparison
    plot_latency_comparison(synthetic_2d, synthetic_3d, clustered_2d, clustered_3d, gdelt, voxel, output_dir)

    # Print summary table
    print_summary_table(synthetic_2d, synthetic_3d, clustered_2d, clustered_3d, gdelt, voxel)

    print("\n" + "=" * 60)
    print("Done!")
    print("=" * 60)


if __name__ == '__main__':
    main()
