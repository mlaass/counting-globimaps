#!/bin/bash
# SBBF Seed Strategy Benchmark Runner
#
# Compares XOR vs MULTIPLY_SHIFT seed strategies across:
# - Synthetic 2D/3D
# - GDELT 2D
# - Voxelized meshes
#
# Usage:
#   ./run_seed_strategy_benchmark.sh         # Run all experiments
#   ./run_seed_strategy_benchmark.sh --fast  # Skip voxel experiments (faster)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

FAST_MODE=false
if [[ "$1" == "--fast" ]]; then
    FAST_MODE=true
    echo "Fast mode: skipping voxel experiments"
fi

echo "========================================"
echo "SBBF Seed Strategy Benchmark Suite"
echo "========================================"
echo "Comparing XOR vs MULTIPLY_SHIFT"
echo ""

# Create output directories
mkdir -p sbbf_results/seed_strategy
mkdir -p sbbf_results/figures

# Step 1: Build C++ benchmark
echo "Step 1: Building C++ benchmark..."
echo "----------------------------------------"
cd build
cmake .. > /dev/null
make sbbf_seed_strategy_benchmark -j$(nproc)
cd ..
echo "Done."
echo ""

# Step 2: Run C++ benchmarks (synthetic + GDELT)
echo "Step 2: Running C++ benchmarks..."
echo "----------------------------------------"
./build/sbbf_seed_strategy_benchmark
echo ""

# Step 3: Run Python voxel benchmark (optional)
if [[ "$FAST_MODE" == false ]]; then
    echo "Step 3: Running voxel benchmark..."
    echo "----------------------------------------"

    # Check if any voxel datasets exist
    if ls datasets/hdf5/*_128.h5 1> /dev/null 2>&1; then
        uv run python experiments/python/sbbf_seed_strategy_voxel.py
    else
        echo "Warning: Voxel datasets not found. Run voxelize_meshes.py first."
        echo "  uv run python experiments/python/voxelize_meshes.py"
    fi
    echo ""
fi

# Step 4: Generate figures
echo "Step 4: Generating figures..."
echo "----------------------------------------"
uv run python scripts/plot_seed_strategy.py
echo ""

echo "========================================="
echo "Complete!"
echo ""
echo "Results:"
echo "  - Uniform 2D: sbbf_results/seed_strategy/synthetic_2d.json"
echo "  - Uniform 3D: sbbf_results/seed_strategy/synthetic_3d.json"
echo "  - Clustered 2D: sbbf_results/seed_strategy/clustered_2d.json"
echo "  - Clustered 3D: sbbf_results/seed_strategy/clustered_3d.json"
echo "  - GDELT 2D: sbbf_results/seed_strategy/gdelt_2d.json"
if [[ "$FAST_MODE" == false ]]; then
    echo "  - Voxel 3D: sbbf_results/seed_strategy/voxel_3d.json"
fi
echo ""
echo "Figures:"
echo "  - sbbf_results/figures/seed_strategy_overview.pdf (2x3 grid)"
echo "  - sbbf_results/figures/seed_strategy_synthetic_2d.pdf"
echo "  - sbbf_results/figures/seed_strategy_synthetic_3d.pdf"
echo "  - sbbf_results/figures/seed_strategy_clustered_2d.pdf"
echo "  - sbbf_results/figures/seed_strategy_clustered_3d.pdf"
echo "  - sbbf_results/figures/seed_strategy_gdelt.pdf"
if [[ "$FAST_MODE" == false ]]; then
    echo "  - sbbf_results/figures/seed_strategy_voxel_all.pdf"
fi
echo "  - sbbf_results/figures/seed_strategy_latency.pdf"
echo "========================================="
