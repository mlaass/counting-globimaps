#!/usr/bin/env python3
"""Generate LaTeX tables from SBBF benchmark JSON results.

Usage:
    uv run scripts/generate_latex_tables.py --input sbbf_results/synthetic_comparison.json
    uv run scripts/generate_latex_tables.py --baseline RegisterBF  # Change baseline
    uv run scripts/generate_latex_tables.py --table fpr            # Only FPR table
"""

import argparse
import json
from collections import defaultdict
from pathlib import Path
from typing import Optional

# ============================================================================
# CONFIGURATION - Modify these constants as needed
# ============================================================================

BASELINE_FILTER = "BlockedBF"  # Default baseline for relative FPR comparison

# Filter display name mapping (raw name -> display name)
FILTER_DISPLAY_NAMES = {
    "SBBF-Morton2D": "SBBF-Morton",
    "SBBF-Morton3D": "SBBF-Morton",
    "SBBF-Hilbert2D": "SBBF-Hilbert",
    "SBBF-Hilbert3D": "SBBF-Hilbert",
    "BlockedBF-Wy+Pat": "BlockedBF",
    "RegisterBF-Wy+Pat": "RegisterBF",
}

# Strategy display name mapping
STRATEGY_DISPLAY_NAMES = {
    "double_hash": "dbl",
    "pattern_lookup": "pat",
}

# Canonical order for filters in tables
FILTER_ORDER = [
    "SBBF-Morton",
    "SBBF-Hilbert",
    "BlockedBF",
    "RegisterBF",
]


# ============================================================================
# Helper functions
# ============================================================================

def load_json(filepath: str) -> dict:
    """Load JSON benchmark results."""
    with open(filepath) as f:
        return json.load(f)


def get_display_name(raw_name: str) -> str:
    """Get display name for a filter."""
    return FILTER_DISPLAY_NAMES.get(raw_name, raw_name)


def get_strategy_display(strategy: str) -> str:
    """Get display name for a strategy."""
    return STRATEGY_DISPLAY_NAMES.get(strategy, strategy)


def format_fpr(fpr: float) -> str:
    """Format FPR in scientific notation (e.g., 8.0e-4)."""
    if fpr == 0:
        return "0"
    # Format as X.Ye-Z
    exp = 0
    val = fpr
    while val < 1.0:
        val *= 10
        exp -= 1
    while val >= 10.0:
        val /= 10
        exp += 1
    return f"{val:.1f}e{exp}"


def format_relative(rel: float) -> str:
    """Format relative value (e.g., 0.25$\\times$)."""
    return f"{rel:.2f}$\\times$"


def highlight_best(value: str) -> str:
    """Apply highlighting to best performer (bold + green cell)."""
    return f"\\cellcolor{{green!20}}\\textbf{{{value}}}"


def find_min_indices(values: list, key=lambda x: x) -> list[int]:
    """Find all indices with the minimum value."""
    if not values:
        return []
    min_val = min(key(v) for v in values)
    return [i for i, v in enumerate(values) if key(v) == min_val]


# ============================================================================
# Table generation
# ============================================================================

def generate_performance_table(data: dict, dim: int) -> list[dict]:
    """Extract performance data for a given dimension."""
    rows = []
    for entry in data.get("results", []):
        config = entry.get("config", {})
        metrics = entry.get("metrics", {})

        if config.get("dimensions") != dim:
            continue

        # Only include one distribution (uniform) to avoid duplicates
        if config.get("distribution") != "uniform":
            continue

        rows.append({
            "filter": get_display_name(config.get("name", "")),
            "strategy": get_strategy_display(config.get("strategy", "")),
            "insert_ins": int(metrics.get("insert_ins", 0)),
            "insert_cyc": int(metrics.get("insert_cyc", 0)),
            "query_ins": int(metrics.get("query_ins", 0)),
            "query_cyc": int(metrics.get("query_cyc", 0)),
        })

    # Sort by filter order then strategy
    def sort_key(row):
        try:
            filter_idx = FILTER_ORDER.index(row["filter"])
        except ValueError:
            filter_idx = 999
        return (filter_idx, row["strategy"])

    rows.sort(key=sort_key)
    return rows


def generate_performance_table_latex(data: dict) -> str:
    """Generate Table 2: End-to-End Performance."""
    lines = []

    # Header
    lines.append("\\begin{table}[htbp]")
    lines.append("\\centering")
    lines.append("\\caption{SBBF End-to-End Operation Performance}")
    lines.append("\\label{tab:sbbf-performance}")
    lines.append("\\begin{tabular}{@{}lllrrrr@{}}")
    lines.append("\\toprule")
    lines.append(" & & & \\multicolumn{2}{c}{Insert} & \\multicolumn{2}{c}{Query} \\\\")
    lines.append("\\cmidrule(lr){4-5} \\cmidrule(lr){6-7}")
    lines.append("Dim & Filter & Strat. & Instr. & Cyc. & Instr. & Cyc. \\\\")
    lines.append("\\midrule")

    # Generate rows for 2D and 3D
    for dim in [2, 3]:
        rows = generate_performance_table(data, dim)

        if not rows:
            continue

        # Find best performers (minimum) for each metric
        best_insert_ins = min(r["insert_ins"] for r in rows)
        best_insert_cyc = min(r["insert_cyc"] for r in rows)
        best_query_ins = min(r["query_ins"] for r in rows)
        best_query_cyc = min(r["query_cyc"] for r in rows)

        # Section header
        lines.append(f"\\multicolumn{{7}}{{@{{}}l}}{{\\textit{{{dim}D}}}} \\\\")

        for row in rows:
            # Format values with highlighting
            ins_ins = str(row["insert_ins"])
            ins_cyc = str(row["insert_cyc"])
            q_ins = str(row["query_ins"])
            q_cyc = str(row["query_cyc"])

            if row["insert_ins"] == best_insert_ins:
                ins_ins = highlight_best(ins_ins)
            if row["insert_cyc"] == best_insert_cyc:
                ins_cyc = highlight_best(ins_cyc)
            if row["query_ins"] == best_query_ins:
                q_ins = highlight_best(q_ins)
            if row["query_cyc"] == best_query_cyc:
                q_cyc = highlight_best(q_cyc)

            lines.append(f" & {row['filter']} & {row['strategy']} & {ins_ins} & {ins_cyc} & {q_ins} & {q_cyc} \\\\")

        # Add midrule between 2D and 3D sections
        if dim == 2:
            lines.append("\\midrule")

    lines.append("\\bottomrule")
    lines.append("\\end{tabular}")
    lines.append("\\end{table}")

    return "\n".join(lines)


def generate_fpr_table_data(data: dict, distribution: str) -> dict:
    """Extract FPR data for a given distribution, organized by dimension."""
    # Collect data keyed by (filter, strategy)
    fpr_data = defaultdict(dict)

    for entry in data.get("results", []):
        config = entry.get("config", {})
        metrics = entry.get("metrics", {})

        if config.get("distribution") != distribution:
            continue

        filter_name = get_display_name(config.get("name", ""))
        strategy = get_strategy_display(config.get("strategy", ""))
        dim = config.get("dimensions", 2)
        fpr = metrics.get("fpr", 0)

        key = (filter_name, strategy)
        fpr_data[key][f"fpr_{dim}d"] = fpr

    return fpr_data


def generate_neighbor_table_latex(data: dict, baseline: str, dim: int = 3) -> str:
    """Generate Table: Neighbor Query Performance."""
    lines = []

    # Extract neighbor data for the specified dimension
    rows = []
    for entry in data.get("results", []):
        config = entry.get("config", {})
        metrics = entry.get("metrics", {})

        if config.get("dimensions") != dim:
            continue

        # Only include one distribution (uniform) to avoid duplicates
        if config.get("distribution") != "uniform":
            continue

        neighbor_ins = int(metrics.get("neighbor_ins", 0))
        neighbor_cyc = int(metrics.get("neighbor_cyc", 0))
        ipc = neighbor_ins / neighbor_cyc if neighbor_cyc > 0 else 0

        rows.append({
            "filter": get_display_name(config.get("name", "")),
            "strategy": get_strategy_display(config.get("strategy", "")),
            "neighbor_ins": neighbor_ins,
            "neighbor_cyc": neighbor_cyc,
            "ipc": ipc,
        })

    # Sort by filter order then strategy
    def sort_key(row):
        try:
            filter_idx = FILTER_ORDER.index(row["filter"])
        except ValueError:
            filter_idx = 999
        return (filter_idx, row["strategy"])

    rows.sort(key=sort_key)

    if not rows:
        return "% No neighbor data found"

    # Get baseline cycles for relative comparison
    baseline_cyc = None
    for row in rows:
        if row["filter"] == baseline:
            baseline_cyc = row["neighbor_cyc"]
            break
    if baseline_cyc is None:
        baseline_cyc = rows[-1]["neighbor_cyc"]  # fallback to last

    # Add relative values
    for row in rows:
        row["rel"] = row["neighbor_cyc"] / baseline_cyc if baseline_cyc > 0 else 0

    # Find best performers
    best_ins = min(r["neighbor_ins"] for r in rows)
    best_cyc = min(r["neighbor_cyc"] for r in rows)
    best_ipc = max(r["ipc"] for r in rows)  # Higher IPC is better

    # Determine number of neighbors based on dimension
    num_neighbors = 6 if dim == 3 else 4

    # Header
    lines.append("\\begin{table}[htbp]")
    lines.append("\\centering")
    lines.append(f"\\caption{{{dim}D Neighborhood Query Performance ({num_neighbors} neighbors)}}")
    lines.append("\\label{tab:neighbor-performance}")
    lines.append("\\begin{tabular}{@{}llrrrr@{}}")
    lines.append("\\toprule")
    lines.append("Filter & Strat. & Instr. & Cyc. & IPC & $\\times$BL \\\\")
    lines.append("\\midrule")

    for row in rows:
        # Format values
        ins_str = str(row["neighbor_ins"])
        cyc_str = str(row["neighbor_cyc"])
        ipc_str = f"{row['ipc']:.1f}"
        rel_str = format_relative(row["rel"])

        # Highlight best
        if row["neighbor_ins"] == best_ins:
            ins_str = highlight_best(ins_str)
        if row["neighbor_cyc"] == best_cyc:
            cyc_str = highlight_best(cyc_str)
        if row["ipc"] == best_ipc:
            ipc_str = highlight_best(ipc_str)

        lines.append(f"{row['filter']} & {row['strategy']} & {ins_str} & {cyc_str} & {ipc_str} & {rel_str} \\\\")

    lines.append("\\bottomrule")
    lines.append("\\end{tabular}")
    lines.append("\\end{table}")

    return "\n".join(lines)


def generate_fpr_table_latex(data: dict, baseline: str) -> str:
    """Generate Table 3: FPR Comparison."""
    lines = []

    # Header
    lines.append("\\begin{table}[htbp]")
    lines.append("\\centering")
    lines.append("\\caption{False Positive Rate Comparison}")
    lines.append("\\label{tab:fpr-datasets}")
    lines.append("\\begin{tabular}{@{}lllrrrr@{}}")
    lines.append("\\toprule")
    lines.append(" & & & \\multicolumn{2}{c}{2D} & \\multicolumn{2}{c}{3D} \\\\")
    lines.append("\\cmidrule(lr){4-5} \\cmidrule(lr){6-7}")
    lines.append("Dist. & Filter & Strat. & FPR & $\\times$BL & FPR & $\\times$BL \\\\")
    lines.append("\\midrule")

    distributions = [
        ("uniform", "Synthetic Uniform (100K elements)"),
        ("clustered", "Synthetic Clustered (100K elements, 100 clusters)"),
    ]

    for dist_key, dist_label in distributions:
        fpr_data = generate_fpr_table_data(data, dist_key)

        if not fpr_data:
            continue

        # Get baseline FPR values
        baseline_2d = None
        baseline_3d = None
        for (filter_name, _), values in fpr_data.items():
            if filter_name == baseline:
                baseline_2d = values.get("fpr_2d", 1)
                baseline_3d = values.get("fpr_3d", 1)
                break

        if baseline_2d is None:
            baseline_2d = 1
        if baseline_3d is None:
            baseline_3d = 1

        # Build rows with relative values
        rows = []
        for (filter_name, strategy), values in fpr_data.items():
            fpr_2d = values.get("fpr_2d", 0)
            fpr_3d = values.get("fpr_3d", 0)
            rel_2d = fpr_2d / baseline_2d if baseline_2d > 0 else 0
            rel_3d = fpr_3d / baseline_3d if baseline_3d > 0 else 0

            rows.append({
                "filter": filter_name,
                "strategy": strategy,
                "fpr_2d": fpr_2d,
                "rel_2d": rel_2d,
                "fpr_3d": fpr_3d,
                "rel_3d": rel_3d,
            })

        # Sort by filter order then strategy
        def sort_key(row):
            try:
                filter_idx = FILTER_ORDER.index(row["filter"])
            except ValueError:
                filter_idx = 999
            return (filter_idx, row["strategy"])

        rows.sort(key=sort_key)

        # Find best (lowest) FPR for each dimension
        best_fpr_2d = min(r["fpr_2d"] for r in rows if r["fpr_2d"] > 0)
        best_fpr_3d = min(r["fpr_3d"] for r in rows if r["fpr_3d"] > 0)

        # Section header
        lines.append(f"\\multicolumn{{7}}{{@{{}}l}}{{\\textit{{{dist_label}}}}} \\\\")

        for row in rows:
            # Format FPR values
            fpr_2d_str = format_fpr(row["fpr_2d"])
            fpr_3d_str = format_fpr(row["fpr_3d"])
            rel_2d_str = format_relative(row["rel_2d"])
            rel_3d_str = format_relative(row["rel_3d"])

            # Highlight best FPR
            if row["fpr_2d"] == best_fpr_2d:
                fpr_2d_str = highlight_best(fpr_2d_str)
            if row["fpr_3d"] == best_fpr_3d:
                fpr_3d_str = highlight_best(fpr_3d_str)

            lines.append(f" & {row['filter']} & {row['strategy']} & {fpr_2d_str} & {rel_2d_str} & {fpr_3d_str} & {rel_3d_str} \\\\")

        # Add midrule between distributions
        if dist_key == "uniform":
            lines.append("\\midrule")

    lines.append("\\bottomrule")
    lines.append("\\end{tabular}")
    lines.append("\\end{table}")

    return "\n".join(lines)


# ============================================================================
# Main
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="Generate LaTeX tables from SBBF benchmark results"
    )
    parser.add_argument(
        "--input", "-i",
        default="sbbf_results/synthetic_comparison.json",
        help="Input JSON file path"
    )
    parser.add_argument(
        "--output", "-o",
        default=None,
        help="Output file path (prints to stdout if not specified)"
    )
    parser.add_argument(
        "--baseline", "-b",
        default=BASELINE_FILTER,
        help=f"Baseline filter for relative FPR comparison (default: {BASELINE_FILTER})"
    )
    parser.add_argument(
        "--table", "-t",
        choices=["performance", "fpr", "neighbor", "all"],
        default="all",
        help="Which table(s) to generate"
    )
    parser.add_argument(
        "--dim", "-d",
        type=int,
        default=3,
        choices=[2, 3],
        help="Dimension for neighbor table (default: 3)"
    )
    args = parser.parse_args()

    # Load data
    print(f"Loading: {args.input}", file=__import__("sys").stderr)
    data = load_json(args.input)

    # Generate tables
    output_lines = []

    if args.table in ["performance", "all"]:
        output_lines.append("% Table 2: End-to-End Performance")
        output_lines.append("% Requires: \\usepackage{booktabs}, \\usepackage[table]{xcolor}")
        output_lines.append("")
        output_lines.append(generate_performance_table_latex(data))
        output_lines.append("")

    if args.table in ["fpr", "all"]:
        output_lines.append("% Table 3: FPR Comparison")
        output_lines.append(f"% Baseline: {args.baseline}")
        output_lines.append("% Requires: \\usepackage{booktabs}, \\usepackage[table]{xcolor}")
        output_lines.append("")
        output_lines.append(generate_fpr_table_latex(data, args.baseline))
        output_lines.append("")

    if args.table in ["neighbor", "all"]:
        output_lines.append(f"% Table: {args.dim}D Neighbor Query Performance")
        output_lines.append(f"% Baseline: {args.baseline}")
        output_lines.append("% Requires: \\usepackage{booktabs}, \\usepackage[table]{xcolor}")
        output_lines.append("")
        output_lines.append(generate_neighbor_table_latex(data, args.baseline, args.dim))
        output_lines.append("")

    output = "\n".join(output_lines)

    # Output
    if args.output:
        Path(args.output).write_text(output)
        print(f"Written to: {args.output}", file=__import__("sys").stderr)
    else:
        print(output)


if __name__ == "__main__":
    main()
