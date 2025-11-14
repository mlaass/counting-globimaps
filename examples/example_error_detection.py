#!/usr/bin/env python3
"""
Error Detection Example

This example demonstrates how to use CountingGloBiMap's error detection features:
- Enabling input collection mode
- Detecting false positives and count errors
- Analyzing error magnitudes and rates
"""

import counting_globimap as cg
import random

def main():
    print("=" * 60)
    print("CountingGloBiMap - Error Detection Example")
    print("=" * 60)

    # Create a filter with input collection enabled
    # This allows us to track actual inputs and detect errors
    config = cg.make_single_layer_config(
        k=6,           # Fewer hash functions = more errors (for demonstration)
        bits=8,
        logsize=18     # Smaller size = more errors (256 KB)
    )

    print(f"\nConfiguration: {config}")
    print(f"  Hash functions: {config.hash_k}")
    print(f"  Memory: {(2**18) / 1024:.1f} KB")
    print(f"  Input collection: ENABLED")

    # Create filter WITH input collection
    cbf = cg.CountingGloBiMap(config, collect_input=True)
    print(f"\nCreated filter: {cbf}")

    # Insert a set of points
    print("\n" + "-" * 60)
    print("Inserting points...")
    print("-" * 60)

    random.seed(42)  # For reproducibility
    width, height = 1000, 1000
    num_points = 5000

    inserted_points = []
    for i in range(num_points):
        x = random.randint(0, width - 1)
        y = random.randint(0, height - 1)
        point = [x, y]
        cbf.put(point)
        inserted_points.append(point)

    print(f"Inserted {num_points} random points in {width}x{height} region")

    # Detect errors in the region
    print("\n" + "-" * 60)
    print("Detecting errors...")
    print("-" * 60)

    cbf.detect_errors(0, 0, width, height)

    print(f"Error detection completed!")
    print(f"Error rate: {cbf.error_rate * 100:.4f}%")

    # Get error statistics
    print("\n" + "=" * 60)
    print("Error Analysis")
    print("=" * 60)

    import json
    error_summary = json.loads(cbf.error_summary())

    print(f"\nUnique input points: {error_summary['unique_input']}")
    print(f"Errors detected: {error_summary['errors']}")
    print(f"Error rate: {error_summary['error_rate'] * 100:.4f}%")

    if error_summary['errors'] > 0:
        print(f"\nError magnitudes:")
        print(f"  Minimum: {error_summary['magnitude_min']}")
        print(f"  Maximum: {error_summary['magnitude_max']}")
        print(f"  Mean: {error_summary['magnitude_mean']:.2f}")
        print(f"  Sum: {error_summary['magnitude_sum']}")

        # Show histogram of error magnitudes
        print(f"\nError magnitude histogram (first 10 bins):")
        histogram = error_summary['histogram'][:10]
        for i, count in enumerate(histogram):
            if count > 0:
                print(f"  Bin {i}: {count:.2f}")

    # Full filter summary
    print("\n" + "=" * 60)
    print("Complete Filter Summary")
    print("=" * 60)
    filter_summary = json.loads(cbf.summary())
    print(json.dumps(filter_summary, indent=2))

    # Demonstrate false positive detection
    print("\n" + "-" * 60)
    print("Testing for false positives...")
    print("-" * 60)

    # Test some points that were NOT inserted
    false_positive_count = 0
    test_count = 1000

    for i in range(test_count):
        # Generate a point that's very unlikely to have been inserted
        test_point = [random.randint(5000, 6000), random.randint(5000, 6000)]
        if cbf.get_bool(test_point):
            false_positive_count += 1

    fp_rate = (false_positive_count / test_count) * 100
    print(f"Tested {test_count} points outside inserted region")
    print(f"False positives: {false_positive_count}")
    print(f"False positive rate: {fp_rate:.2f}%")

    print("\n✓ Error detection example completed!")

if __name__ == "__main__":
    main()
