#!/usr/bin/env python3
"""Generate LaTeX table for seed strategy comparison on voxelized meshes.

Reads voxel_3d.json from seed strategy benchmark and generates table_seed_voxel.tex.

Usage:
    uv run scripts/generate_seed_voxel_table.py
    uv run scripts/generate_seed_voxel_table.py --input sbbf_results/seed_strategy/voxel_3d.json
    uv run scripts/generate_seed_voxel_table.py --output sbbf_paper/table_seed_voxel.tex
"""

import argparse
import json
from pathlib import Path


def load_json(filepath: str) -> dict:
    """Load JSON benchmark results."""
    with open(filepath) as f:
        return json.load(f)


def format_fpr_percent(fpr: float) -> str:
    """Format FPR as percentage with 2 decimal places."""
    return f"{fpr * 100:.2f}\\%"


def generate_table(data: dict, sfc_type: str = "HILBERT_3D", intra_strategy: str = "double_hash") -> str:
    """Generate LaTeX table from voxel seed strategy results.

    Args:
        data: JSON data from voxel_3d.json
        sfc_type: SFC type to filter by (default: HILBERT_3D)
        intra_strategy: Intra-block strategy to filter by (default: double_hash)
    """
    results = data.get("results", [])
    hash_k = data.get("config", {}).get("hash_k", 8)

    # Group results by mesh
    mesh_data = {}
    for entry in results:
        config = entry.get("config", {})
        metrics = entry.get("metrics", {})

        # Filter by SFC type and intra-block strategy
        if config.get("sfc_type") != sfc_type:
            continue
        if config.get("intra_strategy") != intra_strategy:
            continue

        mesh = config.get("mesh", "")
        resolution = config.get("resolution", 128)
        seed = config.get("seed_strategy", "")
        fpr = metrics.get("fpr", 0)

        if mesh not in mesh_data:
            mesh_data[mesh] = {"resolution": resolution}
        mesh_data[mesh][seed] = fpr

    # Generate LaTeX
    lines = []

    # Determine strategy display names
    sfc_display = sfc_type.replace("_", " ").title().replace("3d", "3D").replace("2d", "2D")
    intra_display = f"\\texttt{{{intra_strategy}}}"

    lines.append("\\begin{table}[htbp]")
    lines.append("\\centering")
    lines.append(f"\\caption{{Seed Strategy on Voxelized Meshes ({sfc_display}, {intra_display}, $k={hash_k}$)}}")
    lines.append("\\label{tab:seed-voxel}")
    lines.append("\\begin{tabular}{@{}llrr@{}}")
    lines.append("\\toprule")
    lines.append("Mesh & Res. & XOR & Mult-Shift \\\\")
    lines.append("\\midrule")

    # Sort meshes in canonical order
    mesh_order = ["bunny", "teapot", "armadillo", "dragon"]
    sorted_meshes = sorted(mesh_data.keys(), key=lambda m: mesh_order.index(m) if m in mesh_order else 999)

    for mesh in sorted_meshes:
        info = mesh_data[mesh]
        res = info.get("resolution", 128)
        xor_fpr = info.get("XOR", 0)
        ms_fpr = info.get("MULTIPLY_SHIFT", 0)

        mesh_display = mesh.title()
        res_display = f"${res}^3$"
        xor_display = format_fpr_percent(xor_fpr)
        ms_display = format_fpr_percent(ms_fpr)

        # Highlight lower FPR (better)
        if xor_fpr < ms_fpr:
            xor_display = f"\\textbf{{{xor_display}}}"
        elif ms_fpr < xor_fpr:
            ms_display = f"\\textbf{{{ms_display}}}"

        lines.append(f"{mesh_display} & {res_display} & {xor_display} & {ms_display} \\\\")

    lines.append("\\bottomrule")
    lines.append("\\end{tabular}")
    lines.append("\\end{table}")

    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(
        description="Generate LaTeX table for seed strategy on voxelized meshes"
    )
    parser.add_argument(
        "--input", "-i",
        default="sbbf_results/seed_strategy/voxel_3d.json",
        help="Input JSON file path"
    )
    parser.add_argument(
        "--output", "-o",
        default="sbbf_paper/table_seed_voxel.tex",
        help="Output LaTeX file path"
    )
    parser.add_argument(
        "--sfc", "-s",
        default="HILBERT_3D",
        choices=["HILBERT_3D", "MORTON_3D"],
        help="SFC type to include in table (default: HILBERT_3D)"
    )
    parser.add_argument(
        "--strategy",
        default="double_hash",
        choices=["double_hash", "pattern_lookup"],
        help="Intra-block strategy to include (default: double_hash)"
    )
    args = parser.parse_args()

    # Load data
    print(f"Loading: {args.input}")
    data = load_json(args.input)

    # Get hash_k for info
    hash_k = data.get("config", {}).get("hash_k", "?")
    print(f"Hash k: {hash_k}")

    # Generate table
    table = generate_table(data, sfc_type=args.sfc, intra_strategy=args.strategy)

    # Write output
    Path(args.output).parent.mkdir(parents=True, exist_ok=True)
    Path(args.output).write_text(table)
    print(f"Written to: {args.output}")

    # Also print to stdout
    print("\n" + "=" * 60)
    print(table)
    print("=" * 60)


if __name__ == "__main__":
    main()
