#!/usr/bin/env python3
"""
Example: Analyzing GDELT global events with CountingGloBiMap

This example demonstrates how to use CountingGloBiMap to analyze spatial patterns
in the GDELT (Global Database of Events, Language, and Tone) dataset, which tracks
global events from news media worldwide.

Dataset: GDELT 2.0 Events Database
Download: Run ../download_datasets.sh gdelt
"""

import sys
import csv
from pathlib import Path
from collections import defaultdict

# Add parent directory to path to import counting_globimap
sys.path.insert(0, str(Path(__file__).parent.parent))

import numpy as np

try:
    import counting_globimap as cgm
except ImportError:
    print("Error: counting_globimap module not found.")
    print("Please build it with: python setup.py build_ext --inplace")
    sys.exit(1)


# CAMEO Event Categories (simplified)
EVENT_CATEGORIES = {
    'verbal_coop': (1, 5),      # Events 01-05: Verbal cooperation
    'material_coop': (6, 8),     # Events 06-08: Material cooperation
    'verbal_conflict': (9, 13),  # Events 09-13: Verbal conflict
    'material_conflict': (14, 20), # Events 14-20: Material conflict (protests, fights, etc.)
}

QUAD_CLASS_NAMES = {
    '1': 'Verbal Cooperation',
    '2': 'Material Cooperation',
    '3': 'Verbal Conflict',
    '4': 'Material Conflict',
}


def load_gdelt_events(csv_path, filter_quad_class=None, max_events=None):
    """Load GDELT event data from CSV.

    Args:
        csv_path: Path to GDELT CSV file
        filter_quad_class: If specified, only load events of this QuadClass (1-4)
        max_events: Maximum number of events to load (for testing)

    Returns:
        list of (lat, lon, event_code, goldstein_scale, quad_class) tuples
    """
    events = []

    print(f"Loading GDELT events from: {csv_path}")

    with open(csv_path, 'r', encoding='utf-8') as f:
        # GDELT uses tab-separated values
        reader = csv.reader(f, delimiter='\t')
        next(reader)  # Skip header

        for i, row in enumerate(reader):
            if max_events and i >= max_events:
                break

            try:
                # GDELT CSV columns (0-indexed):
                # 17: QuadClass (1-4)
                # 18: GoldsteinScale (-10 to +10)
                # 41: ActionGeo_Lat
                # 42: ActionGeo_Long
                # 27: EventCode

                if len(row) < 43:
                    continue

                quad_class = row[17].strip()
                lat = float(row[41]) if row[41] else 0
                lon = float(row[42]) if row[42] else 0
                event_code = row[27].strip() if row[27] else ''
                goldstein_scale = float(row[18]) if row[18] else 0

                # Filter by QuadClass if specified
                if filter_quad_class and quad_class != str(filter_quad_class):
                    continue

                # Skip events without location
                if lat == 0 and lon == 0:
                    continue

                events.append((lat, lon, event_code, goldstein_scale, quad_class))

            except (ValueError, IndexError):
                continue

            # Progress indicator
            if (i + 1) % 50000 == 0:
                print(f"  Processed {i+1} rows...")

    return events


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


def analyze_gdelt_events(csv_path, resolution=0.1, filter_quad_class=None):
    """Analyze GDELT events using CountingGloBiMap.

    Args:
        csv_path: Path to GDELT CSV file
        resolution: Spatial resolution in degrees
        filter_quad_class: Filter for specific event type (1-4)
    """
    print("\n" + "="*70)
    print("GDELT GLOBAL EVENTS ANALYSIS")
    print("="*70)

    if filter_quad_class:
        print(f"\nFiltering for: QuadClass {filter_quad_class} ({QUAD_CLASS_NAMES.get(str(filter_quad_class), 'Unknown')})")

    print(f"Spatial resolution: {resolution}° (≈{resolution * 111:.1f} km)")

    # Load events
    print("\nLoading GDELT events...")
    events = load_gdelt_events(csv_path, filter_quad_class=filter_quad_class)

    if not events:
        print("No events found in file")
        return

    print(f"Loaded {len(events):,} events")

    # Analyze event distribution by QuadClass
    quad_class_counts = defaultdict(int)
    for _, _, _, _, quad_class in events:
        quad_class_counts[quad_class] += 1

    print("\n" + "="*70)
    print("EVENT TYPE DISTRIBUTION")
    print("="*70)
    for qc in sorted(quad_class_counts.keys()):
        count = quad_class_counts[qc]
        pct = count / len(events) * 100
        name = QUAD_CLASS_NAMES.get(qc, f"Unknown ({qc})")
        print(f"  {name:<25} {count:8,} ({pct:5.1f}%)")

    # Create multi-layer counting filter
    config = cgm.make_multi_layer_config(
        k=4,  # 4 hash functions
        layer_specs=[
            (1, 24),   # 1-bit, 2^24 buckets (2 MB)
            (8, 20),   # 8-bit, 2^20 buckets (1 MB)
            (16, 18),  # 16-bit, 2^18 buckets (512 KB)
            (32, 16),  # 32-bit, 2^16 buckets (256 KB)
        ]
    )

    cgmap = cgm.CountingGloBiMap(config, collect_input=True)

    print(f"\n" + "="*70)
    print("FILTER CONFIGURATION")
    print("="*70)
    print(f"  Hash functions: k=4")
    print(f"  Memory usage: {cgmap.byte_size() / 1024 / 1024:.2f} MB")

    # Insert events into filter
    print("\n" + "="*70)
    print("INSERTING EVENTS")
    print("="*70)

    pixel_counts = defaultdict(int)
    goldstein_by_pixel = defaultdict(list)

    for lat, lon, event_code, goldstein_scale, quad_class in events:
        x, y = coords_to_pixels(lat, lon, resolution)
        cgmap.put([x, y])
        pixel_counts[(x, y)] += 1
        goldstein_by_pixel[(x, y)].append(goldstein_scale)

    print(f"Inserted {len(events):,} events into filter")
    print(f"Unique locations: {len(pixel_counts):,}")

    # Identify hotspots
    print("\n" + "="*70)
    print("TOP 20 EVENT HOTSPOTS")
    print("="*70)

    sorted_pixels = sorted(pixel_counts.items(), key=lambda x: x[1], reverse=True)

    print(f"\n{'Rank':<6} {'Lat':>8} {'Lon':>9} {'Actual':>8} {'Estimated':>10} {'Avg Impact':>12}")
    print("-" * 70)

    for rank, ((x, y), actual_count) in enumerate(sorted_pixels[:20], 1):
        estimated = cgmap.get_min([x, y])

        # Convert back to lat/lon
        lat = 90 - (y * resolution)
        lon = (x * resolution) - 180

        # Calculate average Goldstein scale for this location
        avg_goldstein = np.mean(goldstein_by_pixel[(x, y)])

        print(f"{rank:<6} {lat:8.2f} {lon:9.2f} {actual_count:8,} {estimated:10,} {avg_goldstein:12.2f}")

    # Analyze by region
    print("\n" + "="*70)
    print("EVENT DENSITY BY MAJOR REGIONS")
    print("="*70)

    regions = [
        ("Washington DC", 38.9072, -77.0369),
        ("Brussels (EU)", 50.8503, 4.3517),
        ("Moscow", 55.7558, 37.6173),
        ("Beijing", 39.9042, 116.4074),
        ("Kyiv (Ukraine)", 50.4501, 30.5234),
        ("Gaza", 31.5, 34.4668),
        ("Damascus (Syria)", 33.5138, 36.2765),
        ("Kabul", 34.5553, 69.2075),
        ("Delhi", 28.7041, 77.1025),
        ("New York", 40.7128, -74.0060),
    ]

    print(f"\n{'Region':<25} {'Lat':>8} {'Lon':>9} {'Events':>10} {'Avg Impact':>12}")
    print("-" * 70)

    for region, lat, lon in regions:
        x, y = coords_to_pixels(lat, lon, resolution)
        estimated = cgmap.get_min([x, y])

        # Get average impact if we have data
        avg_impact = np.mean(goldstein_by_pixel[(x, y)]) if (x, y) in goldstein_by_pixel else 0.0

        print(f"{region:<25} {lat:8.2f} {lon:9.2f} {estimated:10,} {avg_impact:12.2f}")

    # Error analysis
    print("\n" + "="*70)
    print("ERROR ANALYSIS")
    print("="*70)

    errors = cgmap.detect_errors()
    print(f"\nFilter error statistics:")
    print(f"  Input pixels tracked: {errors['input_size']:,}")
    print(f"  False positives: {errors['fp_count']:,}")
    print(f"  False positive rate: {errors['fp_rate']*100:.3f}%")

    # Accuracy test on random sample
    test_pixels = list(pixel_counts.keys())[:100]
    actual_counts = [pixel_counts[p] for p in test_pixels]
    estimated_counts = [cgmap.get_min(list(p)) for p in test_pixels]

    errors_list = [abs(a - e) for a, e in zip(actual_counts, estimated_counts)]
    mean_error = np.mean(errors_list)
    max_error = np.max(errors_list)
    rmse = np.sqrt(np.mean(np.array(errors_list)**2))

    print(f"\nAccuracy on 100 random locations:")
    print(f"  Mean absolute error: {mean_error:.2f}")
    print(f"  Max absolute error: {max_error}")
    print(f"  RMSE: {rmse:.2f}")

    # Memory efficiency
    print("\n" + "="*70)
    print("MEMORY EFFICIENCY")
    print("="*70)

    uncompressed_size = len(pixel_counts) * 16  # (x, y, count) ≈ 16 bytes each
    compressed_size = cgmap.byte_size()
    ratio = uncompressed_size / compressed_size

    print(f"\nMemory usage comparison:")
    print(f"  Uncompressed (dict): {uncompressed_size / 1024 / 1024:.2f} MB")
    print(f"  CountingGloBiMap: {compressed_size / 1024 / 1024:.2f} MB")
    print(f"  Compression ratio: {ratio:.1f}x")

    return cgmap, events


def analyze_conflict_vs_cooperation(csv_path):
    """Compare spatial patterns of conflict vs cooperation events."""

    print("\n" + "="*70)
    print("CONFLICT VS COOPERATION SPATIAL ANALYSIS")
    print("="*70)

    # Load all events
    events = load_gdelt_events(csv_path)

    # Separate by QuadClass
    conflict_events = [(lat, lon) for lat, lon, _, _, qc in events if qc in ['3', '4']]
    coop_events = [(lat, lon) for lat, lon, _, _, qc in events if qc in ['1', '2']]

    print(f"\nConflict events (verbal + material): {len(conflict_events):,}")
    print(f"Cooperation events (verbal + material): {len(coop_events):,}")

    # Create separate filters
    config = cgm.make_single_layer_config(k=4, bits=16, logsize=22)

    conflict_map = cgm.CountingGloBiMap(config, collect_input=False)
    coop_map = cgm.CountingGloBiMap(config, collect_input=False)

    # Insert events
    for lat, lon in conflict_events:
        x, y = coords_to_pixels(lat, lon, resolution=0.1)
        conflict_map.put([x, y])

    for lat, lon in coop_events:
        x, y = coords_to_pixels(lat, lon, resolution=0.1)
        coop_map.put([x, y])

    # Compare regions
    regions = [
        ("Washington DC", 38.9072, -77.0369),
        ("Moscow", 55.7558, 37.6173),
        ("Kyiv", 50.4501, 30.5234),
        ("Gaza", 31.5, 34.4668),
        ("Brussels", 50.8503, 4.3517),
    ]

    print(f"\n{'Region':<20} {'Conflict':>12} {'Cooperation':>12} {'Ratio':>10}")
    print("-" * 70)

    for region, lat, lon in regions:
        x, y = coords_to_pixels(lat, lon, resolution=0.1)
        conflict_count = conflict_map.get_min([x, y])
        coop_count = coop_map.get_min([x, y])

        ratio = conflict_count / coop_count if coop_count > 0 else float('inf')
        ratio_str = f"{ratio:.2f}" if ratio != float('inf') else "∞"

        print(f"{region:<20} {conflict_count:12,} {coop_count:12,} {ratio_str:>10}")

    print(f"\nMemory usage:")
    print(f"  Conflict map: {conflict_map.byte_size() / 1024:.1f} KB")
    print(f"  Cooperation map: {coop_map.byte_size() / 1024:.1f} KB")
    print(f"  Total: {(conflict_map.byte_size() + coop_map.byte_size()) / 1024:.1f} KB")


def main():
    # Find GDELT dataset
    datasets_dir = Path(__file__).parent.parent / "datasets" / "gdelt"

    if not datasets_dir.exists():
        print(f"GDELT dataset not found at: {datasets_dir}")
        print("\nPlease download dataset first:")
        print("  cd ..")
        print("  ./download_datasets.sh gdelt")
        print("\nNote: This will download ~100-200 MB of data")
        return

    csv_path = datasets_dir / "gdelt_events_sample.csv"

    if not csv_path.exists():
        print(f"GDELT CSV file not found at: {csv_path}")
        print("\nPlease run: ./download_datasets.sh gdelt")
        return

    # Analysis 1: All events
    print("\nAnalysis 1: All GDELT Events")
    cgmap, events = analyze_gdelt_events(csv_path, resolution=0.1)

    # Analysis 2: Material conflicts only (protests, fights, etc.)
    print("\n\n" + "="*70)
    print("Analysis 2: Material Conflicts Only (QuadClass 4)")
    print("="*70)
    analyze_gdelt_events(csv_path, resolution=0.1, filter_quad_class=4)

    # Analysis 3: Conflict vs Cooperation comparison
    print("\n\n")
    analyze_conflict_vs_cooperation(csv_path)


if __name__ == "__main__":
    main()
