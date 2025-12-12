#!/usr/bin/env python3
"""
Dataset Comparison Report Generator

Generates markdown report with embedded figures analyzing performance
of counting bloom filter implementations on real-world datasets.
"""

import sys
from pathlib import Path

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

from report_utils import (
    setup_plot_style,
    load_results_safe,
    check_results_available,
    get_figures_dir,
    write_report,
    generate_placeholder_report,
    save_figures_to_pdf,
    format_bytes,
    format_time,
    table_to_markdown,
    embed_figure,
    get_impl_color,
    get_impl_colors_list,
    get_report_timestamp,
    IMPL_COLORS,
)


EXPERIMENT_TYPE = "dataset_comparison"
REPORT_FILENAME = "dataset_comparison_report.md"
PDF_FILENAME = "dataset_comparison_figures.pdf"


def load_results():
    """Load and validate experiment results."""
    return load_results_safe(EXPERIMENT_TYPE)


def create_summary_table(results: dict) -> pd.DataFrame:
    """Create summary table with all metrics."""
    rows = []

    for result_name, result_data in results.items():
        dataset = result_data.get('dataset', result_name)

        for impl in result_data.get('implementations', []):
            rows.append({
                'Dataset': dataset.replace('_', ' ').title(),
                'Implementation': impl['name'],
                'Memory': format_bytes(impl['memory_bytes']),
                'Memory (bytes)': impl['memory_bytes'],
                'Insert Time': format_time(impl['insert_time_sec']),
                'Insert (s)': impl['insert_time_sec'],
                'Query Mean': format_time(impl['query_time_mean_sec']),
                'Query (μs)': impl['query_time_mean_sec'] * 1e6,
            })

    df = pd.DataFrame(rows)
    return df


def plot_memory_comparison(results: dict) -> str:
    """Generate grouped bar chart of memory usage by implementation."""
    setup_plot_style()

    # Extract data
    datasets = []
    impl_names = list(IMPL_COLORS.keys())
    data = {impl: [] for impl in impl_names}

    for result_name, result_data in sorted(results.items()):
        dataset = result_data.get('dataset', result_name).replace('_', ' ').title()
        datasets.append(dataset)

        impl_memory = {impl['name']: impl['memory_bytes'] / 1024
                       for impl in result_data['implementations']}

        for impl in impl_names:
            data[impl].append(impl_memory.get(impl, 0))

    # Create plot
    fig, ax = plt.subplots(figsize=(10, 6))

    x = np.arange(len(datasets))
    width = 0.2
    multiplier = 0

    for impl, values in data.items():
        offset = width * multiplier
        bars = ax.bar(x + offset, values, width, label=impl, color=get_impl_color(impl))
        multiplier += 1

    ax.set_xlabel('Dataset')
    ax.set_ylabel('Memory (KB)')
    ax.set_title('Memory Usage by Implementation')
    ax.set_xticks(x + width * 1.5)
    ax.set_xticklabels(datasets)
    ax.legend(loc='upper right', fontsize=9)
    ax.grid(axis='y', alpha=0.3)

    fig_path = get_figures_dir() / "dataset_comparison_memory.png"
    plt.tight_layout()
    plt.savefig(fig_path, bbox_inches='tight')
    plt.close()

    return str(fig_path)


def plot_insert_time_comparison(results: dict) -> str:
    """Generate grouped bar chart of insert time by dataset."""
    setup_plot_style()

    datasets = []
    impl_names = list(IMPL_COLORS.keys())
    data = {impl: [] for impl in impl_names}

    for result_name, result_data in sorted(results.items()):
        dataset = result_data.get('dataset', result_name).replace('_', ' ').title()
        datasets.append(dataset)

        impl_times = {impl['name']: impl['insert_time_sec']
                      for impl in result_data['implementations']}

        for impl in impl_names:
            data[impl].append(impl_times.get(impl, 0))

    fig, ax = plt.subplots(figsize=(10, 6))

    x = np.arange(len(datasets))
    width = 0.2
    multiplier = 0

    for impl, values in data.items():
        offset = width * multiplier
        bars = ax.bar(x + offset, values, width, label=impl, color=get_impl_color(impl))
        multiplier += 1

    ax.set_xlabel('Dataset')
    ax.set_ylabel('Insert Time (seconds)')
    ax.set_title('Insert Time by Implementation')
    ax.set_xticks(x + width * 1.5)
    ax.set_xticklabels(datasets)
    ax.legend(loc='upper left', fontsize=9)
    ax.grid(axis='y', alpha=0.3)

    fig_path = get_figures_dir() / "dataset_comparison_insert_time.png"
    plt.tight_layout()
    plt.savefig(fig_path, bbox_inches='tight')
    plt.close()

    return str(fig_path)


def plot_query_time_comparison(results: dict) -> str:
    """Generate bar chart with error bars (min/mean/max) for query time."""
    setup_plot_style()

    # Combine all datasets for this plot
    impl_data = {}

    for result_name, result_data in results.items():
        for impl in result_data['implementations']:
            name = impl['name']
            if name not in impl_data:
                impl_data[name] = {'mean': [], 'min': [], 'max': []}

            impl_data[name]['mean'].append(impl['query_time_mean_sec'] * 1e6)
            impl_data[name]['min'].append(impl['query_time_min_sec'] * 1e6)
            impl_data[name]['max'].append(impl['query_time_max_sec'] * 1e6)

    # Average across datasets
    impl_names = list(IMPL_COLORS.keys())
    means = []
    mins = []
    maxs = []

    for impl in impl_names:
        if impl in impl_data:
            means.append(np.mean(impl_data[impl]['mean']))
            mins.append(np.mean(impl_data[impl]['min']))
            maxs.append(np.mean(impl_data[impl]['max']))
        else:
            means.append(0)
            mins.append(0)
            maxs.append(0)

    fig, ax = plt.subplots(figsize=(10, 6))

    x = np.arange(len(impl_names))
    colors = get_impl_colors_list(impl_names)

    # Error bars: yerr expects (lower_errors, upper_errors)
    lower_errors = [m - mi for m, mi in zip(means, mins)]
    upper_errors = [ma - m for m, ma in zip(means, maxs)]

    bars = ax.bar(x, means, color=colors, edgecolor='black', linewidth=0.5)
    ax.errorbar(x, means, yerr=[lower_errors, upper_errors],
                fmt='none', ecolor='black', capsize=5, capthick=1.5)

    ax.set_xlabel('Implementation')
    ax.set_ylabel('Query Time (μs)')
    ax.set_title('Query Time Comparison (averaged across datasets)')
    ax.set_xticks(x)
    ax.set_xticklabels(impl_names, rotation=15, ha='right')
    ax.grid(axis='y', alpha=0.3)

    fig_path = get_figures_dir() / "dataset_comparison_query_time.png"
    plt.tight_layout()
    plt.savefig(fig_path, bbox_inches='tight')
    plt.close()

    return str(fig_path)


def plot_efficiency_scatter(results: dict) -> str:
    """Generate scatter plot: memory vs query time (Pareto front analysis)."""
    setup_plot_style()

    fig, ax = plt.subplots(figsize=(10, 6))

    # Plot each implementation across all datasets
    for result_name, result_data in results.items():
        dataset = result_data.get('dataset', result_name)

        for impl in result_data['implementations']:
            memory_kb = impl['memory_bytes'] / 1024
            query_us = impl['query_time_mean_sec'] * 1e6
            color = get_impl_color(impl['name'])

            ax.scatter(memory_kb, query_us, c=color, s=120, alpha=0.8,
                       edgecolors='black', linewidth=0.5)

    # Add legend
    for impl, color in IMPL_COLORS.items():
        ax.scatter([], [], c=color, s=100, label=impl, edgecolors='black')

    ax.set_xlabel('Memory (KB)')
    ax.set_ylabel('Query Time (μs)')
    ax.set_title('Memory vs Query Time Trade-off')
    ax.legend(loc='upper right', fontsize=9)
    ax.grid(True, alpha=0.3)

    # Add Pareto frontier annotation
    ax.annotate('← Better', xy=(0.02, 0.02), xycoords='axes fraction',
                fontsize=10, color='green', alpha=0.7)

    fig_path = get_figures_dir() / "dataset_comparison_efficiency.png"
    plt.tight_layout()
    plt.savefig(fig_path, bbox_inches='tight')
    plt.close()

    return str(fig_path)


def get_key_findings(results: dict) -> list:
    """Extract key findings from results."""
    findings = []

    # Aggregate across datasets
    impl_metrics = {}

    for result_data in results.values():
        for impl in result_data['implementations']:
            name = impl['name']
            if name not in impl_metrics:
                impl_metrics[name] = {'memory': [], 'insert': [], 'query': []}

            impl_metrics[name]['memory'].append(impl['memory_bytes'])
            impl_metrics[name]['insert'].append(impl['insert_time_sec'])
            impl_metrics[name]['query'].append(impl['query_time_mean_sec'])

    # Find best performers
    avg_memory = {k: np.mean(v['memory']) for k, v in impl_metrics.items()}
    avg_query = {k: np.mean(v['query']) for k, v in impl_metrics.items()}
    avg_insert = {k: np.mean(v['insert']) for k, v in impl_metrics.items()}

    best_memory = min(avg_memory, key=avg_memory.get)
    best_query = min(avg_query, key=avg_query.get)
    best_insert = min(avg_insert, key=avg_insert.get)

    findings.append(f"**Most memory efficient**: {best_memory} ({format_bytes(int(avg_memory[best_memory]))} average)")
    findings.append(f"**Fastest queries**: {best_query} ({format_time(avg_query[best_query])} average)")
    findings.append(f"**Fastest inserts**: {best_insert} ({format_time(avg_insert[best_insert])} average)")

    return findings


def generate_report(results: dict) -> tuple:
    """Generate full markdown report and list of figures."""
    sections = []
    figures = []

    # Header
    sections.append("# Dataset Comparison Report\n")
    sections.append(f"*Generated: {get_report_timestamp()}*\n")

    # Count datasets and implementations
    num_datasets = len(results)
    num_impls = len(results[list(results.keys())[0]]['implementations']) if results else 0

    sections.append("## Executive Summary\n")
    sections.append(f"This report analyzes {num_impls} counting bloom filter implementations ")
    sections.append(f"across {num_datasets} real-world datasets.\n")

    # Key findings
    findings = get_key_findings(results)
    sections.append("\n**Key Findings:**\n")
    for finding in findings:
        sections.append(f"- {finding}\n")

    # Datasets analyzed
    sections.append("\n## Datasets Analyzed\n")
    datasets_info = []
    for result_name, result_data in sorted(results.items()):
        dataset = result_data.get('dataset', result_name)
        width = result_data.get('width', 'N/A')
        height = result_data.get('height', 'N/A')
        datasets_info.append(f"- **{dataset.replace('_', ' ').title()}**: {width}x{height} grid")
    sections.append("\n".join(datasets_info) + "\n")

    # Memory comparison
    sections.append("\n## Memory Usage\n")
    fig1 = plot_memory_comparison(results)
    figures.append(fig1)
    sections.append(embed_figure(fig1, "Memory usage comparison across implementations"))

    # Insert time comparison
    sections.append("\n## Insert Performance\n")
    fig2 = plot_insert_time_comparison(results)
    figures.append(fig2)
    sections.append(embed_figure(fig2, "Insert time comparison (lower is better)"))

    # Query time comparison
    sections.append("\n## Query Performance\n")
    fig3 = plot_query_time_comparison(results)
    figures.append(fig3)
    sections.append(embed_figure(fig3, "Query time with min/max error bars (lower is better)"))

    # Efficiency trade-off
    sections.append("\n## Memory vs Speed Trade-off\n")
    fig4 = plot_efficiency_scatter(results)
    figures.append(fig4)
    sections.append(embed_figure(fig4, "Memory vs query time (bottom-left is optimal)"))

    # Summary table
    sections.append("\n## Detailed Results\n")
    df = create_summary_table(results)
    display_df = df[['Dataset', 'Implementation', 'Memory', 'Insert Time', 'Query Mean']].copy()
    display_df = display_df.set_index(['Dataset', 'Implementation'])
    sections.append(display_df.to_markdown())
    sections.append("\n")

    # Recommendations
    sections.append("\n## Recommendations\n")
    sections.append("Based on the analysis:\n")
    sections.append("- **For memory-constrained applications**: Count-Min Sketch offers the smallest footprint\n")
    sections.append("- **For query-heavy workloads**: Spectral BF (MI) provides fastest queries\n")
    sections.append("- **For balanced performance**: d-Left CBF offers good memory/speed trade-off\n")
    sections.append("- **For accuracy with cascading**: CountingGloBiMap (MI) handles varying count magnitudes\n")

    # Footer
    sections.append("\n---\n")
    sections.append(f"*Report generated: {get_report_timestamp()}*\n")

    return "\n".join(sections), figures


def main():
    """Main entry point."""
    print(f"Generating {EXPERIMENT_TYPE} report...")

    # Check for data
    if not check_results_available(EXPERIMENT_TYPE):
        report = generate_placeholder_report(
            "Dataset Comparison",
            "This report compares counting bloom filter implementations on real-world datasets "
            "(GDELT events, COVID-19 cases) measuring memory usage, insert time, and query performance.",
            "```bash\n# From project root\n./run_all_experiments.sh\n\n"
            "# Or run specific experiment\n./build/globimap_test_dataset_compare\n```"
        )
        write_report(REPORT_FILENAME, report)
        return

    # Load and generate
    results, has_data = load_results()

    if not has_data:
        report = generate_placeholder_report(
            "Dataset Comparison",
            "This report compares counting bloom filter implementations on real-world datasets.",
            "```bash\n./build/globimap_test_dataset_compare\n```"
        )
        write_report(REPORT_FILENAME, report)
        return

    report_content, figures = generate_report(results)
    write_report(REPORT_FILENAME, report_content)

    # Generate PDF with all figures
    save_figures_to_pdf(figures, PDF_FILENAME)

    print("  Done!")


if __name__ == "__main__":
    main()
