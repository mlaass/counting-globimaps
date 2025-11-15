"""
Utilities for Jupyter notebooks to work with experiment results and datasets.

This module provides dataset-agnostic helpers for:
- Discovering available HDF5 datasets
- Loading experiment results from JSON files
- Extracting dataset metadata
"""

import json
import h5py
import numpy as np
from pathlib import Path
from typing import List, Dict, Tuple, Optional


def discover_datasets(hdf5_dir: str = "../datasets/hdf5") -> List[Tuple[str, str]]:
    """
    Auto-discover available HDF5 datasets.

    Args:
        hdf5_dir: Path to directory containing .h5 files

    Returns:
        List of (display_name, filename) tuples

    Example:
        >>> datasets = discover_datasets()
        >>> for name, file in datasets:
        ...     print(f"{name}: {file}")
        GDELT Events: gdelt_events.h5
        COVID-19 Cases: covid19_cases.h5
    """
    datasets = []
    hdf5_path = Path(hdf5_dir)

    if not hdf5_path.exists():
        print(f"Warning: HDF5 directory not found: {hdf5_dir}")
        return datasets

    for h5_file in sorted(hdf5_path.glob("*.h5")):
        # Create display name from filename
        display_name = h5_file.stem.replace('_', ' ').title()
        datasets.append((display_name, h5_file.name))

    return datasets


def get_dataset_metadata(h5_file: str, hdf5_dir: str = "../datasets/hdf5") -> Dict:
    """
    Extract metadata from HDF5 file attributes.

    Args:
        h5_file: Filename of HDF5 file
        hdf5_dir: Directory containing HDF5 files

    Returns:
        Dictionary with metadata (description, format, categories, etc.)

    Example:
        >>> meta = get_dataset_metadata("gdelt_events_multicategory.h5")
        >>> print(meta['description'])
        GDELT events with QuadClass categories
    """
    h5_path = Path(hdf5_dir) / h5_file

    if not h5_path.exists():
        return {}

    metadata = {}
    try:
        with h5py.File(h5_path, 'r') as f:
            # Extract file-level attributes
            for key, val in f.attrs.items():
                metadata[key] = val

            # Get dataset information
            if 'coords' in f:
                coords = f['coords']
                metadata['shape'] = coords.shape
                metadata['dtype'] = str(coords.dtype)

                # For multi-category datasets
                if len(coords.shape) == 2 and coords.shape[1] == 3:
                    metadata['has_categories'] = True
                    # Count events per category
                    data = coords[:]
                    categories, counts = np.unique(data[:, 2], return_counts=True)
                    metadata['category_counts'] = {int(cat): int(count)
                                                   for cat, count in zip(categories, counts)}
                else:
                    metadata['has_categories'] = False

    except Exception as e:
        print(f"Error reading metadata from {h5_file}: {e}")

    return metadata


def load_experiment_results(results_dir: str, pattern: str = "*.json") -> Dict[str, Dict]:
    """
    Load all experiment result JSON files from a directory.

    Args:
        results_dir: Path to results directory (e.g., "../results/dataset_comparison")
        pattern: Glob pattern for JSON files (default: "*.json")

    Returns:
        Dictionary mapping filename (without .json) to parsed JSON content

    Example:
        >>> results = load_experiment_results("../results/dataset_comparison")
        >>> for name, data in results.items():
        ...     print(f"{name}: {len(data['implementations'])} implementations")
    """
    results = {}
    results_path = Path(results_dir)

    if not results_path.exists():
        print(f"Warning: Results directory not found: {results_dir}")
        return results

    for json_file in sorted(results_path.glob(pattern)):
        try:
            with open(json_file, 'r') as f:
                data = json.load(f)
                # Use filename without extension as key
                results[json_file.stem] = data
        except Exception as e:
            print(f"Error loading {json_file}: {e}")

    return results


def list_result_types() -> List[Tuple[str, str]]:
    """
    List available experiment result types and their directories.

    Returns:
        List of (experiment_type, directory_path) tuples

    Example:
        >>> for exp_type, path in list_result_types():
        ...     print(f"{exp_type}: {path}")
    """
    base_results = Path("../results")

    if not base_results.exists():
        return []

    result_types = []
    for subdir in sorted(base_results.iterdir()):
        if subdir.is_dir() and subdir.name != "logs":
            # Create display name from directory name
            display = subdir.name.replace('_', ' ').title()
            result_types.append((display, str(subdir)))

    return result_types


def load_dataset_hdf5(filename: str, hdf5_dir: str = "../datasets/hdf5",
                      max_points: Optional[int] = None) -> np.ndarray:
    """
    Load coordinate data from HDF5 file.

    Args:
        filename: HDF5 filename
        hdf5_dir: Directory containing HDF5 files
        max_points: Maximum number of points to load (None = all)

    Returns:
        NumPy array of coordinates [N, 2] or [N, 3] for multi-category

    Example:
        >>> coords = load_dataset_hdf5("gdelt_events.h5", max_points=10000)
        >>> print(f"Loaded {len(coords)} points")
    """
    h5_path = Path(hdf5_dir) / filename

    with h5py.File(h5_path, 'r') as f:
        coords = f['coords']
        if max_points:
            data = coords[:max_points]
        else:
            data = coords[:]

    return np.array(data)


def summarize_results(results_dir: str, metric: str = "mean_error_pct"):
    """
    Create a summary DataFrame from experiment results.

    Args:
        results_dir: Path to results directory
        metric: Metric to extract (e.g., "mean_error_pct", "insert_time_sec")

    Returns:
        pandas DataFrame with implementations as rows, datasets as columns

    Example:
        >>> df = summarize_results("../results/dataset_comparison", "mean_error_pct")
        >>> print(df)
    """
    import pandas as pd

    results = load_experiment_results(results_dir)

    # Build matrix: implementations × datasets
    data = {}
    for dataset_name, result in results.items():
        if 'implementations' in result:
            data[dataset_name] = {
                impl['name']: impl.get(metric, np.nan)
                for impl in result['implementations']
            }

    df = pd.DataFrame(data)
    return df


# Convenience function for common workflow
def quick_analysis(experiment_type: str = "dataset_comparison"):
    """
    Quick analysis workflow: discover datasets, load results, and summarize.

    Args:
        experiment_type: Type of experiment (subdirectory under results/)

    Returns:
        Tuple of (datasets, results, summary_df)

    Example:
        >>> datasets, results, summary = quick_analysis("dataset_comparison")
        >>> print(summary)
    """
    # Discover available datasets
    datasets = discover_datasets()

    # Load experiment results
    results_dir = f"../results/{experiment_type}"
    results = load_experiment_results(results_dir)

    # Create summary
    try:
        summary = summarize_results(results_dir)
    except Exception as e:
        print(f"Could not create summary: {e}")
        summary = None

    return datasets, results, summary
