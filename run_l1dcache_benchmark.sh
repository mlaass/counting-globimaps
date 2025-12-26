#!/bin/bash
#
# Run L1-dcache benchmark for SBBF paper
#
# Measures L1-dcache hit rates during 3D voxel volume traversals.
# Compares SBBF-Hilbert (SFC-ordered) vs BlockedBF (hash-based).
#
# Prerequisites:
#   - Build the benchmark: cd build && cmake .. && make l1dcache_benchmark
#   - Enable perf counters: sudo sysctl kernel.perf_event_paranoid=1
#
# Usage:
#   ./run_l1dcache_benchmark.sh
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

OUTPUT_DIR="sbbf_results/l1dcache"
FIGURES_DIR="sbbf_results/figures"

# Check if benchmark exists
if [[ ! -f "build/l1dcache_benchmark" ]]; then
    echo "ERROR: build/l1dcache_benchmark not found"
    echo "Build it first: cd build && cmake .. && make l1dcache_benchmark"
    exit 1
fi

# Check perf_event_paranoid
PARANOID=$(cat /proc/sys/kernel/perf_event_paranoid 2>/dev/null || echo "unknown")
if [[ "$PARANOID" != "unknown" && "$PARANOID" -gt 1 ]]; then
    echo "WARNING: kernel.perf_event_paranoid=$PARANOID (need <= 1)"
    echo "Run: sudo sysctl kernel.perf_event_paranoid=1"
    echo ""
    read -p "Continue anyway? [y/N] " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"
mkdir -p "$FIGURES_DIR"

echo "=========================================="
echo "L1-dcache Benchmark for SBBF Paper"
echo "=========================================="
echo ""
echo "Output directory: $OUTPUT_DIR"
echo "Figures directory: $FIGURES_DIR"
echo ""

# Run benchmark
echo "Running benchmark..."
./build/l1dcache_benchmark \
    --grid-sizes 64,128,256 \
    --bits-per-elem 10 \
    --hash-k 4 \
    --warmup 3 \
    --iterations 5 \
    --output "$OUTPUT_DIR"

# Generate figures
echo ""
echo "Generating figures..."
uv run scripts/plot_l1dcache.py \
    --input "$OUTPUT_DIR/l1dcache_results.json" \
    --output "$FIGURES_DIR" \
    --combined

echo ""
echo "=========================================="
echo "Done!"
echo "=========================================="
echo ""
echo "Results: $OUTPUT_DIR/l1dcache_results.json"
echo "Figures: $FIGURES_DIR/l1dcache_*.pdf"
echo ""
