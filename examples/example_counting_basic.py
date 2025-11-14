#!/usr/bin/env python3
"""
Basic CountingGloBiMap Example

This example demonstrates the fundamental operations of CountingGloBiMap:
- Creating a filter with a simple configuration
- Inserting points
- Querying points
- Getting cardinality estimates
"""

import counting_globimap as cg

def main():
    print("=" * 60)
    print("CountingGloBiMap - Basic Example")
    print("=" * 60)

    # Create a simple single-layer filter
    # Parameters: k=8 hash functions, 8-bit counters, 2^20 size (1MB)
    config = cg.make_single_layer_config(
        k=8,           # Number of hash functions
        bits=8,        # 8-bit counters (0-255)
        logsize=20     # 2^20 = 1,048,576 counters
    )

    print(f"\nConfiguration: {config}")
    print(f"  Hash functions (k): {config.hash_k}")
    print(f"  Layers: {len(config.layers)}")
    print(f"  Layer 0: {config.layers[0].bits}-bit counters, size 2^{config.layers[0].logsize}")

    # Create the CountingGloBiMap
    cbf = cg.CountingGloBiMap(config, collect_input=False)
    print(f"\nCreated filter: {cbf}")
    print(f"Memory usage: {cbf.byte_size():,} bytes ({cbf.byte_size() / 1024:.1f} KB)")

    # Insert some points
    print("\n" + "-" * 60)
    print("Inserting points...")
    print("-" * 60)

    points = [
        [100, 200],
        [150, 250],
        [100, 200],  # Duplicate - will increment count
        [300, 400],
        [100, 200],  # Another duplicate
    ]

    for point in points:
        cbf.put(point)
        print(f"Inserted point {point}")

    # Query points
    print("\n" + "-" * 60)
    print("Querying points...")
    print("-" * 60)

    test_points = [
        [100, 200],  # Inserted 3 times
        [150, 250],  # Inserted 1 time
        [300, 400],  # Inserted 1 time
        [999, 999],  # Never inserted
    ]

    for point in test_points:
        exists = cbf.get_bool(point)
        count = cbf.get_min(point)
        mean_count = cbf.get_mean(point)

        print(f"\nPoint {point}:")
        print(f"  Exists (probabilistic): {exists}")
        print(f"  Min-count estimate: {count}")
        print(f"  Mean-count estimate: {mean_count:.2f}")

    # Summary
    print("\n" + "=" * 60)
    print("Filter Summary")
    print("=" * 60)
    print(cbf.summary())

    print("\n✓ Example completed successfully!")

if __name__ == "__main__":
    main()
