#!/usr/bin/env python3
"""
Multi-Layer CountingGloBiMap Example

This example demonstrates the hierarchical multi-layer design of CountingGloBiMap:
- Creating a filter with multiple layers of different bit depths
- Understanding cascading overflow between layers
- Observing how counts spread across layers
"""

import counting_globimap as cg

def main():
    print("=" * 60)
    print("CountingGloBiMap - Multi-Layer Example")
    print("=" * 60)

    # Create a multi-layer filter with cascading layers
    # Layer 0: 8-bit counters (0-255) - compact, fast
    # Layer 1: 16-bit counters (0-65535) - for overflow from layer 0
    # Layer 2: 32-bit counters - for very high counts
    config = cg.make_multi_layer_config(
        k=8,  # 8 hash functions
        layer_specs=[
            (8, 24),   # Layer 0: 8-bit, 2^24 size (16 MB)
            (16, 20),  # Layer 1: 16-bit, 2^20 size (2 MB)
            (32, 16),  # Layer 2: 32-bit, 2^16 size (256 KB)
        ]
    )

    print(f"\nMulti-layer configuration: {config}")
    print(f"  Hash functions (k): {config.hash_k}")
    print(f"  Number of layers: {len(config.layers)}")

    for i, layer in enumerate(config.layers):
        size_bytes = (2 ** layer.logsize) * (layer.bits // 8)
        print(f"  Layer {i}: {layer.bits}-bit counters, 2^{layer.logsize} size ({size_bytes / 1024 / 1024:.2f} MB)")

    # Create the filter
    cbf = cg.CountingGloBiMap(config, collect_input=False)
    print(f"\nCreated filter: {cbf}")
    print(f"Total memory: {cbf.byte_size() / 1024 / 1024:.2f} MB")

    # Insert points with varying frequencies
    print("\n" + "-" * 60)
    print("Inserting points with different frequencies...")
    print("-" * 60)

    # Point 1: Insert once
    for i in range(1):
        cbf.put([100, 100])
    print(f"Point [100, 100]: inserted 1 time")

    # Point 2: Insert 10 times
    for i in range(10):
        cbf.put([200, 200])
    print(f"Point [200, 200]: inserted 10 times")

    # Point 3: Insert 100 times
    for i in range(100):
        cbf.put([300, 300])
    print(f"Point [300, 300]: inserted 100 times")

    # Point 4: Insert 1000 times (will likely use multiple layers)
    for i in range(1000):
        cbf.put([400, 400])
    print(f"Point [400, 400]: inserted 1000 times")

    # Query and show results
    print("\n" + "-" * 60)
    print("Cardinality estimates:")
    print("-" * 60)

    test_points = [
        ([100, 100], 1),
        ([200, 200], 10),
        ([300, 300], 100),
        ([400, 400], 1000),
    ]

    for point, expected in test_points:
        count = cbf.get_min(point)
        error = abs(count - expected)
        error_rate = (error / expected) * 100 if expected > 0 else 0

        print(f"\nPoint {point}:")
        print(f"  Expected count: {expected}")
        print(f"  Estimated count: {count}")
        print(f"  Absolute error: {error}")
        print(f"  Error rate: {error_rate:.2f}%")

    # Show detailed summary
    print("\n" + "=" * 60)
    print("Detailed Filter Summary (JSON)")
    print("=" * 60)
    import json
    summary = json.loads(cbf.summary())
    print(json.dumps(summary, indent=2))

    print("\n✓ Multi-layer example completed!")

if __name__ == "__main__":
    main()
