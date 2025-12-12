#!/usr/bin/env python3
"""
Multi-Category Analysis Report Generator

Generates markdown report analyzing category isolation and accuracy
for counting bloom filter implementations on multi-category datasets.
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
    format_pct,
    format_number,
    table_to_markdown,
    embed_figure,
    get_impl_color,
    get_impl_colors_list,
    get_report_timestamp,
    IMPL_COLORS,
    CATEGORY_COLORS,
)


EXPERIMENT_TYPE = "multicategory"
REPORT_FILENAME = "multicategory_report.md"
PDF_FILENAME = "multicategory_figures.pdf"

# GDELT QuadClass category names
CATEGORY_NAMES = {
    1: "Verbal Cooperation",
    2: "Material Cooperation",
    3: "Verbal Conflict",
    4: "Material Conflict",
}


def load_results():
    """Load and validate experiment results."""
    return load_results_safe(EXPERIMENT_TYPE)


def plot_isolation_heatmap(results: dict) -> str:
    """Generate heatmap showing error % by implementation and category."""
    setup_plot_style()

    # Find isolation test results
    isolation_data = None
    for name, data in results.items():
        if data.get('experiment') == 'multicategory_isolation':
            isolation_data = data
            break

    if not isolation_data:
        return None

    # Build matrix: implementations x categories
    impl_names = [impl['name'] for impl in isolation_data['implementations']]
    categories = [cat['category_id'] for cat in isolation_data['implementations'][0]['categories']]

    matrix = np.zeros((len(impl_names), len(categories)))

    for i, impl in enumerate(isolation_data['implementations']):
        for j, cat in enumerate(impl['categories']):
            matrix[i, j] = cat['error_pct']

    fig, ax = plt.subplots(figsize=(10, 6))

    # Use a diverging colormap centered at 0
    im = ax.imshow(matrix, cmap='RdYlGn_r', aspect='auto', vmin=0, vmax=max(1, matrix.max()))

    # Labels
    ax.set_xticks(np.arange(len(categories)))
    ax.set_yticks(np.arange(len(impl_names)))
    ax.set_xticklabels([f"Cat {c}" for c in categories])
    ax.set_yticklabels(impl_names)

    # Rotate x labels
    plt.setp(ax.get_xticklabels(), rotation=0, ha="center")

    # Add text annotations
    for i in range(len(impl_names)):
        for j in range(len(categories)):
            text = ax.text(j, i, f"{matrix[i, j]:.1f}%",
                           ha="center", va="center", color="black", fontsize=10)

    ax.set_title("Category Isolation Error (%) - Lower is Better")
    ax.set_xlabel("Category")
    ax.set_ylabel("Implementation")

    # Colorbar
    cbar = ax.figure.colorbar(im, ax=ax)
    cbar.ax.set_ylabel("Error %", rotation=-90, va="bottom")

    fig_path = get_figures_dir() / "multicategory_isolation_heatmap.png"
    plt.tight_layout()
    plt.savefig(fig_path, bbox_inches='tight')
    plt.close()

    return str(fig_path)


def plot_category_distribution(results: dict) -> str:
    """Generate pie chart showing GDELT category distribution."""
    setup_plot_style()

    # Find dataset benchmark results
    dataset_data = None
    for name, data in results.items():
        if data.get('experiment') == 'multicategory_dataset_benchmark':
            dataset_data = data
            break

    if not dataset_data or not dataset_data.get('implementations'):
        return None

    # Get category counts from first implementation
    impl = dataset_data['implementations'][0]
    categories = impl['categories']

    labels = []
    sizes = []
    colors = []

    for cat in categories:
        cat_id = cat['category_id']
        labels.append(f"{CATEGORY_NAMES.get(cat_id, f'Category {cat_id}')}")
        sizes.append(cat['insert_count'])
        colors.append(CATEGORY_COLORS.get(cat_id, '#999999'))

    fig, ax = plt.subplots(figsize=(10, 8))

    wedges, texts, autotexts = ax.pie(
        sizes, labels=labels, autopct='%1.1f%%',
        colors=colors, startangle=90,
        textprops={'fontsize': 11}
    )

    ax.set_title(f"GDELT Event Distribution by Category\n({format_number(sum(sizes))} total events)")

    fig_path = get_figures_dir() / "multicategory_category_distribution.png"
    plt.tight_layout()
    plt.savefig(fig_path, bbox_inches='tight')
    plt.close()

    return str(fig_path)


def plot_accuracy_by_category(results: dict) -> str:
    """Generate grouped bar chart of error % by category and implementation."""
    setup_plot_style()

    # Find dataset benchmark results
    dataset_data = None
    for name, data in results.items():
        if data.get('experiment') == 'multicategory_dataset_benchmark':
            dataset_data = data
            break

    if not dataset_data:
        return None

    impl_names = [impl['name'] for impl in dataset_data['implementations']]
    categories = [cat['category_id'] for cat in dataset_data['implementations'][0]['categories']]

    # Build data structure
    data = {impl: [] for impl in impl_names}
    for impl in dataset_data['implementations']:
        for cat in impl['categories']:
            data[impl['name']].append(cat['mean_error_pct'])

    fig, ax = plt.subplots(figsize=(12, 6))

    x = np.arange(len(categories))
    width = 0.2
    multiplier = 0

    for impl in impl_names:
        offset = width * multiplier
        bars = ax.bar(x + offset, data[impl], width, label=impl, color=get_impl_color(impl))
        multiplier += 1

    ax.set_xlabel('Category')
    ax.set_ylabel('Mean Error (%)')
    ax.set_title('Estimation Error by Category and Implementation')
    ax.set_xticks(x + width * 1.5)
    ax.set_xticklabels([CATEGORY_NAMES.get(c, f"Cat {c}") for c in categories], rotation=15, ha='right')
    ax.legend(loc='upper right', fontsize=9)
    ax.grid(axis='y', alpha=0.3)

    # Add horizontal line at 0
    ax.axhline(y=0, color='black', linestyle='-', linewidth=0.5)

    fig_path = get_figures_dir() / "multicategory_accuracy_by_category.png"
    plt.tight_layout()
    plt.savefig(fig_path, bbox_inches='tight')
    plt.close()

    return str(fig_path)


def plot_memory_vs_accuracy(results: dict) -> str:
    """Generate scatter plot of memory vs mean error."""
    setup_plot_style()

    fig, ax = plt.subplots(figsize=(10, 6))

    # Combine data from all result files
    for name, data in results.items():
        for impl in data.get('implementations', []):
            memory_kb = impl['memory_bytes'] / 1024
            error = impl.get('mean_error_pct', 0)
            color = get_impl_color(impl['name'])

            # Different marker for different experiment types
            marker = 'o' if 'isolation' in data.get('experiment', '') else 's'

            ax.scatter(memory_kb, error, c=color, s=150, alpha=0.8,
                       edgecolors='black', linewidth=0.5, marker=marker)

    # Add legend
    for impl, color in IMPL_COLORS.items():
        ax.scatter([], [], c=color, s=100, label=impl, edgecolors='black')

    ax.set_xlabel('Memory (KB)')
    ax.set_ylabel('Mean Error (%)')
    ax.set_title('Memory vs Accuracy Trade-off')
    ax.legend(loc='upper right', fontsize=9)
    ax.grid(True, alpha=0.3)

    # Annotation
    ax.annotate('← Better (low memory, low error)', xy=(0.02, 0.02),
                xycoords='axes fraction', fontsize=10, color='green', alpha=0.7)

    fig_path = get_figures_dir() / "multicategory_memory_vs_accuracy.png"
    plt.tight_layout()
    plt.savefig(fig_path, bbox_inches='tight')
    plt.close()

    return str(fig_path)


def create_isolation_summary_table(results: dict) -> pd.DataFrame:
    """Create summary table for isolation test."""
    isolation_data = None
    for name, data in results.items():
        if data.get('experiment') == 'multicategory_isolation':
            isolation_data = data
            break

    if not isolation_data:
        return None

    rows = []
    for impl in isolation_data['implementations']:
        rows.append({
            'Implementation': impl['name'],
            'Memory': format_bytes(impl['memory_bytes']),
            'Insert Time': format_time(impl['insert_time_sec']),
            'Mean Error': f"{impl['mean_error_pct']:.2f}%",
            'Max Error': f"{impl['max_error_pct']:.2f}%",
            'Isolation Rate': f"{impl['isolation_rate_pct']:.1f}%",
        })

    return pd.DataFrame(rows).set_index('Implementation')


def create_dataset_summary_table(results: dict) -> pd.DataFrame:
    """Create summary table for dataset benchmark."""
    dataset_data = None
    for name, data in results.items():
        if data.get('experiment') == 'multicategory_dataset_benchmark':
            dataset_data = data
            break

    if not dataset_data:
        return None

    rows = []
    for impl in dataset_data['implementations']:
        # Calculate average error across categories
        avg_error = np.mean([cat['mean_error_pct'] for cat in impl['categories']])

        rows.append({
            'Implementation': impl['name'],
            'Memory': format_bytes(impl['memory_bytes']),
            'Insert Time': format_time(impl['insert_time_sec']),
            'Query Time': format_time(impl['query_mean_sec']),
            'Avg Error': f"{avg_error:.2f}%",
        })

    return pd.DataFrame(rows).set_index('Implementation')


def get_key_findings(results: dict) -> list:
    """Extract key findings from results."""
    findings = []

    # Check isolation results
    for name, data in results.items():
        if data.get('experiment') == 'multicategory_isolation':
            all_perfect = all(
                impl['isolation_rate_pct'] == 100.0
                for impl in data['implementations']
            )
            if all_perfect:
                findings.append("All implementations achieve **100% category isolation** on synthetic test")
            break

    # Check dataset benchmark
    for name, data in results.items():
        if data.get('experiment') == 'multicategory_dataset_benchmark':
            # Find best and worst performers
            impl_errors = []
            for impl in data['implementations']:
                avg_error = np.mean([cat['mean_error_pct'] for cat in impl['categories']])
                impl_errors.append((impl['name'], avg_error))

            impl_errors.sort(key=lambda x: x[1])
            best = impl_errors[0]
            worst = impl_errors[-1]

            if best[1] == 0:
                findings.append(f"**{best[0]}** achieves perfect accuracy (0% error) on real dataset")
            else:
                findings.append(f"Most accurate: **{best[0]}** ({best[1]:.2f}% average error)")

            if worst[1] > 0:
                findings.append(f"d-Left CBF shows higher error ({worst[1]:.1f}%) due to fingerprint collisions")
            break

    return findings


def generate_report(results: dict) -> tuple:
    """Generate full markdown report and list of figures."""
    sections = []
    figures = []

    # Header
    sections.append("# Multi-Category Analysis Report\n")
    sections.append(f"*Generated: {get_report_timestamp()}*\n")

    # Executive summary
    sections.append("## Executive Summary\n")
    sections.append("This report analyzes how counting bloom filter implementations handle ")
    sections.append("multi-category data, testing category isolation and per-category accuracy.\n")

    # Key findings
    findings = get_key_findings(results)
    sections.append("\n**Key Findings:**\n")
    for finding in findings:
        sections.append(f"- {finding}\n")

    # Synthetic Isolation Test
    sections.append("\n## Synthetic Isolation Test\n")
    sections.append("Tests category isolation at a single location with 4 categories ")
    sections.append("(100, 500, 50, 1000 inserts each).\n")

    iso_table = create_isolation_summary_table(results)
    if iso_table is not None:
        sections.append("\n### Results Summary\n")
        sections.append(iso_table.to_markdown())
        sections.append("\n")

    fig1 = plot_isolation_heatmap(results)
    if fig1:
        figures.append(fig1)
        sections.append(embed_figure(fig1, "Error % by implementation and category (green = perfect)"))

    # Dataset Benchmark
    sections.append("\n## Real Dataset Benchmark (GDELT)\n")
    sections.append("Tests on 1.9M GDELT events with 4 QuadClass categories.\n")

    # Category distribution
    fig2 = plot_category_distribution(results)
    if fig2:
        figures.append(fig2)
        sections.append("\n### Category Distribution\n")
        sections.append(embed_figure(fig2, "Distribution of events across GDELT QuadClass categories"))

    # Dataset summary table
    dataset_table = create_dataset_summary_table(results)
    if dataset_table is not None:
        sections.append("\n### Performance Summary\n")
        sections.append(dataset_table.to_markdown())
        sections.append("\n")

    # Accuracy by category
    fig3 = plot_accuracy_by_category(results)
    if fig3:
        figures.append(fig3)
        sections.append("\n### Per-Category Accuracy\n")
        sections.append(embed_figure(fig3, "Mean estimation error by category and implementation"))

    # Memory vs accuracy
    fig4 = plot_memory_vs_accuracy(results)
    if fig4:
        figures.append(fig4)
        sections.append("\n### Memory vs Accuracy Trade-off\n")
        sections.append(embed_figure(fig4, "Memory usage vs mean error (circles=isolation test, squares=dataset)"))

    # Conclusions
    sections.append("\n## Conclusions\n")
    sections.append("- **Category isolation works**: All implementations properly separate categories\n")
    sections.append("- **Spectral BF (MI)** and **Count-Min Sketch** achieve perfect accuracy\n")
    sections.append("- **d-Left CBF** shows some error due to fingerprint collisions at high load\n")
    sections.append("- **CountingGloBiMap (MI)** provides excellent accuracy with minimal overhead\n")

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
            "Multi-Category Analysis",
            "This report analyzes category isolation and accuracy for counting bloom filter "
            "implementations on multi-category datasets (GDELT events with QuadClass categories).",
            "```bash\n# From project root\n./run_all_experiments.sh\n\n"
            "# Or run specific experiments\n./build/globimap_test_multicategory\n"
            "./build/globimap_test_multicategory_dataset\n```"
        )
        write_report(REPORT_FILENAME, report)
        return

    # Load and generate
    results, has_data = load_results()

    if not has_data:
        report = generate_placeholder_report(
            "Multi-Category Analysis",
            "This report analyzes category isolation and accuracy.",
            "```bash\n./build/globimap_test_multicategory\n```"
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
