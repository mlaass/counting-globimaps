#!/bin/bash
# Scalability benchmark for SBBF paper figures (2D + 3D)
# Runs benchmarks at different scales with appropriate filter sizes

set -e

OUTPUT_DIR="sbbf_results/scalability"
MAX_ELEMENTS=2147483648  # 2^31
SKIP_LARGE=false
BITS_PER_ELEMENT=10

# Compute log_blocks for target bits/element
# Formula: log_blocks = round(log2(elements * bits_per_element / 64))
compute_log_blocks() {
    local elements=$1
    local bpe=$2
    python3 -c "import math; print(round(math.log2($elements * $bpe / 64)))"
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --max-elements)
            MAX_ELEMENTS="$2"
            shift 2
            ;;
        --bits-per-element)
            BITS_PER_ELEMENT="$2"
            shift 2
            ;;
        --skip-large)
            SKIP_LARGE=true
            shift
            ;;
        --output-dir)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        -h|--help)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --max-elements N      Skip benchmarks with more than N elements (default: 500000000)"
            echo "  --bits-per-element B  Target bits per element (default: 10)"
            echo "  --skip-large          Skip benchmarks with >100M elements"
            echo "  --output-dir DIR      Output directory (default: sbbf_results/scalability)"
            echo "  -h, --help            Show this help"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

if [ "$SKIP_LARGE" = true ]; then
    MAX_ELEMENTS=100000000
fi

mkdir -p "$OUTPUT_DIR"

echo "=========================================="
echo "  SBBF Scalability Benchmark Suite"
echo "=========================================="
echo ""
echo "Output directory: $OUTPUT_DIR"
echo "Max elements: $MAX_ELEMENTS"
echo "Target bits/element: $BITS_PER_ELEMENT"
echo ""

# Check if benchmark binary exists
if [ ! -f "./build/sbbf_benchmark" ]; then
    echo "Error: ./build/sbbf_benchmark not found"
    echo "Please build with: cd build && make sbbf_benchmark"
    exit 1
fi

# Powers of 2 for more even scaling
ELEMENT_COUNTS=($(python3 -c "print(' '.join(str(2**i) for i in range(17, 32, 2)))"))


# Debug: print the element counts
echo "Element counts: ${ELEMENT_COUNTS[@]}"


# Count total runs
TOTAL_RUNS=0
for elements in "${ELEMENT_COUNTS[@]}"; do
    if [ "$elements" -le "$MAX_ELEMENTS" ]; then
        TOTAL_RUNS=$((TOTAL_RUNS + 1))
    fi
done

echo "Running $TOTAL_RUNS benchmark configurations..."
echo ""

START_TIME=$(date +%s)
COMPLETED_RUNS=0

for elements in "${ELEMENT_COUNTS[@]}"; do
    if [ "$elements" -gt "$MAX_ELEMENTS" ]; then
        echo "Skipping $elements elements (exceeds --max-elements $MAX_ELEMENTS)"
        continue
    fi

    log_blocks=$(compute_log_blocks "$elements" "$BITS_PER_ELEMENT")
    COMPLETED_RUNS=$((COMPLETED_RUNS + 1))

    # Calculate actual bits/element for display
    actual_bpe=$(python3 -c "print(f'{2**$log_blocks * 64 / $elements:.1f}')")

    echo "========================================"
    echo "[$COMPLETED_RUNS/$TOTAL_RUNS] Running benchmark:"
    echo "  Elements:   $elements"
    echo "  Log blocks: $log_blocks"
    echo "  Bits/elem:  $actual_bpe"
    echo "========================================"

    ./build/sbbf_benchmark \
        --scenario sweep \
        --elements "$elements" \
        --log-blocks "$log_blocks" \
        --sfc morton,hilbert \
        --strategy double_hash,pattern_lookup \
        --dims 2,3 \
        --distribution uniform \
        --baseline reduced \
        --output "$OUTPUT_DIR/scale_${elements}.json" \
        2>&1 | tee "$OUTPUT_DIR/scale_${elements}.log"

    echo ""
done

END_TIME=$(date +%s)
ELAPSED=$((END_TIME - START_TIME))

echo "=========================================="
echo "  Benchmark Complete!"
echo "=========================================="
echo ""
echo "Results saved to: $OUTPUT_DIR/"
echo "Total time: ${ELAPSED}s"
echo ""
echo "To generate figures, run:"
echo "  uv run scripts/plot_scalability.py --input $OUTPUT_DIR --output sbbf_results/figures"
