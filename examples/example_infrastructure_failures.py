#!/usr/bin/env python3
"""
Example: Analyzing infrastructure failure patterns with CountingGloBiMap

This example demonstrates using CountingGloBiMap to analyze spatial patterns
in infrastructure failures (power outages, water main breaks, etc.) using
NYC 311 service request data.

Dataset: NYC OpenData 311 Service Requests
Download: Run ../download_datasets.sh infrastructure
"""

import sys
import csv
from pathlib import Path
from datetime import datetime

# Add parent directory to path to import counting_globimap
sys.path.insert(0, str(Path(__file__).parent.parent))

import numpy as np

try:
    import counting_globimap as cgm
except ImportError:
    print("Error: counting_globimap module not found.")
    print("Please build it with: python setup.py build_ext --inplace")
    sys.exit(1)


def load_311_data(csv_path, complaint_types=None):
    """Load NYC 311 service request data.

    Args:
        csv_path: Path to NYC 311 CSV file
        complaint_types: List of complaint types to filter (None = all)

    Returns:
        list of (lat, lon, complaint_type) tuples
    """
    requests = []

    print(f"Loading 311 data from: {csv_path}")

    with open(csv_path, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)

        for i, row in enumerate(reader):
            try:
                lat = float(row.get('Latitude', 0))
                lon = float(row.get('Longitude', 0))
                complaint_type = row.get('Complaint Type', 'Unknown')

                # Filter by complaint types if specified
                if complaint_types and complaint_type not in complaint_types:
                    continue

                if lat != 0 and lon != 0:
                    requests.append((lat, lon, complaint_type))

            except (ValueError, KeyError):
                continue

            # Progress indicator for large files
            if (i + 1) % 100000 == 0:
                print(f"  Processed {i+1} rows...")

    return requests


def coords_to_pixels(lat, lon, min_lat, min_lon, resolution=0.001):
    """Convert lat/lon to pixel coordinates.

    Args:
        lat, lon: Coordinates
        min_lat, min_lon: Origin point
        resolution: Pixel size in degrees (0.001° ≈ 111m)

    Returns:
        (x, y) pixel coordinates
    """
    x = int((lon - min_lon) / resolution)
    y = int((lat - min_lat) / resolution)
    return x, y


def analyze_infrastructure_failures(csv_path, resolution=0.001):
    """Analyze infrastructure failure patterns.

    Args:
        csv_path: Path to NYC 311 CSV
        resolution: Spatial resolution in degrees
    """
    print("\n" + "="*70)
    print("NYC INFRASTRUCTURE FAILURE ANALYSIS")
    print("="*70)

    # Infrastructure-related complaint types
    infrastructure_types = [
        'Water System',
        'Sewer',
        'Street Light Condition',
        'Street Condition',
        'Broken Parking Meter',
        'Water Main Break',
        'Manhole Overflow',
        'Damaged Tree',
        'Highway Condition',
        'Traffic Signal Condition',
    ]

    print(f"\nSpatial resolution: {resolution}° (≈{resolution * 111 * 1000:.0f}m)")
    print(f"\nFiltering for infrastructure-related complaints:")
    for ctype in infrastructure_types:
        print(f"  - {ctype}")

    # Load data
    print("\nLoading 311 service requests...")
    requests = load_311_data(csv_path, complaint_types=infrastructure_types)

    if not requests:
        print("\nNo infrastructure failure data found.")
        print("The file may use different complaint type names.")
        return

    print(f"\nLoaded {len(requests)} infrastructure failure reports")

    # Calculate bounds
    lats = [lat for lat, _, _ in requests]
    lons = [lon for _, lon, _ in requests]
    min_lat, max_lat = min(lats), max(lats)
    min_lon, max_lon = min(lons), max(lons)

    print(f"\nGeographic bounds:")
    print(f"  Latitude:  {min_lat:.4f} to {max_lat:.4f}")
    print(f"  Longitude: {min_lon:.4f} to {max_lon:.4f}")

    # Create counting filter
    # For NYC at ~111m resolution, we need ~2^16 buckets per dimension
    config = cgm.make_multi_layer_config(
        k=4,  # 4 hash functions for better accuracy
        layer_specs=[
            (1, 22),   # 1-bit, 2^22 buckets (512 KB)
            (8, 18),   # 8-bit, 2^18 buckets (256 KB)
            (16, 16),  # 16-bit, 2^16 buckets (128 KB)
            (32, 14),  # 32-bit, 2^14 buckets (64 KB)
        ]
    )

    cgmap = cgm.CountingGloBiMap(config, collect_input=True)

    print(f"\nFilter configuration:")
    print(f"  Hash functions: k=4")
    print(f"  Memory usage: {cgmap.byte_size() / 1024:.1f} KB")

    # Insert failure reports
    print("\nInserting failure reports into filter...")
    complaint_counts = {}

    for lat, lon, complaint_type in requests:
        x, y = coords_to_pixels(lat, lon, min_lat, min_lon, resolution)
        cgmap.put([x, y])

        complaint_counts[complaint_type] = complaint_counts.get(complaint_type, 0) + 1

    # Statistics
    print("\n" + "="*70)
    print("COMPLAINT TYPE BREAKDOWN")
    print("="*70)

    sorted_complaints = sorted(complaint_counts.items(), key=lambda x: x[1], reverse=True)
    for complaint_type, count in sorted_complaints:
        pct = count / len(requests) * 100
        print(f"{complaint_type:<40} {count:6} ({pct:5.1f}%)")

    # Identify hotspots
    print("\n" + "="*70)
    print("HOTSPOT ANALYSIS")
    print("="*70)

    print("\nIdentifying high-failure areas...")

    # Create grid of estimates
    pixel_counts = {}
    for lat, lon, _ in requests:
        x, y = coords_to_pixels(lat, lon, min_lat, min_lon, resolution)
        pixel_counts[(x, y)] = pixel_counts.get((x, y), 0) + 1

    # Find top hotspots
    sorted_pixels = sorted(pixel_counts.items(), key=lambda x: x[1], reverse=True)

    print(f"\nTop 10 failure hotspots:")
    print(f"{'Rank':<6} {'X':>6} {'Y':>6} {'Actual':>8} {'Estimated':>10} {'Error':>8}")
    print("-" * 70)

    for rank, ((x, y), actual_count) in enumerate(sorted_pixels[:10], 1):
        estimated = cgmap.get_min([x, y])
        error = abs(estimated - actual_count)

        # Convert back to lat/lon for reference
        lat = min_lat + y * resolution
        lon = min_lon + x * resolution

        print(f"{rank:<6} {x:6} {y:6} {actual_count:8} {estimated:10} {error:8}")
        print(f"       Location: ({lat:.4f}, {lon:.4f})")

    # Error analysis
    print("\n" + "="*70)
    print("ERROR ANALYSIS")
    print("="*70)

    errors = cgmap.detect_errors()
    print(f"\nFilter error statistics:")
    print(f"  Input pixels tracked: {errors['input_size']:,}")
    print(f"  False positives: {errors['fp_count']:,}")
    print(f"  False positive rate: {errors['fp_rate']*100:.3f}%")

    # Cardinality estimation
    total_pixels = len(pixel_counts)
    print(f"\nCardinality (unique failure locations):")
    print(f"  Actual unique pixels: {total_pixels:,}")

    # Memory comparison
    print("\n" + "="*70)
    print("MEMORY EFFICIENCY")
    print("="*70)

    uncompressed_size = len(requests) * 16  # (x, y, type) ≈ 16 bytes
    compressed_size = cgmap.byte_size()
    ratio = uncompressed_size / compressed_size

    print(f"\nMemory usage comparison:")
    print(f"  Uncompressed data: {uncompressed_size / 1024:.1f} KB")
    print(f"  CountingGloBiMap: {compressed_size / 1024:.1f} KB")
    print(f"  Compression ratio: {ratio:.1f}x")

    return cgmap, requests, min_lat, min_lon


def main():
    # Find infrastructure dataset
    datasets_dir = Path(__file__).parent.parent / "datasets" / "infrastructure"

    if not datasets_dir.exists():
        print(f"Infrastructure dataset not found at: {datasets_dir}")
        print("\nPlease download datasets first:")
        print("  cd ..")
        print("  ./download_datasets.sh infrastructure")
        return

    # Find CSV file
    csv_files = list(datasets_dir.glob("*.csv"))

    if not csv_files:
        print(f"No CSV files found in: {datasets_dir}")
        return

    csv_path = csv_files[0]

    # Analyze data
    analyze_infrastructure_failures(csv_path, resolution=0.001)


if __name__ == "__main__":
    main()
