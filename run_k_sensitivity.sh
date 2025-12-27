#!/bin/bash
# SBBF K-Sensitivity Benchmark Runner
#
# Runs both synthetic and voxel k-sensitivity experiments
# and generates paper figures.
#
# Usage:
#   ./run_k_sensitivity.sh         # Run all experiments
#   ./run_k_sensitivity.sh --fast  # Skip voxel experiments (faster)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

FAST_MODE=false
if [[ "$1" == "--fast" ]]; then
    FAST_MODE=true
    echo "Fast mode: skipping voxel experiments"
fi

echo "========================================"
echo "SBBF K-Sensitivity Benchmark Suite"
echo "========================================"
echo ""

# Create output directories
mkdir -p sbbf_results/k_sensitivity
mkdir -p sbbf_results/figures

# Build C++ benchmark
echo "Step 1: Building C++ benchmark..."
echo "----------------------------------------"
cd build
cmake .. > /dev/null
make sbbf_k_sensitivity -j$(nproc)
cd ..
echo "Done."
echo ""

# Run synthetic benchmark
echo "Step 2: Running synthetic benchmark..."
echo "----------------------------------------"
./build/sbbf_k_sensitivity
echo ""

# Run voxel benchmark (optional)
if [[ "$FAST_MODE" == false ]]; then
    echo "Step 3: Running voxel benchmark..."
    echo "----------------------------------------"

    # Check if voxel datasets exist
    if [[ -f "datasets/hdf5/bunny_128.h5" && -f "datasets/hdf5/teapot_128.h5" ]]; then
        uv run python experiments/python/sbbf_k_sensitivity.py
    else
        echo "Warning: Voxel datasets not found. Run voxelize_meshes.py first."
        echo "  uv run python experiments/python/voxelize_meshes.py"
    fi
    echo ""
fi

# Generate plots
echo "Step 4: Generating figures..."
echo "----------------------------------------"
uv run python scripts/plot_k_sensitivity.py --all-strategies
echo ""

echo "========================================"
echo "Complete!"
echo ""
echo "Results:"
echo "  - Synthetic: sbbf_results/k_sensitivity/synthetic.json"
if [[ "$FAST_MODE" == false ]]; then
    echo "  - Voxel: sbbf_results/k_sensitivity/voxel.json"
fi
echo ""
echo "Figures:"
echo "  - sbbf_results/figures/k_sensitivity_synthetic.pdf"
echo "  - sbbf_results/figures/k_sensitivity_synthetic_all.pdf"
if [[ "$FAST_MODE" == false ]]; then
    echo "  - sbbf_results/figures/k_sensitivity_voxel.pdf"
    echo "  - sbbf_results/figures/k_sensitivity_voxel_all.pdf"
fi
echo "========================================"
