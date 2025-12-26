#!/usr/bin/env python3
"""
Bloom Filter comparison experiments on voxelized 3D meshes.

Compares three bloom filter implementations on voxelized meshes:
- SBBF (Spatial-Blocked Bloom Filter) with native neighbor queries
- BlockedBloomFilter (cache-line aligned)
- GloBiMap (standard binary bloom filter)

Measures false positive rates, applies neighbor-based denoising,
and renders 7-panel comparison images showing raw and denoised results.

Usage:
    uv run python experiments/python/sbbf_voxel_experiment.py

Prerequisites:
    - Build the counting_globimap Python module (cd build && make counting_globimap)
    - Run voxelize_meshes.py first to generate HDF5 datasets
"""
import json
import math
import sys
from datetime import datetime
from pathlib import Path

import h5py
import numpy as np
import pyvista as pv
from tqdm import tqdm

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

# Paths
HDF5_DIR = PROJECT_ROOT / "datasets" / "hdf5"
RESULTS_DIR = PROJECT_ROOT / "sbbf_results"
FIGURES_DIR = PROJECT_ROOT / "sbbf_results" / "figures"


# ============================================================
# EXPERIMENT CONFIGURATION - Edit this list to define experiments
# ============================================================

EXPERIMENTS = [
    # Bunny at 64³ - compare filter sizes
    {
        "name": "bunny_64_small",
        "mesh": "bunny",
        "resolution": 64,
        "log_num_blocks": 12,  # 4K blocks = 32 KB
        "hash_k": 4,
        "sfc_type": "HILBERT_3D",
        "min_neighbors": 2,
    },
    {
        "name": "bunny_64_medium",
        "mesh": "bunny",
        "resolution": 64,
        "log_num_blocks": 14,  # 16K blocks = 128 KB
        "hash_k": 4,
        "sfc_type": "HILBERT_3D",
        "min_neighbors": 2,
    },
    {
        "name": "bunny_64_large",
        "mesh": "bunny",
        "resolution": 64,
        "log_num_blocks": 16,  # 64K blocks = 512 KB
        "hash_k": 4,
        "sfc_type": "HILBERT_3D",
        "min_neighbors": 2,
    },
    # Teapot at 64³
    {
        "name": "teapot_64_medium",
        "mesh": "teapot",
        "resolution": 64,
        "log_num_blocks": 14,
        "hash_k": 4,
        "sfc_type": "HILBERT_3D",
        "min_neighbors": 2,
    },
    # Compare Hilbert vs Morton
    {
        "name": "bunny_64_morton",
        "mesh": "bunny",
        "resolution": 64,
        "log_num_blocks": 14,
        "hash_k": 4,
        "sfc_type": "MORTON_3D",
        "min_neighbors": 2,
    },
    # Higher resolution experiments (slower)
    {
        "name": "bunny_128_medium",
        "mesh": "bunny",
        "resolution": 128,
        "log_num_blocks": 16,
        "hash_k": 4,
        "sfc_type": "HILBERT_3D",
        "min_neighbors": 2,
    },
    {
        "name": "teapot_128_medium",
        "mesh": "teapot",
        "resolution": 128,
        "log_num_blocks": 16,
        "hash_k": 4,
        "sfc_type": "HILBERT_3D",
        "min_neighbors": 2,
    },
]

# ============================================================
# End of configuration
# ============================================================


def fix_orientation(coords: np.ndarray) -> np.ndarray:
    """Swap Y and Z axes to correct vertical orientation."""
    if len(coords) == 0:
        return coords
    # Swap columns: [x, y, z] -> [x, z, y]
    return coords[:, [0, 2, 1]]


def count_neighbors_3d(bf, x: int, y: int, z: int, full_26: bool = True, resolution: int = 256) -> int:
    """Count occupied neighbors by querying individually (for standard BFs)."""
    count = 0
    for dx in [-1, 0, 1]:
        for dy in [-1, 0, 1]:
            for dz in [-1, 0, 1]:
                if dx == 0 and dy == 0 and dz == 0:
                    continue
                if not full_26 and abs(dx) + abs(dy) + abs(dz) > 1:
                    continue  # 6-connected only
                nx, ny, nz = x + dx, y + dy, z + dz
                # Skip out-of-bounds neighbors
                if nx < 0 or ny < 0 or nz < 0:
                    continue
                if nx >= resolution or ny >= resolution or nz >= resolution:
                    continue
                # Use appropriate query method
                if hasattr(bf, 'query'):
                    if bf.query([nx, ny, nz]):
                        count += 1
                elif hasattr(bf, 'get'):
                    if bf.get([nx, ny, nz]):
                        count += 1
    return count


def load_voxels(mesh_name: str, resolution: int) -> np.ndarray:
    """Load voxel coordinates from HDF5 file."""
    path = HDF5_DIR / f"{mesh_name}_{resolution}.h5"
    if not path.exists():
        raise FileNotFoundError(f"Voxel file not found: {path}")

    with h5py.File(path, "r") as f:
        coords = f["coords"][:]
        print(f"  Loaded {len(coords):,} voxels from {path.name}")
    return coords


def get_sfc_type(name: str) -> cg.SFCType:
    """Convert string SFC type to enum."""
    sfc_map = {
        "HILBERT_3D": cg.SFCType.HILBERT_3D,
        "MORTON_3D": cg.SFCType.MORTON_3D,
        "HILBERT_2D": cg.SFCType.HILBERT_2D,
        "MORTON_2D": cg.SFCType.MORTON_2D,
    }
    return sfc_map.get(name, cg.SFCType.HILBERT_3D)


def run_blocked_bf(
    voxel_coords: np.ndarray,
    true_set: set,
    resolution: int,
    min_neighbors: int,
) -> dict:
    """Run BlockedBloomFilter experiment."""
    num_voxels = len(voxel_coords)
    num_negatives = resolution**3 - num_voxels

    # Configure BlockedBF with similar memory budget
    bf_config = cg.BlockedBFConfig()
    bf_config.expected_items = num_voxels
    bf_config.false_positive_rate = 0.01

    bf = cg.BlockedBloomFilter(bf_config)

    # Insert voxels
    for x, y, z in voxel_coords:
        bf.put([int(x), int(y), int(z)])

    # Query all grid points
    queried_points = []
    false_positives = []

    for x in range(resolution):
        for y in range(resolution):
            for z in range(resolution):
                if bf.query([x, y, z]):
                    queried_points.append((x, y, z))
                    if (x, y, z) not in true_set:
                        false_positives.append((x, y, z))

    queried = np.array(queried_points) if queried_points else np.empty((0, 3))
    fps = np.array(false_positives) if false_positives else np.empty((0, 3))

    raw_fpr = len(fps) / num_negatives if num_negatives > 0 else 0.0

    # Apply denoising using manual neighbor queries
    denoised = []
    for x, y, z in queried:
        neighbor_count = count_neighbors_3d(bf, int(x), int(y), int(z), full_26=True, resolution=resolution)
        if neighbor_count >= min_neighbors:
            denoised.append((x, y, z))

    denoised = np.array(denoised) if denoised else np.empty((0, 3))

    denoised_fps = sum(1 for p in denoised if tuple(p) not in true_set)
    denoised_fpr = denoised_fps / num_negatives if num_negatives > 0 else 0.0

    return {
        "queried": queried,
        "fps": fps,
        "denoised": denoised,
        "raw_fpr": raw_fpr,
        "raw_fps": len(fps),
        "denoised_fpr": denoised_fpr,
        "denoised_fps": denoised_fps,
        "memory_kb": bf.memory_bytes() / 1024,
    }


def run_globimap(
    voxel_coords: np.ndarray,
    true_set: set,
    resolution: int,
    min_neighbors: int,
) -> dict:
    """Run GloBiMap (binary BF) experiment."""
    num_voxels = len(voxel_coords)
    num_negatives = resolution**3 - num_voxels

    # Configure GloBiMap with similar memory budget
    logm = max(16, int(math.ceil(math.log2(num_voxels * 10))))

    bf = cg.globimap()
    bf.configure(8, logm)  # k=8 hash functions

    # Insert voxels
    for x, y, z in voxel_coords:
        bf.put([int(x), int(y), int(z)])

    # Query all grid points
    queried_points = []
    false_positives = []

    for x in range(resolution):
        for y in range(resolution):
            for z in range(resolution):
                if bf.get([x, y, z]):
                    queried_points.append((x, y, z))
                    if (x, y, z) not in true_set:
                        false_positives.append((x, y, z))

    queried = np.array(queried_points) if queried_points else np.empty((0, 3))
    fps = np.array(false_positives) if false_positives else np.empty((0, 3))

    raw_fpr = len(fps) / num_negatives if num_negatives > 0 else 0.0

    # Apply denoising using manual neighbor queries
    denoised = []
    for x, y, z in queried:
        neighbor_count = count_neighbors_3d(bf, int(x), int(y), int(z), full_26=True, resolution=resolution)
        if neighbor_count >= min_neighbors:
            denoised.append((x, y, z))

    denoised = np.array(denoised) if denoised else np.empty((0, 3))

    denoised_fps = sum(1 for p in denoised if tuple(p) not in true_set)
    denoised_fpr = denoised_fps / num_negatives if num_negatives > 0 else 0.0

    # GloBiMap doesn't expose memory_bytes directly, calculate from logm
    memory_kb = (2**logm) / 8 / 1024

    return {
        "queried": queried,
        "fps": fps,
        "denoised": denoised,
        "raw_fpr": raw_fpr,
        "raw_fps": len(fps),
        "denoised_fpr": denoised_fpr,
        "denoised_fps": denoised_fps,
        "memory_kb": memory_kb,
    }


def get_seed_strategy(name: str) -> cg.SeedStrategy:
    """Convert string seed strategy to enum."""
    strategy_map = {
        "XOR": cg.SeedStrategy.XOR,
        "MULTIPLY_SHIFT": cg.SeedStrategy.MULTIPLY_SHIFT,
    }
    return strategy_map.get(name, cg.SeedStrategy.XOR)


def run_sbbf(
    voxel_coords: np.ndarray,
    true_set: set,
    resolution: int,
    min_neighbors: int,
    sfc_type: cg.SFCType,
    log_num_blocks: int,
    hash_k: int,
    seed_strategy: cg.SeedStrategy,
    strategy_name: str,
) -> dict:
    """Run SBBF with a specific seed strategy."""
    num_negatives = resolution**3 - len(voxel_coords)

    # Configure SBBF
    config = cg.SBBFConfig()
    config.sfc_type = sfc_type
    config.log_num_blocks = log_num_blocks
    config.hash_k = hash_k
    config.bits_per_block = 64
    config.sfc_bits = max(8, int(math.ceil(math.log2(resolution + 1))))
    config.seed_strategy = seed_strategy

    sbbf = cg.SpatialBlockedBloomFilter(config)

    # Insert all voxels
    for x, y, z in tqdm(voxel_coords, desc=f"  Insert ({strategy_name})", leave=False):
        sbbf.put3d(int(x), int(y), int(z))

    # Query all grid points
    queried_points = []
    false_positives = []

    for x in tqdm(range(resolution), desc=f"  Query ({strategy_name})", leave=False):
        for y in range(resolution):
            for z in range(resolution):
                if sbbf.query3d(x, y, z):
                    queried_points.append((x, y, z))
                    if (x, y, z) not in true_set:
                        false_positives.append((x, y, z))

    queried = np.array(queried_points) if queried_points else np.empty((0, 3))
    fps = np.array(false_positives) if false_positives else np.empty((0, 3))

    raw_fpr = len(fps) / num_negatives if num_negatives > 0 else 0.0

    # Apply denoising
    denoised = []
    for x, y, z in tqdm(queried, desc=f"  Denoise ({strategy_name})", leave=False):
        neighbor_count = sbbf.neighbors3d(int(x), int(y), int(z), full_26=True)
        if neighbor_count >= min_neighbors:
            denoised.append((x, y, z))

    denoised = np.array(denoised) if denoised else np.empty((0, 3))

    denoised_fps = sum(1 for p in denoised if tuple(p) not in true_set)
    denoised_fpr = denoised_fps / num_negatives if num_negatives > 0 else 0.0

    fps_removed = len(fps) - denoised_fps
    correction_rate = fps_removed / len(fps) if len(fps) > 0 else 0.0

    return {
        "queried": queried,
        "fps": fps,
        "denoised": denoised,
        "raw_fpr": raw_fpr,
        "raw_fps": len(fps),
        "denoised_fpr": denoised_fpr,
        "denoised_fps": denoised_fps,
        "fps_removed": fps_removed,
        "correction_rate": correction_rate,
        "memory_kb": sbbf.memory_bytes() / 1024,
        "fill_ratio": sbbf.fill_ratio(),
    }


def run_single_experiment(exp: dict) -> dict:
    """
    Run a single experiment comparing SBBF (XOR vs MULTIPLY_SHIFT), BlockedBF, and GloBiMap.

    Returns dict with config and stats.
    """
    mesh_name = exp["mesh"]
    resolution = exp["resolution"]
    log_num_blocks = exp["log_num_blocks"]
    hash_k = exp["hash_k"]
    sfc_type = get_sfc_type(exp["sfc_type"])
    min_neighbors = exp.get("min_neighbors", 2)

    # Load voxels
    voxel_coords = load_voxels(mesh_name, resolution)
    true_set = set(map(tuple, voxel_coords))

    # Run SBBF with XOR strategy
    print("  Running SBBF (XOR)...")
    sbbf_xor = run_sbbf(
        voxel_coords, true_set, resolution, min_neighbors,
        sfc_type, log_num_blocks, hash_k, cg.SeedStrategy.XOR, "XOR"
    )
    print(f"  SBBF XOR FPR: {sbbf_xor['raw_fpr']:.4%} -> {sbbf_xor['denoised_fpr']:.4%}")

    # Run SBBF with MULTIPLY_SHIFT strategy
    print("  Running SBBF (MULTIPLY_SHIFT)...")
    sbbf_ms = run_sbbf(
        voxel_coords, true_set, resolution, min_neighbors,
        sfc_type, log_num_blocks, hash_k, cg.SeedStrategy.MULTIPLY_SHIFT, "MS"
    )
    print(f"  SBBF MultShift FPR: {sbbf_ms['raw_fpr']:.4%} -> {sbbf_ms['denoised_fpr']:.4%}")

    # Run BlockedBloomFilter comparison
    print("  Running BlockedBloomFilter...")
    blocked_result = run_blocked_bf(voxel_coords, true_set, resolution, min_neighbors)
    print(f"  BlockedBF FPR: {blocked_result['raw_fpr']:.4%} -> {blocked_result['denoised_fpr']:.4%}")

    # Run GloBiMap comparison
    print("  Running GloBiMap...")
    globimap_result = run_globimap(voxel_coords, true_set, resolution, min_neighbors)
    print(f"  GloBiMap FPR: {globimap_result['raw_fpr']:.4%} -> {globimap_result['denoised_fpr']:.4%}")

    return {
        "name": exp["name"],
        "config": {
            "mesh": mesh_name,
            "resolution": resolution,
            "log_num_blocks": log_num_blocks,
            "hash_k": hash_k,
            "sfc_type": exp["sfc_type"],
            "min_neighbors": min_neighbors,
        },
        "stats": {
            "num_voxels": len(voxel_coords),
            "grid_points": resolution**3,
            # SBBF XOR stats
            "sbbf_xor_memory_kb": sbbf_xor["memory_kb"],
            "sbbf_xor_fill_ratio": sbbf_xor["fill_ratio"],
            "sbbf_xor_raw_fpr": sbbf_xor["raw_fpr"],
            "sbbf_xor_raw_fps": sbbf_xor["raw_fps"],
            "sbbf_xor_denoised_fpr": sbbf_xor["denoised_fpr"],
            "sbbf_xor_denoised_fps": sbbf_xor["denoised_fps"],
            "sbbf_xor_fps_removed": sbbf_xor["fps_removed"],
            "sbbf_xor_correction_rate": sbbf_xor["correction_rate"],
            # SBBF MULTIPLY_SHIFT stats
            "sbbf_ms_memory_kb": sbbf_ms["memory_kb"],
            "sbbf_ms_fill_ratio": sbbf_ms["fill_ratio"],
            "sbbf_ms_raw_fpr": sbbf_ms["raw_fpr"],
            "sbbf_ms_raw_fps": sbbf_ms["raw_fps"],
            "sbbf_ms_denoised_fpr": sbbf_ms["denoised_fpr"],
            "sbbf_ms_denoised_fps": sbbf_ms["denoised_fps"],
            "sbbf_ms_fps_removed": sbbf_ms["fps_removed"],
            "sbbf_ms_correction_rate": sbbf_ms["correction_rate"],
            # BlockedBF stats
            "blocked_memory_kb": blocked_result["memory_kb"],
            "blocked_raw_fpr": blocked_result["raw_fpr"],
            "blocked_raw_fps": blocked_result["raw_fps"],
            "blocked_denoised_fpr": blocked_result["denoised_fpr"],
            "blocked_denoised_fps": blocked_result["denoised_fps"],
            # GloBiMap stats
            "globimap_memory_kb": globimap_result["memory_kb"],
            "globimap_raw_fpr": globimap_result["raw_fpr"],
            "globimap_raw_fps": globimap_result["raw_fps"],
            "globimap_denoised_fpr": globimap_result["denoised_fpr"],
            "globimap_denoised_fps": globimap_result["denoised_fps"],
        },
        # Keep arrays for rendering (not saved to JSON)
        "_ground_truth": voxel_coords,
        "_sbbf_xor_queried": sbbf_xor["queried"],
        "_sbbf_xor_fps": sbbf_xor["fps"],
        "_sbbf_xor_denoised": sbbf_xor["denoised"],
        "_sbbf_ms_queried": sbbf_ms["queried"],
        "_sbbf_ms_fps": sbbf_ms["fps"],
        "_sbbf_ms_denoised": sbbf_ms["denoised"],
        "_blocked_queried": blocked_result["queried"],
        "_blocked_fps": blocked_result["fps"],
        "_blocked_denoised": blocked_result["denoised"],
        "_globimap_queried": globimap_result["queried"],
        "_globimap_fps": globimap_result["fps"],
        "_globimap_denoised": globimap_result["denoised"],
    }


def render_panel_raw(plotter, queried, fps, title, fpr, point_size):
    """Render a raw query panel (TP=blue, FP=red)."""
    plotter.add_title(f"{title}\n(FPR={fpr:.2%})", font_size=8)
    if len(queried) > 0:
        fp_set = set(map(tuple, fps)) if len(fps) > 0 else set()
        tps = np.array([p for p in queried if tuple(p) not in fp_set])
        if len(tps) > 0:
            plotter.add_mesh(
                pv.PolyData(fix_orientation(tps).astype(float)),
                color="blue", point_size=point_size, render_points_as_spheres=True
            )
        if len(fps) > 0:
            plotter.add_mesh(
                pv.PolyData(fix_orientation(fps).astype(float)),
                color="red", point_size=point_size + 1, render_points_as_spheres=True
            )
    plotter.add_axes()
    plotter.camera_position = "iso"


def render_panel_denoised(plotter, denoised, title, fpr, point_size):
    """Render a denoised panel (cyan)."""
    plotter.add_title(f"{title}\n(FPR={fpr:.2%})", font_size=8)
    if len(denoised) > 0:
        cloud = pv.PolyData(fix_orientation(denoised).astype(float))
        plotter.add_mesh(cloud, color="cyan", point_size=point_size, render_points_as_spheres=True)
    plotter.add_axes()
    plotter.camera_position = "iso"


def render_comparison(
    result: dict,
    output_path: Path,
):
    """
    Render side-by-side comparison with PyVista.

    Creates a 1x9 subplot:
    - Ground truth (green)
    - SBBF XOR Raw (TP=blue, FP=red)
    - SBBF XOR Denoised (cyan)
    - SBBF MultShift Raw (TP=blue, FP=red)
    - SBBF MultShift Denoised (cyan)
    - BlockedBF Raw (TP=blue, FP=red)
    - BlockedBF Denoised (cyan)
    - GloBiMap Raw (TP=blue, FP=red)
    - GloBiMap Denoised (cyan)
    """
    output_path.parent.mkdir(parents=True, exist_ok=True)

    ground_truth = result["_ground_truth"]
    resolution = result["config"]["resolution"]
    stats = result["stats"]

    # Use off-screen rendering
    pv.OFF_SCREEN = True

    plotter = pv.Plotter(shape=(1, 9), window_size=(5400, 600), off_screen=True)

    point_size = max(2, 8 - resolution // 64)

    # Panel 0: Ground truth (green)
    plotter.subplot(0, 0)
    plotter.add_title(f"Ground Truth\n({len(ground_truth):,} voxels)", font_size=8)
    if len(ground_truth) > 0:
        cloud = pv.PolyData(fix_orientation(ground_truth).astype(float))
        plotter.add_mesh(cloud, color="green", point_size=point_size, render_points_as_spheres=True)
    plotter.add_axes()
    plotter.camera_position = "iso"

    # Panel 1: SBBF XOR Raw
    plotter.subplot(0, 1)
    render_panel_raw(plotter, result["_sbbf_xor_queried"], result["_sbbf_xor_fps"],
                     "SBBF XOR Raw", stats["sbbf_xor_raw_fpr"], point_size)

    # Panel 2: SBBF XOR Denoised
    plotter.subplot(0, 2)
    render_panel_denoised(plotter, result["_sbbf_xor_denoised"],
                          "SBBF XOR Denoised", stats["sbbf_xor_denoised_fpr"], point_size)

    # Panel 3: SBBF MultShift Raw
    plotter.subplot(0, 3)
    render_panel_raw(plotter, result["_sbbf_ms_queried"], result["_sbbf_ms_fps"],
                     "SBBF MS Raw", stats["sbbf_ms_raw_fpr"], point_size)

    # Panel 4: SBBF MultShift Denoised
    plotter.subplot(0, 4)
    render_panel_denoised(plotter, result["_sbbf_ms_denoised"],
                          "SBBF MS Denoised", stats["sbbf_ms_denoised_fpr"], point_size)

    # Panel 5: BlockedBF Raw
    plotter.subplot(0, 5)
    render_panel_raw(plotter, result["_blocked_queried"], result["_blocked_fps"],
                     "BlockedBF Raw", stats["blocked_raw_fpr"], point_size)

    # Panel 6: BlockedBF Denoised
    plotter.subplot(0, 6)
    render_panel_denoised(plotter, result["_blocked_denoised"],
                          "BlockedBF Denoised", stats["blocked_denoised_fpr"], point_size)

    # Panel 7: GloBiMap Raw
    plotter.subplot(0, 7)
    render_panel_raw(plotter, result["_globimap_queried"], result["_globimap_fps"],
                     "GloBiMap Raw", stats["globimap_raw_fpr"], point_size)

    # Panel 8: GloBiMap Denoised
    plotter.subplot(0, 8)
    render_panel_denoised(plotter, result["_globimap_denoised"],
                          "GloBiMap Denoised", stats["globimap_denoised_fpr"], point_size)

    # Save screenshot
    plotter.screenshot(str(output_path))
    plotter.close()

    print(f"  Saved: {output_path.name}")


def save_results(results: list[dict], output_path: Path):
    """Save experiment results to JSON (without numpy arrays)."""
    output_path.parent.mkdir(parents=True, exist_ok=True)

    # Remove internal arrays before saving
    clean_results = []
    for r in results:
        clean = {
            "name": r["name"],
            "config": r["config"],
            "stats": r["stats"],
            "image": f"{r['name']}.png",
        }
        clean_results.append(clean)

    data = {
        "timestamp": datetime.now().isoformat(),
        "num_experiments": len(clean_results),
        "experiments": clean_results,
    }

    with open(output_path, "w") as f:
        json.dump(data, f, indent=2)

    print(f"\nSaved results to {output_path}")


def main():
    """Main entry point."""
    print("=" * 60)
    print("Bloom Filter Comparison: SBBF vs BlockedBF vs GloBiMap")
    print(f"Running {len(EXPERIMENTS)} experiments")
    print("=" * 60)

    # Ensure directories exist
    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    FIGURES_DIR.mkdir(parents=True, exist_ok=True)

    results = []

    for i, exp in enumerate(EXPERIMENTS, 1):
        print(f"\n{'=' * 60}")
        print(f"[{i}/{len(EXPERIMENTS)}] {exp['name']}")
        print("=" * 60)

        try:
            result = run_single_experiment(exp)
            results.append(result)

            # Render comparison image
            img_path = FIGURES_DIR / f"{exp['name']}.png"
            render_comparison(result, img_path)

        except FileNotFoundError as e:
            print(f"  SKIPPED: {e}")
            continue
        except Exception as e:
            print(f"  ERROR: {e}")
            continue

    # Save all results to JSON
    if results:
        save_results(results, RESULTS_DIR / "voxel_experiments.json")

    print("\n" + "=" * 60)
    print("Experiments complete!")
    print(f"  Results: {RESULTS_DIR / 'voxel_experiments.json'}")
    print(f"  Images:  {FIGURES_DIR}/*.png")
    print("=" * 60)


if __name__ == "__main__":
    main()
