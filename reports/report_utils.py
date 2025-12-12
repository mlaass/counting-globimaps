"""
Common utilities for report generation.

This module provides helpers for:
- Path resolution across project
- Results loading with empty-directory handling
- Figure styling and generation
- Markdown formatting
- PDF compilation
"""

import sys
import json
from pathlib import Path
from datetime import datetime
from typing import Dict, List, Tuple, Optional

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.image as mpimg
from matplotlib.backends.backend_pdf import PdfPages
import seaborn as sns


# =============================================================================
# Path Helpers
# =============================================================================

def get_project_root() -> Path:
    """Return absolute path to project root."""
    return Path(__file__).parent.parent


def get_results_dir(experiment_type: str) -> Path:
    """Return path to results subdirectory."""
    return get_project_root() / "results" / experiment_type


def get_figures_dir() -> Path:
    """Return path to figures output directory, creating if needed."""
    figures_dir = Path(__file__).parent / "figures"
    figures_dir.mkdir(exist_ok=True)
    return figures_dir


def get_output_dir() -> Path:
    """Return path to report output directory, creating if needed."""
    output_dir = Path(__file__).parent / "output"
    output_dir.mkdir(exist_ok=True)
    return output_dir


# =============================================================================
# Results Loading (wrapping notebooks/utils.py)
# =============================================================================

# Import from notebooks/utils.py
sys.path.insert(0, str(get_project_root() / "notebooks"))
from utils import load_experiment_results, discover_datasets, get_dataset_metadata


def load_results_safe(experiment_type: str) -> Tuple[Dict, bool]:
    """
    Load results with empty-directory handling.

    Returns:
        Tuple of (results_dict, has_data)
    """
    results_dir = get_results_dir(experiment_type)
    results = load_experiment_results(str(results_dir))
    has_data = len(results) > 0
    return results, has_data


def check_results_available(experiment_type: str) -> bool:
    """Check if any JSON results exist for this experiment type."""
    results_dir = get_results_dir(experiment_type)
    if not results_dir.exists():
        return False
    return len(list(results_dir.glob("*.json"))) > 0


# =============================================================================
# Visualization Styling
# =============================================================================

# Consistent color scheme across all reports
COLORS = ["#F72585", "#7209B7", "#3A0CA3", "#4361EE", "#4CC9F0"]

IMPL_COLORS = {
    "Spectral BF (MI)": "#F72585",
    "d-Left CBF": "#7209B7",
    "Count-Min Sketch": "#3A0CA3",
    "CountingGloBiMap (MI)": "#4361EE",
}

# Category colors for multi-category reports
CATEGORY_COLORS = {
    1: "#2ecc71",  # Green - Verbal Cooperation
    2: "#3498db",  # Blue - Material Cooperation
    3: "#f39c12",  # Orange - Verbal Conflict
    4: "#e74c3c",  # Red - Material Conflict
}


def setup_plot_style():
    """Configure matplotlib/seaborn for consistent report styling."""
    sns.set_style('whitegrid')
    plt.rcParams['figure.figsize'] = (10, 6)
    plt.rcParams['figure.dpi'] = 150
    plt.rcParams['savefig.dpi'] = 150
    plt.rcParams['font.size'] = 11
    plt.rcParams['axes.titlesize'] = 13
    plt.rcParams['axes.labelsize'] = 11


def get_impl_color(impl_name: str) -> str:
    """Get consistent color for implementation."""
    return IMPL_COLORS.get(impl_name, "#999999")


def get_impl_colors_list(impl_names: List[str]) -> List[str]:
    """Get list of colors matching implementation names."""
    return [get_impl_color(name) for name in impl_names]


# =============================================================================
# Formatting Helpers
# =============================================================================

def format_bytes(b: int) -> str:
    """Format bytes as human-readable (KB, MB)."""
    if b >= 1024 * 1024:
        return f"{b / (1024*1024):.2f} MB"
    elif b >= 1024:
        return f"{b / 1024:.2f} KB"
    return f"{b} B"


def format_time(sec: float) -> str:
    """Format seconds with appropriate precision."""
    if sec < 0.001:
        return f"{sec * 1e6:.2f} μs"
    elif sec < 1:
        return f"{sec * 1000:.2f} ms"
    return f"{sec:.3f} s"


def format_number(n: float) -> str:
    """Format large numbers with K/M suffixes."""
    if n >= 1e6:
        return f"{n/1e6:.2f}M"
    elif n >= 1e3:
        return f"{n/1e3:.1f}K"
    return f"{n:.0f}"


def format_pct(pct: float) -> str:
    """Format percentage with appropriate precision."""
    if pct == 0:
        return "0%"
    elif pct < 0.01:
        return f"{pct:.4f}%"
    elif pct < 1:
        return f"{pct:.2f}%"
    return f"{pct:.1f}%"


# =============================================================================
# Markdown Generation
# =============================================================================

def table_to_markdown(df: pd.DataFrame, title: Optional[str] = None) -> str:
    """Convert pandas DataFrame to markdown table."""
    lines = []
    if title:
        lines.append(f"### {title}\n")
    lines.append(df.to_markdown(index=True))
    lines.append("")
    return "\n".join(lines)


def embed_figure(fig_path: str, caption: str, alt_text: str = "") -> str:
    """Generate markdown for embedded figure."""
    rel_path = f"figures/{Path(fig_path).name}"
    return f"![{alt_text or caption}]({rel_path})\n\n*{caption}*\n"


def section_header(level: int, title: str) -> str:
    """Generate markdown section header."""
    return f"{'#' * level} {title}\n"


def bullet_list(items: List[str]) -> str:
    """Generate markdown bullet list."""
    return "\n".join(f"- {item}" for item in items) + "\n"


# =============================================================================
# Report Writing
# =============================================================================

def write_report(filename: str, content: str):
    """Write markdown report to output directory."""
    output_path = get_output_dir() / filename
    with open(output_path, 'w') as f:
        f.write(content)
    print(f"  Report written: {output_path}")


def generate_placeholder_report(experiment_name: str, description: str,
                                 how_to_run: str) -> str:
    """Generate placeholder report for empty results directories."""
    return f"""# {experiment_name} Report

**Status**: No results available yet

## Description

{description}

## How to Generate Results

{how_to_run}

---

*Report generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}*
"""


# =============================================================================
# PDF Generation
# =============================================================================

def save_figures_to_pdf(figure_paths: List[str], pdf_filename: str):
    """Combine all figures into a single PDF."""
    if not figure_paths:
        print(f"  No figures to compile into PDF")
        return

    pdf_path = get_output_dir() / pdf_filename

    with PdfPages(pdf_path) as pdf:
        for fig_path in figure_paths:
            fig, ax = plt.subplots(figsize=(12, 8))
            img = mpimg.imread(fig_path)
            ax.imshow(img)
            ax.axis('off')
            # Add filename as title
            ax.set_title(Path(fig_path).stem, fontsize=10, pad=10)
            pdf.savefig(fig, bbox_inches='tight', dpi=150)
            plt.close(fig)

    print(f"  PDF saved: {pdf_path}")


# =============================================================================
# Data Extraction Helpers
# =============================================================================

def extract_implementations_df(results: Dict, metrics: List[str]) -> pd.DataFrame:
    """
    Extract implementation data into a DataFrame.

    Args:
        results: Dict of {result_name: result_data}
        metrics: List of metric names to extract

    Returns:
        DataFrame with implementations as index
    """
    all_data = []

    for result_name, result_data in results.items():
        dataset = result_data.get('dataset', result_name)

        for impl in result_data.get('implementations', []):
            row = {'dataset': dataset, 'name': impl['name']}
            for metric in metrics:
                row[metric] = impl.get(metric, np.nan)
            all_data.append(row)

    return pd.DataFrame(all_data)


def get_report_timestamp() -> str:
    """Get formatted timestamp for reports."""
    return datetime.now().strftime('%Y-%m-%d %H:%M:%S')
