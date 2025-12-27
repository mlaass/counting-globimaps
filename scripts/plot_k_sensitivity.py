#!/usr/bin/env python3
"""Generate k-sensitivity figures for SBBF paper.

Creates two figures:
1. Synthetic data: 2D and 3D subplots showing FPR vs k
2. Voxel data: bunny and teapot subplots showing FPR vs k

Usage:
    uv run scripts/plot_k_sensitivity.py
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

# Color scheme: SFC type determines color, strategy determines linestyle
COLORS = {
    'MORTON': '#1f77b4',    # Blue
    'HILBERT': '#ff7f0e',   # Orange
}

MARKERS = {
    'MORTON': 'o',
    'HILBERT': 's',
}

# Linestyle: seed strategy
LINESTYLES = {
    'XOR': '-',
    'MULTIPLY_SHIFT': '--',
}

# For intra-block strategy differentiation (when both are shown)
INTRA_MARKERS = {
    'double_hash': 'o',
    'pattern_lookup': '^',
}


def load_synthetic_results(input_dir: str) -> dict:
    """Load synthetic benchmark results."""
    path = Path(input_dir) / "synthetic.json"
    if not path.exists():
        raise FileNotFoundError(f"Synthetic results not found: {path}")
    with open(path) as f:
        return json.load(f)


def load_voxel_results(input_dir: str) -> dict:
    """Load voxel benchmark results."""
    path = Path(input_dir) / "voxel.json"
    if not path.exists():
        raise FileNotFoundError(f"Voxel results not found: {path}")
    with open(path) as f:
        return json.load(f)


def extract_series(results: list, sfc_type: str, dim: int,
                   intra_strategy: str, seed_strategy: str) -> tuple:
    """Extract k values and FPR for a specific configuration."""
    k_values = []
    fpr_values = []

    for entry in results:
        config = entry.get('config', {})
        metrics = entry.get('metrics', {})

        entry_sfc = config.get('sfc_type', '')
        entry_dim = config.get('dimensions', 2)
        entry_intra = config.get('intra_strategy', '')
        entry_seed = config.get('seed_strategy', '')

        # Match configuration
        if sfc_type not in entry_sfc:
            continue
        if entry_dim != dim:
            continue
        if intra_strategy != entry_intra:
            continue
        if seed_strategy != entry_seed:
            continue

        k = config.get('k', 0)
        fpr = metrics.get('fpr', None)

        if k > 0 and fpr is not None:
            k_values.append(k)
            fpr_values.append(fpr)

    # Sort by k
    if k_values:
        sorted_pairs = sorted(zip(k_values, fpr_values))
        k_values, fpr_values = zip(*sorted_pairs)

    return np.array(k_values), np.array(fpr_values)


def extract_voxel_series(results: list, mesh: str, intra_strategy: str,
                         seed_strategy: str) -> tuple:
    """Extract k values and FPR for voxel data."""
    k_values = []
    fpr_values = []

    for entry in results:
        config = entry.get('config', {})
        metrics = entry.get('metrics', {})

        entry_mesh = config.get('mesh', '')
        entry_intra = config.get('intra_strategy', '')
        entry_seed = config.get('seed_strategy', '')

        if entry_mesh != mesh:
            continue
        if intra_strategy != entry_intra:
            continue
        if seed_strategy != entry_seed:
            continue

        k = config.get('k', 0)
        fpr = metrics.get('fpr', None)

        if k > 0 and fpr is not None:
            k_values.append(k)
            fpr_values.append(fpr)

    if k_values:
        sorted_pairs = sorted(zip(k_values, fpr_values))
        k_values, fpr_values = zip(*sorted_pairs)

    return np.array(k_values), np.array(fpr_values)


def plot_synthetic(results: dict, output_dir: str, intra_strategy: str = 'double_hash'):
    """Plot synthetic data FPR vs k (2D and 3D subplots)."""
    fig, axes = plt.subplots(1, 2, figsize=(10, 4))

    dims_titles = [(2, '2D Synthetic Data', axes[0]), (3, '3D Synthetic Data', axes[1])]
    data = results.get('results', [])

    for dim, title, ax in dims_titles:
        has_data = False

        for sfc in ['MORTON', 'HILBERT']:
            for seed in ['XOR', 'MULTIPLY_SHIFT']:
                sfc_full = f"{sfc}_{dim}D"
                k_vals, fpr_vals = extract_series(data, sfc_full, dim, intra_strategy, seed)

                if len(k_vals) > 0:
                    has_data = True
                    label = f"{sfc.capitalize()}-{seed}"
                    ax.plot(k_vals, fpr_vals * 100,
                           marker=MARKERS[sfc],
                           color=COLORS[sfc],
                           linestyle=LINESTYLES[seed],
                           label=label,
                           markersize=6,
                           linewidth=1.5)

        if has_data:
            ax.set_xlabel('k (hash functions)')
            ax.set_ylabel('False Positive Rate (%)')
            ax.set_title(title)
            ax.legend(loc='best', fontsize=9)
            ax.grid(True, alpha=0.3)
            ax.set_xticks(range(2, 13))

    plt.tight_layout()

    # Save
    for ext in ['pdf', 'png']:
        filepath = Path(output_dir) / f"k_sensitivity_synthetic.{ext}"
        fig.savefig(filepath, dpi=150 if ext == 'png' else None)
        print(f"Saved: {filepath}")

    plt.close()


def plot_synthetic_all_strategies(results: dict, output_dir: str):
    """Plot synthetic data with all intra-block strategies (2x2 grid)."""
    fig, axes = plt.subplots(2, 2, figsize=(12, 10))

    data = results.get('results', [])

    configs = [
        (2, 'double_hash', '2D Double-Hash', axes[0, 0]),
        (2, 'pattern_lookup', '2D Pattern-Lookup', axes[0, 1]),
        (3, 'double_hash', '3D Double-Hash', axes[1, 0]),
        (3, 'pattern_lookup', '3D Pattern-Lookup', axes[1, 1]),
    ]

    for dim, intra, title, ax in configs:
        has_data = False

        for sfc in ['MORTON', 'HILBERT']:
            for seed in ['XOR', 'MULTIPLY_SHIFT']:
                sfc_full = f"{sfc}_{dim}D"
                k_vals, fpr_vals = extract_series(data, sfc_full, dim, intra, seed)

                if len(k_vals) > 0:
                    has_data = True
                    label = f"{sfc.capitalize()}-{seed}"
                    ax.plot(k_vals, fpr_vals * 100,
                           marker=MARKERS[sfc],
                           color=COLORS[sfc],
                           linestyle=LINESTYLES[seed],
                           label=label,
                           markersize=6,
                           linewidth=1.5)

        if has_data:
            ax.set_xlabel('k (hash functions)')
            ax.set_ylabel('FPR (%)')
            ax.set_title(title)
            ax.legend(loc='best', fontsize=8)
            ax.grid(True, alpha=0.3)
            ax.set_xticks(range(2, 13))

    plt.tight_layout()

    for ext in ['pdf', 'png']:
        filepath = Path(output_dir) / f"k_sensitivity_synthetic_all.{ext}"
        fig.savefig(filepath, dpi=150 if ext == 'png' else None)
        print(f"Saved: {filepath}")

    plt.close()


def plot_voxel(results: dict, output_dir: str, intra_strategy: str = 'double_hash'):
    """Plot voxel data FPR vs k (bunny and teapot subplots)."""
    fig, axes = plt.subplots(1, 2, figsize=(10, 4))

    meshes = [('bunny', 'Stanford Bunny', axes[0]), ('teapot', 'Utah Teapot', axes[1])]
    data = results.get('results', [])

    # Colors for seed strategies in voxel plot
    seed_colors = {
        'XOR': '#2ca02c',       # Green
        'MULTIPLY_SHIFT': '#d62728',  # Red
    }

    for mesh, title, ax in meshes:
        has_data = False

        for seed in ['XOR', 'MULTIPLY_SHIFT']:
            k_vals, fpr_vals = extract_voxel_series(data, mesh, intra_strategy, seed)

            if len(k_vals) > 0:
                has_data = True
                label = f"Hilbert-{seed}"
                ax.plot(k_vals, fpr_vals * 100,
                       marker='s',
                       color=seed_colors[seed],
                       linestyle=LINESTYLES[seed],
                       label=label,
                       markersize=6,
                       linewidth=1.5)

        if has_data:
            ax.set_xlabel('k (hash functions)')
            ax.set_ylabel('False Positive Rate (%)')
            ax.set_title(title)
            ax.legend(loc='best')
            ax.grid(True, alpha=0.3)
            ax.set_xticks(range(2, 13))

    plt.tight_layout()

    for ext in ['pdf', 'png']:
        filepath = Path(output_dir) / f"k_sensitivity_voxel.{ext}"
        fig.savefig(filepath, dpi=150 if ext == 'png' else None)
        print(f"Saved: {filepath}")

    plt.close()


def plot_voxel_all_strategies(results: dict, output_dir: str):
    """Plot voxel data with all intra-block strategies (2x2 grid)."""
    fig, axes = plt.subplots(2, 2, figsize=(12, 10))

    data = results.get('results', [])

    configs = [
        ('bunny', 'double_hash', 'Bunny Double-Hash', axes[0, 0]),
        ('bunny', 'pattern_lookup', 'Bunny Pattern-Lookup', axes[0, 1]),
        ('teapot', 'double_hash', 'Teapot Double-Hash', axes[1, 0]),
        ('teapot', 'pattern_lookup', 'Teapot Pattern-Lookup', axes[1, 1]),
    ]

    seed_colors = {
        'XOR': '#2ca02c',
        'MULTIPLY_SHIFT': '#d62728',
    }

    for mesh, intra, title, ax in configs:
        has_data = False

        for seed in ['XOR', 'MULTIPLY_SHIFT']:
            k_vals, fpr_vals = extract_voxel_series(data, mesh, intra, seed)

            if len(k_vals) > 0:
                has_data = True
                label = f"Hilbert-{seed}"
                ax.plot(k_vals, fpr_vals * 100,
                       marker='s',
                       color=seed_colors[seed],
                       linestyle=LINESTYLES[seed],
                       label=label,
                       markersize=6,
                       linewidth=1.5)

        if has_data:
            ax.set_xlabel('k (hash functions)')
            ax.set_ylabel('FPR (%)')
            ax.set_title(title)
            ax.legend(loc='best', fontsize=8)
            ax.grid(True, alpha=0.3)
            ax.set_xticks(range(2, 13))

    plt.tight_layout()

    for ext in ['pdf', 'png']:
        filepath = Path(output_dir) / f"k_sensitivity_voxel_all.{ext}"
        fig.savefig(filepath, dpi=150 if ext == 'png' else None)
        print(f"Saved: {filepath}")

    plt.close()


def compute_voxel_mean_std(results: list, intra_strategy: str, seed_strategy: str) -> tuple:
    """Compute mean and std FPR across all meshes for each k."""
    meshes = ['bunny', 'teapot', 'armadillo', 'dragon']

    # Collect FPR values for each k across meshes
    k_to_fprs = {}
    for mesh in meshes:
        k_vals, fpr_vals = extract_voxel_series(results, mesh, intra_strategy, seed_strategy)
        for k, fpr in zip(k_vals, fpr_vals):
            if k not in k_to_fprs:
                k_to_fprs[k] = []
            k_to_fprs[k].append(fpr)

    # Compute mean and std for each k
    k_values = sorted(k_to_fprs.keys())
    mean_fpr = np.array([np.mean(k_to_fprs[k]) for k in k_values])
    std_fpr = np.array([np.std(k_to_fprs[k]) for k in k_values])

    return np.array(k_values), mean_fpr, std_fpr


def plot_combined(synthetic: dict, voxel: dict, output_dir: str):
    """Create combined figure with 2D, 3D synthetic and averaged voxel data.

    Single unified plot showing FPR vs k for best configuration:
    - Hilbert + pattern_lookup + XOR for all data types
    """
    fig, ax = plt.subplots(figsize=(8, 5))

    syn_data = synthetic.get('results', [])
    vox_data = voxel.get('results', [])

    # Best configuration: Hilbert + pattern_lookup + XOR
    intra = 'pattern_lookup'
    seed = 'XOR'

    # Colors for each data type
    colors = {
        '2D': '#1f77b4',  # Blue
        '3D': '#ff7f0e',  # Orange
        'Voxel': '#2ca02c',  # Green
    }

    # 2D Synthetic
    k_2d, fpr_2d = extract_series(syn_data, 'HILBERT_2D', 2, intra, seed)
    if len(k_2d) > 0:
        ax.plot(k_2d, fpr_2d * 100, marker='o', color=colors['2D'],
               linestyle='-', label='2D Synthetic', markersize=8, linewidth=2)

    # 3D Synthetic
    k_3d, fpr_3d = extract_series(syn_data, 'HILBERT_3D', 3, intra, seed)
    if len(k_3d) > 0:
        ax.plot(k_3d, fpr_3d * 100, marker='s', color=colors['3D'],
               linestyle='-', label='3D Synthetic', markersize=8, linewidth=2)

    # Voxel (averaged across 4 meshes with error bars)
    k_vox, mean_fpr, std_fpr = compute_voxel_mean_std(vox_data, intra, seed)
    if len(k_vox) > 0:
        ax.errorbar(k_vox, mean_fpr * 100, yerr=std_fpr * 100,
                   marker='^', color=colors['Voxel'], linestyle='-',
                   label='Voxels (mean of 4 meshes)', markersize=8, linewidth=2,
                   capsize=4, capthick=1.5)

    # Find optimal k (minimum FPR across all)
    all_fprs = []
    if len(fpr_2d) > 0:
        all_fprs.extend(zip(k_2d, fpr_2d))
    if len(fpr_3d) > 0:
        all_fprs.extend(zip(k_3d, fpr_3d))
    if len(mean_fpr) > 0:
        all_fprs.extend(zip(k_vox, mean_fpr))

    if all_fprs:
        # Find k with minimum average FPR
        k_counts = {}
        for k, fpr in all_fprs:
            if k not in k_counts:
                k_counts[k] = []
            k_counts[k].append(fpr)
        k_avg = {k: np.mean(fprs) for k, fprs in k_counts.items()}
        optimal_k = min(k_avg, key=k_avg.get)

        # Add vertical line at optimal k
        ax.axvline(x=optimal_k, color='gray', linestyle='--', linewidth=1.5, alpha=0.7)
        ax.text(optimal_k + 0.15, ax.get_ylim()[1] * 0.9, f'k={optimal_k}',
               fontsize=12, color='gray', fontweight='bold')

    # Formatting
    ax.set_xlabel('k (hash functions)', fontsize=14)
    ax.set_ylabel('False Positive Rate (%)', fontsize=14)
    ax.set_title('SBBF FPR vs k (Hilbert + pattern_lookup + XOR)', fontsize=16)
    ax.legend(loc='upper right', fontsize=11)
    ax.grid(True, alpha=0.3)
    ax.set_xticks(range(2, 13))
    ax.set_yscale('log')
    ax.set_ylim(0.01, 20)

    plt.tight_layout()

    for ext in ['pdf', 'png']:
        filepath = Path(output_dir) / f"k_sensitivity_combined.{ext}"
        fig.savefig(filepath, dpi=150 if ext == 'png' else None)
        print(f"Saved: {filepath}")

    plt.close()


def main():
    parser = argparse.ArgumentParser(description='Generate k-sensitivity figures')
    parser.add_argument('--input', '-i', default='sbbf_results/k_sensitivity',
                        help='Input directory with JSON results')
    parser.add_argument('--output', '-o', default='sbbf_results/figures',
                        help='Output directory for figures')
    parser.add_argument('--all-strategies', action='store_true',
                        help='Generate 2x2 grid figures with all intra-block strategies')
    args = parser.parse_args()

    Path(args.output).mkdir(parents=True, exist_ok=True)

    print("=" * 60)
    print("Generating k-sensitivity figures")
    print("=" * 60)

    # Plot synthetic data
    synthetic = None
    try:
        synthetic = load_synthetic_results(args.input)
        print("\nSynthetic data figures:")
        plot_synthetic(synthetic, args.output)
        if args.all_strategies:
            plot_synthetic_all_strategies(synthetic, args.output)
    except FileNotFoundError as e:
        print(f"Warning: {e}")

    # Plot voxel data
    voxel = None
    try:
        voxel = load_voxel_results(args.input)
        print("\nVoxel data figures:")
        plot_voxel(voxel, args.output)
        if args.all_strategies:
            plot_voxel_all_strategies(voxel, args.output)
    except FileNotFoundError as e:
        print(f"Warning: {e}")

    # Plot combined figure (requires both synthetic and voxel data)
    if synthetic is not None and voxel is not None:
        print("\nCombined figure:")
        plot_combined(synthetic, voxel, args.output)

    print("\n" + "=" * 60)
    print("Done!")
    print("=" * 60)


if __name__ == '__main__':
    main()
