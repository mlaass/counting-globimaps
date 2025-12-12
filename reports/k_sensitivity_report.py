#!/usr/bin/env python3
"""
K Parameter Sensitivity Report Generator

Generates markdown report analyzing the impact of k (number of hash functions)
on counting bloom filter performance and accuracy.
"""

from report_utils import (
    check_results_available,
    write_report,
    generate_placeholder_report,
)


EXPERIMENT_TYPE = "k_sensitivity"
REPORT_FILENAME = "k_sensitivity_report.md"
PDF_FILENAME = "k_sensitivity_figures.pdf"


def main():
    """Main entry point."""
    print(f"Generating {EXPERIMENT_TYPE} report...")

    # Check for data
    if not check_results_available(EXPERIMENT_TYPE):
        report = generate_placeholder_report(
            "K Parameter Sensitivity Analysis",
            """This report analyzes how the number of hash functions (k) affects:
- **Accuracy**: False positive rate and estimation error
- **Performance**: Insert and query time
- **Memory efficiency**: Trade-offs at different k values

The analysis sweeps k from 1 to 32 across multiple implementations to identify
optimal values for different use cases.""",
            """```bash
# From project root
./run_all_experiments.sh

# Or run specific experiment from build/
./build/globimap_test_k_compare

# Alternative: test with specific datasets
./build/globimap_test_datasets_for_k
```

Results will be saved to `results/k_sensitivity/`."""
        )
        write_report(REPORT_FILENAME, report)
        print("  Generated placeholder (no data available)")
        return

    # TODO: Implement full report when results are available
    # For now, generate placeholder since k_sensitivity is empty
    report = generate_placeholder_report(
        "K Parameter Sensitivity Analysis",
        "Results found but full report generation not yet implemented.",
        "Run experiments again to generate fresh results."
    )
    write_report(REPORT_FILENAME, report)
    print("  Done!")


if __name__ == "__main__":
    main()
