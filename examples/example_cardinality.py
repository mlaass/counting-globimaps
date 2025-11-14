#!/usr/bin/env python3
"""
Cardinality Estimation Example

This example demonstrates using CountingGloBiMap for cardinality estimation:
- Estimating unique element counts in spatial data
- Comparing estimated vs. actual cardinality
- Understanding accuracy vs. memory trade-offs
"""

import counting_globimap as cg
import random
from collections import Counter

def main():
    print("=" * 60)
    print("CountingGloBiMap - Cardinality Estimation Example")
    print("=" * 60)

    # Configuration for cardinality estimation
    # Using more hash functions improves accuracy
    config = cg.make_single_layer_config(
        k=12,  # More hash functions = better accuracy
        bits=16,
        logsize=22  # 4 MB memory
    )

    print(f"\nConfiguration: {config}")
    print(f"  Hash functions: {config.hash_k}")
    print(f"  Memory: {cbf.byte_size() / 1024 / 1024:.2f} MB" if 'cbf' in dir() else f"  Memory: ~{(2**22 * 2) / 1024 / 1024:.2f} MB (estimated)")

    cbf = cg.CountingGloBiMap(config, collect_input=True)

    # Simulate spatial data with repeated coordinates
    print("\n" + "-" * 60)
    print("Simulating spatial data stream...")
    print("-" * 60)

    random.seed(123)
    width, height = 2048, 2048

    # Generate data with Zipf-like distribution (some points are very popular)
    num_insertions = 100000
    num_unique_points = 10000

    # Create a set of unique points
    unique_points = []
    for _ in range(num_unique_points):
        x = random.randint(0, width - 1)
        y = random.randint(0, height - 1)
        unique_points.append([x, y])

    # Insert points with varying frequencies (Zipf distribution)
    actual_counts = Counter()
    print(f"Inserting {num_insertions} points...")

    for _ in range(num_insertions):
        # Pick a point with bias toward first elements (Zipf-like)
        idx = int(random.paretovariate(1.5)) % len(unique_points)
        point = unique_points[idx]
        point_tuple = tuple(point)

        cbf.put(point)
        actual_counts[point_tuple] += 1

    print(f"  Total insertions: {num_insertions}")
    print(f"  Unique points: {len(actual_counts)}")

    # Detect errors to get accurate statistics
    cbf.detect_errors(0, 0, width, height)

    # Compare estimated vs. actual counts for popular points
    print("\n" + "=" * 60)
    print("Cardinality Estimates vs. Actual Counts")
    print("=" * 60)

    # Get the most frequent points
    most_common = actual_counts.most_common(15)

    print(f"\n{'Point':<20} {'Actual':<10} {'Estimated':<12} {'Error':<10} {'Error %':<10}")
    print("-" * 72)

    total_absolute_error = 0
    for point_tuple, actual_count in most_common:
        point = list(point_tuple)
        estimated_count = cbf.get_min(point)
        error = abs(estimated_count - actual_count)
        error_pct = (error / actual_count * 100) if actual_count > 0 else 0

        total_absolute_error += error

        print(f"{str(point):<20} {actual_count:<10} {estimated_count:<12} {error:<10} {error_pct:<10.2f}%")

    avg_error = total_absolute_error / len(most_common)
    print(f"\nAverage absolute error: {avg_error:.2f}")

    # Test cardinality of  regions
    print("\n" + "=" * 60)
    print("Regional Cardinality Estimation")
    print("=" * 60)

    # Count actual unique points in different regions
    regions = [
        ("Top-left quadrant", 0, 0, width // 2, height // 2),
        ("Top-right quadrant", width // 2, 0, width // 2, height // 2),
        ("Bottom-left quadrant", 0, height // 2, width // 2, height // 2),
        ("Bottom-right quadrant", width // 2, height // 2, width // 2, height // 2),
    ]

    for region_name, rx, ry, rw, rh in regions:
        # Count actual unique points in region
        actual_unique = sum(1 for (px, py), count in actual_counts.items()
                           if rx <= px < rx + rw and ry <= py < ry + rh)

        # Estimate using filter (sample some points and check)
        estimated_unique = 0
        sample_size = 1000
        for _ in range(sample_size):
            test_x = random.randint(rx, rx + rw - 1)
            test_y = random.randint(ry, ry + rh - 1)
            if cbf.get_bool([test_x, test_y]):
                estimated_unique += 1

        # Scale up the estimate
        estimated_unique = int(estimated_unique / sample_size * rw * rh)

        error = abs(estimated_unique - actual_unique)
        error_pct = (error / actual_unique * 100) if actual_unique > 0 else 0

        print(f"\n{region_name}:")
        print(f"  Actual unique points: {actual_unique}")
        print(f"  Estimated unique points: {estimated_unique}")
        print(f"  Error: {error} ({error_pct:.2f}%)")

    # Show overall statistics
    print("\n" + "=" * 60)
    print("Overall Statistics")
    print("=" * 60)

    import json
    summary = json.loads(cbf.summary())
    error_summary = json.loads(cbf.error_summary())

    print(f"Filter memory usage: {cbf.byte_size() / 1024 / 1024:.2f} MB")
    print(f"Unique input points tracked: {error_summary['unique_input']}")
    print(f"Error rate: {error_summary['error_rate'] * 100:.4f}%")
    print(f"Total errors detected: {error_summary['errors']}")

    print("\n✓ Cardinality estimation example completed!")

if __name__ == "__main__":
    main()
