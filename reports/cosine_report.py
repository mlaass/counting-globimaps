#!/usr/bin/env python3
"""
Cosine Distribution Report Generator

Generates markdown report analyzing performance on synthetic cosine
distribution data with deterministic patterns.
"""

from report_utils import (
    check_results_available,
    write_report,
    generate_placeholder_report,
)


EXPERIMENT_TYPE = "cosine"
REPORT_FILENAME = "cosine_report.md"
PDF_FILENAME = "cosine_figures.pdf"


def main():
    """Main entry point."""
    print(f"Generating {EXPERIMENT_TYPE} report...")

    # Check for data
    if not check_results_available(EXPERIMENT_TYPE):
        report = generate_placeholder_report(
            "Cosine Distribution Analysis",
            """This report analyzes implementation performance on synthetic data
with a deterministic cosine distribution pattern.

The cosine distribution creates a known spatial pattern that allows precise
validation of cardinality estimation accuracy since ground truth is known exactly.

**Test Configuration:**
- 65,536 inserts (256x256 grid)
- Cosine-weighted distribution creating hotspots
- Exact ground truth for error calculation""",
            """```bash
# From project root
./run_all_experiments.sh

# Or run specific experiment from build/
./build/globimap_test_cos_compare
```

Results will be saved to `results/cosine/`."""
        )
        write_report(REPORT_FILENAME, report)
        print("  Generated placeholder (no data available)")
        return

    # TODO: Implement full report when results are available
    report = generate_placeholder_report(
        "Cosine Distribution Analysis",
        "Results found but full report generation not yet implemented.",
        "Run experiments again to generate fresh results."
    )
    write_report(REPORT_FILENAME, report)
    print("  Done!")


if __name__ == "__main__":
    main()
