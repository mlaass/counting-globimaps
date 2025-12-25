#!/usr/bin/env python3
"""
Voxelize standard 3D meshes and save as HDF5.

Downloads Utah teapot and Stanford bunny meshes, voxelizes them at
multiple resolutions (64, 128, 256), and saves voxel coordinates to HDF5.

Usage:
    uv run python experiments/python/voxelize_meshes.py

Output:
    datasets/hdf5/teapot_64.h5, teapot_128.h5, teapot_256.h5
    datasets/hdf5/bunny_64.h5, bunny_128.h5, bunny_256.h5
"""
import urllib.request
from pathlib import Path

import h5py
import numpy as np
import trimesh

# Configuration
RESOLUTIONS = [64, 128, 256]

# Standard 3D mesh URLs
MESHES = {
    "teapot": "https://graphics.stanford.edu/courses/cs148-10-summer/as3/code/as3/teapot.obj",
    "bunny": "https://raw.githubusercontent.com/alecjacobson/common-3d-test-models/master/data/stanford-bunny.obj",
    "dragon": "https://raw.githubusercontent.com/alecjacobson/common-3d-test-models/master/data/xyzrgb_dragon.obj",
    "armadillo": "https://raw.githubusercontent.com/alecjacobson/common-3d-test-models/master/data/armadillo.obj",
    "happy_buddha": "https://raw.githubusercontent.com/alecjacobson/common-3d-test-models/master/data/happy.obj",
    "suzanne": "https://raw.githubusercontent.com/alecjacobson/common-3d-test-models/master/data/suzanne.obj",
    "spot": "https://raw.githubusercontent.com/alecjacobson/common-3d-test-models/master/data/spot.obj",
    "cow": "https://raw.githubusercontent.com/alecjacobson/common-3d-test-models/master/data/cow.obj",
    "cheburashka": "https://raw.githubusercontent.com/alecjacobson/common-3d-test-models/master/data/cheburashka.obj",
    "fandisk": "https://raw.githubusercontent.com/alecjacobson/common-3d-test-models/master/data/fandisk.obj",
    "horse": "https://raw.githubusercontent.com/alecjacobson/common-3d-test-models/master/data/horse.obj",
    "beast": "https://raw.githubusercontent.com/alecjacobson/common-3d-test-models/master/data/beast.obj",
    "ogre": "https://raw.githubusercontent.com/alecjacobson/common-3d-test-models/master/data/ogre.obj",
    "max_planck": "https://raw.githubusercontent.com/alecjacobson/common-3d-test-models/master/data/max-planck.obj",
    "rocker_arm": "https://raw.githubusercontent.com/alecjacobson/common-3d-test-models/master/data/rocker-arm.obj",
    "nefertiti": "https://raw.githubusercontent.com/alecjacobson/common-3d-test-models/master/data/nefertiti.obj",
    "beetle": "https://raw.githubusercontent.com/alecjacobson/common-3d-test-models/master/data/beetle.obj",
}

# Paths
PROJECT_ROOT = Path(__file__).parent.parent.parent
MESH_DIR = PROJECT_ROOT / "datasets" / "meshes"
HDF5_DIR = PROJECT_ROOT / "datasets" / "hdf5"


def download_mesh(name: str, url: str) -> Path:
    """Download mesh if not already present."""
    # Determine file extension from URL
    ext = Path(url).suffix or ".obj"
    output_path = MESH_DIR / f"{name}{ext}"

    if output_path.exists():
        print(f"  {name}: already exists at {output_path}")
        return output_path

    print(f"  {name}: downloading from {url}")
    try:
        urllib.request.urlretrieve(url, output_path)
        print(f"  {name}: saved to {output_path}")
    except Exception as e:
        print(f"  {name}: download failed: {e}")
        raise

    return output_path


def voxelize_mesh(mesh: trimesh.Trimesh, resolution: int) -> np.ndarray:
    """
    Convert mesh to binary voxel grid.

    Args:
        mesh: Trimesh object
        resolution: Target grid resolution (e.g., 64 for 64x64x64)

    Returns:
        (N, 3) array of filled voxel coordinates (integers)
    """
    # Normalize mesh to fit in unit cube, then scale to resolution
    # First center and scale to unit cube
    mesh_centered = mesh.copy()
    mesh_centered.vertices -= mesh_centered.bounds.mean(axis=0)

    # Scale to fit in resolution with some padding
    scale = (resolution - 2) / mesh_centered.extents.max()
    mesh_centered.vertices *= scale

    # Shift to positive coordinates with 1-voxel padding
    mesh_centered.vertices += resolution / 2

    # Voxelize using trimesh
    # pitch = size of each voxel
    pitch = 1.0
    voxels = mesh_centered.voxelized(pitch=pitch)

    # Get filled voxel coordinates
    coords = voxels.points.astype(np.int32)

    # Clamp to valid range
    coords = np.clip(coords, 0, resolution - 1)

    return coords


def save_voxels_hdf5(coords: np.ndarray, path: Path, resolution: int, mesh_name: str):
    """Save voxel coordinates to HDF5."""
    path.parent.mkdir(parents=True, exist_ok=True)

    with h5py.File(path, "w") as f:
        f.create_dataset("coords", data=coords, compression="gzip", compression_opts=9)
        f.attrs["resolution"] = resolution
        f.attrs["num_voxels"] = len(coords)
        f.attrs["mesh_name"] = mesh_name
        f.attrs["fill_ratio"] = len(coords) / (resolution**3)

    print(f"    Saved {len(coords):,} voxels to {path}")
    print(f"    Fill ratio: {len(coords) / (resolution**3):.4%}")


def main():
    """Main entry point."""
    print("=" * 60)
    print("3D Mesh Voxelization Pipeline")
    print("=" * 60)

    # Ensure directories exist
    MESH_DIR.mkdir(parents=True, exist_ok=True)
    HDF5_DIR.mkdir(parents=True, exist_ok=True)

    # Download meshes
    print("\n[1/2] Downloading meshes...")
    mesh_paths = {}
    for name, url in MESHES.items():
        try:
            mesh_paths[name] = download_mesh(name, url)
        except Exception as e:
            print(f"  WARNING: Could not download {name}: {e}")
            continue

    # Voxelize each mesh at each resolution
    print("\n[2/2] Voxelizing meshes...")
    for name, path in mesh_paths.items():
        print(f"\n  Loading {name}...")
        try:
            mesh = trimesh.load(path, force="mesh")
            if isinstance(mesh, trimesh.Scene):
                # If loaded as scene, get the geometry
                mesh = trimesh.util.concatenate(mesh.dump())
            print(f"    Vertices: {len(mesh.vertices):,}, Faces: {len(mesh.faces):,}")
        except Exception as e:
            print(f"    ERROR loading mesh: {e}")
            continue

        for resolution in RESOLUTIONS:
            print(f"\n  Voxelizing {name} at {resolution}x{resolution}x{resolution}...")
            try:
                coords = voxelize_mesh(mesh, resolution)
                output_path = HDF5_DIR / f"{name}_{resolution}.h5"
                save_voxels_hdf5(coords, output_path, resolution, name)
            except Exception as e:
                print(f"    ERROR: {e}")
                continue

    print("\n" + "=" * 60)
    print("Done! Voxelized datasets saved to datasets/hdf5/")
    print("=" * 60)


if __name__ == "__main__":
    main()
