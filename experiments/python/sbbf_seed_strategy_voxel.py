#!/usr/bin/env python3
"""
SBBF Seed Strategy Benchmark for Voxelized Meshes

Compares XOR vs MULTIPLY_SHIFT seed strategies on voxelized 3D meshes.
Uses fixed k=8 (matches SBBF_BENCHMARK_HASH_K in sbbf_benchmark_config.hpp).

Configurations:
- Meshes: bunny_128, teapot_128, armadillo_128, dragon_128
- SFC types: MORTON_3D, HILBERT_3D
- Intra-block strategies: double_hash, pattern_lookup
- Seed strategies: XOR, MULTIPLY_SHIFT

Metrics: FPR, insert latency, query latency

Output: JSON file with results for all meshes
"""

import json
import math
import sys
import time
from datetime import datetime
from pathlib import Path

import h5py
import numpy as np

# Add build directory to path for counting_globimap module
PROJECT_ROOT = Path(__file__).parent.parent.parent
sys.path.insert(0, str(PROJECT_ROOT / "build"))

try:
    import counting_globimap as cg
except ImportError as e:
    print("ERROR: Could not import counting_globimap module.")
    print("Make sure to build it first: cd build && make counting_globimap")
    print(f"Details: {e}")
    sys.exit(1)

# ============================================================================
# Configuration
# ============================================================================

HDF5_DIR = PROJECT_ROOT / "datasets" / "hdf5"
OUTPUT_DIR = PROJECT_ROOT / "sbbf_results" / "seed_strategy"
BITS_PER_ELEMENT = 10.0  # Standard BF sizing
HASH_K = 8  # Matches SBBF_BENCHMARK_HASH_K in sbbf_benchmark_config.hpp

# Meshes to test
MESHES = [
    {"name": "bunny", "resolution": 128},
    {"name": "teapot", "resolution": 128},
    {"name": "armadillo", "resolution": 128},
    {"name": "dragon", "resolution": 128},
]

# Configurations
SFC_TYPES = [
    ("MORTON_3D", cg.SFCType.MORTON_3D),
    ("HILBERT_3D", cg.SFCType.HILBERT_3D),
]

INTRA_STRATEGIES = [
    ("double_hash", cg.IntraBlockStrategy.DOUBLE_HASH),
    ("pattern_lookup", cg.IntraBlockStrategy.PATTERN_LOOKUP),
]

SEED_STRATEGIES = [
    ("XOR", cg.SeedStrategy.XOR),
    ("MULTIPLY_SHIFT", cg.SeedStrategy.MULTIPLY_SHIFT),
]


# ============================================================================
# Helper Functions
# ============================================================================


def compute_log_blocks(n: int, bits_per_element: float, bits_per_block: int = 64) -> int:
    """Compute log_num_blocks for target bits per element."""
    required_blocks = (n * bits_per_element) / bits_per_block
    log_blocks = int(math.ceil(math.log2(required_blocks)))
    return max(log_blocks, 10)  # Minimum 2^10 = 1024 blocks


def load_voxels(mesh_name: str, resolution: int) -> np.ndarray:
    """Load voxel coordinates from HDF5 file."""
    path = HDF5_DIR / f"{mesh_name}_{resolution}.h5"
    if not path.exists():
        raise FileNotFoundError(f"Voxel file not found: {path}")

    with h5py.File(path, "r") as f:
        coords = f["coords"][:]
    return coords


# ============================================================================
# Benchmark Functions
# ============================================================================


def run_benchmark(
    voxel_coords: np.ndarray,
    resolution: int,
    sfc_type: cg.SFCType,
    intra_strategy: cg.IntraBlockStrategy,
    seed_strategy: cg.SeedStrategy,
) -> dict:
    """Run SBBF benchmark for a single configuration."""
    num_voxels = len(voxel_coords)
    num_negatives = resolution**3 - num_voxels
    true_set = set(map(tuple, voxel_coords))

    # Compute log_num_blocks for bits per element
    log_num_blocks = compute_log_blocks(num_voxels, BITS_PER_ELEMENT)

    # Configure SBBF
    config = cg.SBBFConfig()
    config.sfc_type = sfc_type
    config.log_num_blocks = log_num_blocks
    config.hash_k = HASH_K
    config.bits_per_block = 64
    config.sfc_bits = max(8, int(math.ceil(math.log2(resolution + 1))))
    config.intra_strategy = intra_strategy
    config.seed_strategy = seed_strategy
    if intra_strategy == cg.IntraBlockStrategy.PATTERN_LOOKUP:
        config.pattern_table_size = 1024

    sbbf = cg.SpatialBlockedBloomFilter(config)

    # Measure insert time
    t1 = time.perf_counter_ns()
    for x, y, z in voxel_coords:
        sbbf.put3d(int(x), int(y), int(z))
    t2 = time.perf_counter_ns()
    insert_ns = (t2 - t1) / num_voxels

    # Measure query time and FPR
    false_positives = 0
    query_count = 0
    t3 = time.perf_counter_ns()
    for x in range(resolution):
        for y in range(resolution):
            for z in range(resolution):
                query_count += 1
                if sbbf.query3d(x, y, z):
                    if (x, y, z) not in true_set:
                        false_positives += 1
    t4 = time.perf_counter_ns()
    query_ns = (t4 - t3) / query_count

    fpr = false_positives / num_negatives if num_negatives > 0 else 0.0

    return {
        "fpr": fpr,
        "false_positives": false_positives,
        "actual_fill_rate": sbbf.fill_ratio(),
        "memory_bytes": sbbf.memory_bytes(),
        "log_num_blocks": log_num_blocks,
        "insert_ns": insert_ns,
        "query_ns": query_ns,
    }


# ============================================================================
# Main
# ============================================================================


def main():
    print("=" * 60)
    print("SBBF Seed Strategy Benchmark (Voxelized Meshes)")
    print("=" * 60)
    print(f"Hash k: {HASH_K}")
    print(f"Bits per element: {BITS_PER_ELEMENT}")
    print(f"Meshes: {[m['name'] for m in MESHES]}")
    print("=" * 60)

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    results = []
    available_meshes = []

    # Check which meshes are available
    for mesh in MESHES:
        mesh_name = mesh["name"]
        resolution = mesh["resolution"]
        path = HDF5_DIR / f"{mesh_name}_{resolution}.h5"
        if path.exists():
            available_meshes.append(mesh)
        else:
            print(f"Warning: {mesh_name}_{resolution}.h5 not found, skipping.")

    if not available_meshes:
        print("ERROR: No voxel datasets found!")
        print(f"Run: uv run python experiments/python/voxelize_meshes.py")
        sys.exit(1)

    # Count total configurations
    total_configs = (
        len(available_meshes)
        * len(SFC_TYPES)
        * len(INTRA_STRATEGIES)
        * len(SEED_STRATEGIES)
    )
    current = 0

    for mesh in available_meshes:
        mesh_name = mesh["name"]
        resolution = mesh["resolution"]

        print(f"\n--- {mesh_name}_{resolution} ---")

        voxel_coords = load_voxels(mesh_name, resolution)
        print(f"Loaded {len(voxel_coords):,} voxels")

        for sfc_name, sfc_type in SFC_TYPES:
            for intra_name, intra_strategy in INTRA_STRATEGIES:
                for seed_name, seed_strategy in SEED_STRATEGIES:
                    current += 1
                    print(
                        f"[{current}/{total_configs}] {sfc_name} {intra_name} {seed_name}...",
                        end=" ",
                        flush=True,
                    )

                    result = run_benchmark(
                        voxel_coords,
                        resolution,
                        sfc_type,
                        intra_strategy,
                        seed_strategy,
                    )

                    print(
                        f"FPR={result['fpr']:.4%}, "
                        f"insert={result['insert_ns']:.1f}ns, "
                        f"query={result['query_ns']:.1f}ns"
                    )

                    results.append(
                        {
                            "config": {
                                "mesh": mesh_name,
                                "resolution": resolution,
                                "sfc_type": sfc_name,
                                "intra_strategy": intra_name,
                                "seed_strategy": seed_name,
                                "log_num_blocks": result["log_num_blocks"],
                            },
                            "metrics": {
                                "fpr": result["fpr"],
                                "false_positives": result["false_positives"],
                                "actual_fill_rate": result["actual_fill_rate"],
                                "memory_bytes": result["memory_bytes"],
                                "insert_ns": result["insert_ns"],
                                "query_ns": result["query_ns"],
                            },
                        }
                    )

    # Save results
    output = {
        "experiment": "seed_strategy_voxel",
        "timestamp": datetime.now().isoformat(),
        "config": {
            "bits_per_element": BITS_PER_ELEMENT,
            "hash_k": HASH_K,
            "meshes": available_meshes,
        },
        "results": results,
    }

    output_path = OUTPUT_DIR / "voxel_3d.json"
    with open(output_path, "w") as f:
        json.dump(output, f, indent=2)

    print("\n" + "=" * 60)
    print(f"Results saved to: {output_path}")
    print(f"Total configurations: {len(results)}")
    print("=" * 60)


if __name__ == "__main__":
    main()
