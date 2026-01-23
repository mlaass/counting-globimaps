#!/usr/bin/env python3
"""
Generate LaTeX tables for shortpaper from JSON experiment results.

Usage:
    uv run scripts/generate_shortpaper_tables.py

Reads JSON from results/ directories and outputs LaTeX to cbf-shortpaper/tables/
"""

import json
import os
from pathlib import Path

RESULTS_DIR = Path("results")
OUTPUT_DIR = Path("cbf-shortpaper/tables")


def load_json(filepath: Path) -> dict:
    """Load JSON file."""
    with open(filepath) as f:
        return json.load(f)


def generate_multicategory_table(json_path: Path, output_path: Path, label_suffix: str = ""):
    """Generate Table 1: Multi-category isolation comparison."""
    data = load_json(json_path)

    target_kb = data.get("target_memory_kb", "N/A")
    resolution = data.get("resolution_scale", "N/A")
    dataset = data.get("dataset", "unknown")

    # Sort by error (ascending)
    implementations = sorted(
        data["implementations"],
        key=lambda x: x["overall_mean_error_pct"]
    )

    # Count total events from first implementation's categories
    total_events = sum(
        cat["insert_count"]
        for cat in implementations[0]["categories"]
    )
    total_events_str = f"{total_events/1e6:.1f}M"

    # Get unique cells and max count if available
    unique_cells = implementations[0].get("unique_cells", "N/A")
    max_count = implementations[0].get("max_ground_truth_count", "N/A")

    # Build caption with resolution info
    if resolution != "N/A" and resolution != 1000000:
        caption = f"Multi-category isolation on GDELT ({total_events_str} events) at {target_kb}~KB, resolution={int(resolution)}."
    else:
        caption = f"Multi-category isolation on GDELT ({total_events_str} events) at {target_kb}~KB."

    label = f"tab:multicategory{label_suffix}"

    latex = f"""% Auto-generated from {json_path.name}
% Run: uv run scripts/generate_shortpaper_tables.py
% Unique cells: {unique_cells}, Max count: {max_count}
\\begin{{table}}[ht]
\\centering
\\caption{{{caption}}}
\\label{{{label}}}
\\small
\\begin{{tabular}}{{lcc}}
\\tophline
\\textbf{{Implementation}} & \\textbf{{Memory}} & \\textbf{{Error}} \\\\
 & (KB) & (\\%) \\\\
\\middlehline
"""

    for impl in implementations:
        name = impl["name"]
        mem_kb = impl["memory_bytes"] // 1024
        error = impl["overall_mean_error_pct"]
        latex += f"{name} & {mem_kb} & {error:.2f} \\\\\n"

    latex += """\\bottomhline
\\end{tabular}
\\end{table}
"""

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with open(output_path, 'w') as f:
        f.write(latex)
    print(f"Generated: {output_path}")


def format_count(n: int) -> str:
    """Format count as K/M suffix."""
    if n >= 1_000_000:
        return f"{n/1_000_000:.1f}M"
    elif n >= 1_000:
        return f"{n/1_000:.0f}K"
    return str(n)


def generate_timeseries_table(json_path: Path, output_path: Path):
    """Generate Table 2: Time-series accuracy across snapshots."""
    data = load_json(json_path)

    implementations = data["implementations"]

    # Get snapshot dates from first implementation
    first_impl = implementations[0]
    snapshots = first_impl.get("snapshots", [])

    # Format dates as YYYY-MM
    dates = [s["date"][:7] for s in snapshots]  # YYYY-MM

    # Build header with date and cumulative points
    header_dates = []
    for s in snapshots:
        date = s["date"][:7]
        cum = format_count(s["cumulative_points"])
        header_dates.append(f"{date}")

    # Also show cumulative points row
    cum_points = [format_count(s["cumulative_points"]) for s in snapshots]

    latex = f"""% Auto-generated from {json_path.name}
% Run: uv run scripts/generate_shortpaper_tables.py
\\begin{{table}}[ht]
\\centering
\\caption{{Time-series accuracy: mean error (\\%) at each snapshot.}}
\\label{{tab:timeseries}}
\\small
\\begin{{tabular}}{{l{'c' * len(dates)}}}
\\tophline
\\textbf{{Date}} & {' & '.join(header_dates)} \\\\
\\textbf{{Cumulative}} & {' & '.join(cum_points)} \\\\
\\middlehline
"""

    for impl in implementations:
        name = impl["name"]
        impl_snapshots = impl.get("snapshots", [])
        errors = [s["mean_error_pct"] for s in impl_snapshots]
        error_strs = [f"{e:.2f}" if e > 0 else "0.00" for e in errors]
        latex += f"{name} & {' & '.join(error_strs)} \\\\\n"

    latex += """\\bottomhline
\\end{tabular}
\\end{table}
"""

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with open(output_path, 'w') as f:
        f.write(latex)
    print(f"Generated: {output_path}")


def generate_taxi_table(json_path: Path, output_path: Path):
    """Generate Table 3: NYC Taxi urban hotspot accuracy."""
    data = load_json(json_path)

    implementations = data["implementations"]

    # Sort by mean error
    implementations = sorted(
        implementations,
        key=lambda x: x.get("mean_error_pct", 0)
    )

    # Get total points from first implementation
    total_points = format_count(implementations[0].get("total_points", 0))

    latex = f"""% Auto-generated from {json_path.name}
% Run: uv run scripts/generate_shortpaper_tables.py
\\begin{{table}}[ht]
\\centering
\\caption{{NYC Taxi: urban hotspot accuracy ({total_points} pickups).}}
\\label{{tab:taxi}}
\\small
\\begin{{tabular}}{{lcccc}}
\\tophline
\\textbf{{Implementation}} & \\textbf{{Memory}} & \\textbf{{Insert}} & \\textbf{{Mean Err}} & \\textbf{{Max Err}} \\\\
 & (KB) & (s) & (\\%) & (\\%) \\\\
\\middlehline
"""

    for impl in implementations:
        name = impl["name"]
        mem_kb = impl["memory_bytes"] // 1024
        insert_time = impl.get("insert_time_sec", 0)
        mean_err = impl.get("mean_error_pct", 0)
        max_err = impl.get("max_error_pct", 0)
        latex += f"{name} & {mem_kb} & {insert_time:.3f} & {mean_err:.2f} & {max_err:.0f} \\\\\n"

    latex += """\\bottomhline
\\end{tabular}
\\end{table}
"""

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with open(output_path, 'w') as f:
        f.write(latex)
    print(f"Generated: {output_path}")


def find_latest_json(directory: Path, pattern: str) -> Path | None:
    """Find the most recently modified JSON file matching pattern."""
    matches = list(directory.glob(pattern))
    if not matches:
        return None
    return max(matches, key=lambda p: p.stat().st_mtime)


def main():
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    # Table 1a: Multi-category fair comparison at 1024 KB
    multicategory_1024_json = RESULTS_DIR / "multicategory_fair" / "fair_compare_1024kb_gdelt_multicategory.json"
    if multicategory_1024_json.exists():
        generate_multicategory_table(
            multicategory_1024_json,
            OUTPUT_DIR / "tab_multicategory_1024kb.tex",
            label_suffix="_1024kb"
        )
    else:
        print("Warning: No multicategory_fair 1024KB results found")

    # Table 1b: Multi-category fair comparison at 512 KB
    multicategory_512_json = RESULTS_DIR / "multicategory_fair" / "fair_compare_512kb_gdelt_multicategory.json"
    if multicategory_512_json.exists():
        generate_multicategory_table(
            multicategory_512_json,
            OUTPUT_DIR / "tab_multicategory_512kb.tex",
            label_suffix="_512kb"
        )
    else:
        print("Warning: No multicategory_fair 512KB results found")

    # Also generate a combined/default table from the most recent
    multicategory_json = find_latest_json(
        RESULTS_DIR / "multicategory_fair",
        "fair_compare_*_gdelt_multicategory.json"
    )
    if multicategory_json:
        generate_multicategory_table(
            multicategory_json,
            OUTPUT_DIR / "tab_multicategory.tex"
        )
    else:
        print("Warning: No multicategory_fair results found")

    # Table 2: Time-series evolution
    timeseries_json = find_latest_json(
        RESULTS_DIR / "timeseries",
        "timeseries_*.json"
    )
    if timeseries_json:
        generate_timeseries_table(
            timeseries_json,
            OUTPUT_DIR / "tab_timeseries.tex"
        )
    else:
        print("Warning: No timeseries results found")

    # Table 3: NYC Taxi
    taxi_json = find_latest_json(
        RESULTS_DIR / "taxi",
        "taxi_benchmark_*.json"
    )
    if taxi_json:
        generate_taxi_table(
            taxi_json,
            OUTPUT_DIR / "tab_taxi.tex"
        )
    else:
        print("Warning: No taxi results found")

    print(f"\nAll tables written to {OUTPUT_DIR}/")
    print("Include in LaTeX with: \\input{tables/tab_multicategory.tex}")


if __name__ == "__main__":
    main()
