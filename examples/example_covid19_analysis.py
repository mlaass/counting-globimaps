#!/usr/bin/env python3
"""
Example: Analyzing COVID-19 case data with CountingGloBiMap

This example demonstrates how to use CountingGloBiMap to analyze temporal-spatial
patterns in COVID-19 case data. We create a counting filter to estimate case density
in different regions with minimal memory usage.

Dataset: Johns Hopkins CSSE COVID-19 Daily Reports
Download: Run ../download_datasets.sh covid19
"""

import sys
import csv
from pathlib import Path

# Add parent directory to path to import counting_globimap
sys.path.insert(0, str(Path(__file__).parent.parent))

import numpy as np
import matplotlib.pyplot as plt

try:
    import counting_globimap as cgm
except ImportError:
    print("Error: counting_globimap module not found.")
    print("Please build it with: python setup.py build_ext --inplace")
    sys.exit(1)


def load_covid_data(csv_path):
    """Load COVID-19 case data from Johns Hopkins CSV format.

    Returns:
        list of (lat, lon, confirmed_cases) tuples
    """
    cases = []

    with open(csv_path, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)

        for row in reader:
            try:
                # Handle different CSV formats (Johns Hopkins changed format over time)
                lat = float(row.get('Lat') or row.get('Latitude') or 0)
                lon = float(row.get('Long_') or row.get('Longitude') or 0)
                confirmed = int(row.get('Confirmed', 0))

                if lat != 0 and lon != 0 and confirmed > 0:
                    cases.append((lat, lon, confirmed))
            except (ValueError, KeyError):
                continue

    return cases


def coords_to_pixels(lat, lon, resolution=0.1):
    """Convert lat/lon to pixel coordinates at given resolution (degrees).

    Args:
        lat: Latitude in degrees
        lon: Longitude in degrees
        resolution: Pixel size in degrees (default 0.1° ≈ 11km)

    Returns:
        (x, y) pixel coordinates
    """
    x = int((lon + 180) / resolution)
    y = int((90 - lat) / resolution)
    return x, y


def analyze_covid_data(csv_path, resolution=0.1):
    """Analyze COVID-19 data using CountingGloBiMap.

    Args:
        csv_path: Path to Johns Hopkins COVID-19 CSV file
        resolution: Spatial resolution in degrees
    """
    print(f"\nAnalyzing COVID-19 data from: {csv_path}")
    print(f"Spatial resolution: {resolution}° (≈{resolution * 111:.1f} km)")

    # Load data
    print("\nLoading data...")
    cases = load_covid_data(csv_path)
    print(f"Loaded {len(cases)} locations with confirmed cases")

    if not cases:
        print("No valid case data found in file")
        return

    # Create multi-layer counting filter
    # Layer 1: 1-bit (presence/absence) - 2^24 = 16M buckets
    # Layer 2: 8-bit (0-255 counts) - 2^20 = 1M buckets
    # Layer 3: 16-bit (0-65K counts) - 2^18 = 256K buckets
    # Layer 4: 32-bit (large counts) - 2^16 = 64K buckets
    config = cgm.make_multi_layer_config(
        k=3,  # 3 hash functions
        layer_specs=[
            (1, 24),   # 1-bit, 2^24 buckets (2 MB)
            (8, 20),   # 8-bit, 2^20 buckets (1 MB)
            (16, 18),  # 16-bit, 2^18 buckets (512 KB)
            (32, 16),  # 32-bit, 2^16 buckets (256 KB)
        ]
    )

    cgmap = cgm.CountingGloBiMap(config, collect_input=True)

    print(f"\nFilter memory usage: {cgmap.byte_size() / 1024 / 1024:.2f} MB")

    # Insert case data
    print("\nInserting case data into filter...")
    total_cases = 0

    for lat, lon, confirmed in cases:
        x, y = coords_to_pixels(lat, lon, resolution)

        # Insert pixel multiple times based on case count
        # Use log scale to avoid overwhelming the filter
        count = max(1, int(np.log10(confirmed + 1)))

        for _ in range(count):
            cgmap.put([x, y])

        total_cases += confirmed

    print(f"Total confirmed cases: {total_cases:,}")

    # Analyze spatial distribution
    print("\n" + "="*60)
    print("SPATIAL ANALYSIS")
    print("="*60)

    # Sample some known hotspot regions
    hotspots = [
        ("New York City", 40.7128, -74.0060),
        ("Los Angeles", 34.0522, -118.2437),
        ("London", 51.5074, -0.1278),
        ("Milan", 45.4642, 9.1900),
        ("Wuhan", 30.5928, 114.3055),
        ("Mumbai", 19.0760, 72.8777),
    ]

    print("\nEstimated case density in major cities:")
    print(f"{'City':<20} {'Lat':>8} {'Lon':>9} {'Estimated':>10}")
    print("-" * 60)

    for city, lat, lon in hotspots:
        x, y = coords_to_pixels(lat, lon, resolution)
        estimated = cgmap.get_min([x, y])
        print(f"{city:<20} {lat:8.4f} {lon:9.4f} {estimated:10}")

    # Error analysis
    errors = cgmap.detect_errors()
    print(f"\n\nError Analysis:")
    print(f"  Input pixels tracked: {errors['input_size']}")
    print(f"  False positives detected: {errors['fp_count']}")
    print(f"  False positive rate: {errors['fp_rate']*100:.2f}%")

    # Memory efficiency
    print(f"\n\nMemory Efficiency:")
    uncompressed_size = len(cases) * 16  # (x, y, count) ≈ 16 bytes each
    compressed_size = cgmap.byte_size()
    ratio = uncompressed_size / compressed_size

    print(f"  Uncompressed (naive): {uncompressed_size / 1024 / 1024:.2f} MB")
    print(f"  CountingGloBiMap: {compressed_size / 1024 / 1024:.2f} MB")
    print(f"  Compression ratio: {ratio:.1f}x")

    return cgmap, cases


def visualize_distribution(cgmap, cases, resolution=0.1, output_file=None):
    """Create visualization of case distribution.

    Args:
        cgmap: CountingGloBiMap instance
        cases: List of (lat, lon, count) tuples
        resolution: Spatial resolution in degrees
        output_file: Optional path to save figure
    """
    print("\n\nCreating visualization...")

    # Extract coordinates
    lats = [lat for lat, _, _ in cases]
    lons = [lon for _, lon, _ in cases]
    counts = [count for _, _, count in cases]

    # Create figure with two subplots
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 6))

    # Plot 1: Actual case counts (log scale)
    scatter1 = ax1.scatter(lons, lats, c=np.log10(np.array(counts) + 1),
                          s=10, alpha=0.6, cmap='YlOrRd')
    ax1.set_xlabel('Longitude')
    ax1.set_ylabel('Latitude')
    ax1.set_title('COVID-19 Cases (Log Scale)')
    ax1.grid(True, alpha=0.3)
    plt.colorbar(scatter1, ax=ax1, label='log10(cases + 1)')

    # Plot 2: CountingGloBiMap estimates
    estimated_counts = []
    for lat, lon, _ in cases:
        x, y = coords_to_pixels(lat, lon, resolution)
        est = cgmap.get_min([x, y])
        estimated_counts.append(est)

    scatter2 = ax2.scatter(lons, lats, c=estimated_counts,
                          s=10, alpha=0.6, cmap='YlOrRd')
    ax2.set_xlabel('Longitude')
    ax2.set_ylabel('Latitude')
    ax2.set_title('CountingGloBiMap Estimates')
    ax2.grid(True, alpha=0.3)
    plt.colorbar(scatter2, ax=ax2, label='Estimated count')

    plt.tight_layout()

    if output_file:
        plt.savefig(output_file, dpi=150, bbox_inches='tight')
        print(f"Saved visualization to: {output_file}")
    else:
        plt.show()


def main():
    # Find COVID-19 dataset
    datasets_dir = Path(__file__).parent.parent / "datasets" / "covid19"

    if not datasets_dir.exists():
        print(f"COVID-19 dataset not found at: {datasets_dir}")
        print("\nPlease download datasets first:")
        print("  cd ..")
        print("  ./download_datasets.sh covid19")
        return

    # Find a daily report file
    csv_files = list(datasets_dir.glob("csse_covid_19_daily_reports/*.csv"))

    if not csv_files:
        print(f"No CSV files found in: {datasets_dir}")
        return

    # Use one of the later files (more comprehensive data)
    csv_files.sort()
    csv_path = csv_files[-1]  # Use most recent file

    # Analyze data
    cgmap, cases = analyze_covid_data(csv_path, resolution=0.1)

    # Create visualization
    output_dir = Path(__file__).parent.parent / "output"
    output_dir.mkdir(exist_ok=True)
    output_file = output_dir / "covid19_analysis.png"

    try:
        visualize_distribution(cgmap, cases, resolution=0.1, output_file=output_file)
    except Exception as e:
        print(f"\nVisualization skipped: {e}")
        print("(Install matplotlib if you want visualizations)")


if __name__ == "__main__":
    main()
