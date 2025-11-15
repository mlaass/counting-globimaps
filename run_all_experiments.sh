#!/usr/bin/env bash

# Experiment Runner Script
# Runs all comparison experiments sequentially and collects results

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Build directory
BUILD_DIR="build"

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}Error: Build directory $BUILD_DIR not found${NC}"
    echo "Please run 'mkdir build && cd build && cmake .. && make' first"
    exit 1
fi

# Create timestamped results directory
TIMESTAMP=$(date +"%Y-%m-%d_%H-%M-%S")
RESULTS_DIR="results_$TIMESTAMP"
mkdir -p "$RESULTS_DIR"

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Counting Bloom Filter Experiment Runner${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo -e "Results directory: ${GREEN}$RESULTS_DIR${NC}"
echo ""

# Track experiment status
declare -a PASSED=()
declare -a FAILED=()
TOTAL_START=$(date +%s)

# Function to run an experiment
run_experiment() {
    local name=$1
    local executable=$2
    local description=$3

    echo -e "${YELLOW}---${NC} ${BLUE}$name${NC} ${YELLOW}---${NC}"
    echo "$description"
    echo ""

    EXP_START=$(date +%s)

    if [ -f "$BUILD_DIR/$executable" ]; then
        if "$BUILD_DIR/$executable" > "$RESULTS_DIR/${name}_output.log" 2>&1; then
            EXP_END=$(date +%s)
            DURATION=$((EXP_END - EXP_START))
            echo -e "${GREEN}✓ PASSED${NC} (${DURATION}s)"
            PASSED+=("$name")
        else
            EXP_END=$(date +%s)
            DURATION=$((EXP_END - EXP_START))
            echo -e "${RED}✗ FAILED${NC} (${DURATION}s)"
            echo -e "${YELLOW}  See log: $RESULTS_DIR/${name}_output.log${NC}"
            FAILED+=("$name")
        fi
    else
        echo -e "${RED}✗ EXECUTABLE NOT FOUND${NC}"
        echo -e "${YELLOW}  Expected: $BUILD_DIR/$executable${NC}"
        FAILED+=("$name")
    fi

    echo ""
}

# Run experiments in order
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Running Experiments${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# 1. Quick baseline comparison
run_experiment \
    "compare_all" \
    "compare_all_implementations" \
    "Quick synthetic baseline (3 scenarios: tiny/medium/large budgets)"

# 2. Cosine distribution test
run_experiment \
    "cosine_compare" \
    "globimap_test_cos_compare" \
    "Synthetic cosine distribution (65K inserts, deterministic pattern)"

# 3. K parameter sensitivity
run_experiment \
    "k_sensitivity" \
    "globimap_test_k_compare" \
    "K parameter sweep (k=1,2,4,8,16,32 on 100K points)"

# 4. Real dataset comparison
run_experiment \
    "dataset_compare" \
    "globimap_test_dataset_compare" \
    "Real HDF5 datasets (GDELT 1.9M events, COVID-19 1.8M cases with 10K queries)"

# Calculate total runtime
TOTAL_END=$(date +%s)
TOTAL_DURATION=$((TOTAL_END - TOTAL_START))
TOTAL_MINUTES=$((TOTAL_DURATION / 60))
TOTAL_SECONDS=$((TOTAL_DURATION % 60))

# Print summary
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Summary${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

echo -e "Total runtime: ${GREEN}${TOTAL_MINUTES}m ${TOTAL_SECONDS}s${NC}"
echo ""

echo -e "${GREEN}Passed:${NC} ${#PASSED[@]}"
for exp in "${PASSED[@]}"; do
    echo -e "  ${GREEN}✓${NC} $exp"
done
echo ""

if [ ${#FAILED[@]} -gt 0 ]; then
    echo -e "${RED}Failed:${NC} ${#FAILED[@]}"
    for exp in "${FAILED[@]}"; do
        echo -e "  ${RED}✗${NC} $exp"
    done
    echo ""
fi

echo -e "Results saved to: ${GREEN}$RESULTS_DIR/${NC}"
echo ""

# Move JSON results to results directory
echo -e "${BLUE}Collecting JSON results...${NC}"
LOCAL_RESULTS="./results"
if [ -d "$LOCAL_RESULTS" ]; then
    # Find all compare_*.json files created in the last 5 minutes
    find "$LOCAL_RESULTS" -name "compare_*.json" -mmin -5 -exec cp {} "$RESULTS_DIR/" \; 2>/dev/null
    JSON_COUNT=$(find "$RESULTS_DIR" -name "compare_*.json" 2>/dev/null | wc -l)
    if [ $JSON_COUNT -gt 0 ]; then
        echo -e "${GREEN}Collected $JSON_COUNT JSON result files${NC}"
    else
        echo -e "${YELLOW}No JSON results found in $LOCAL_RESULTS${NC}"
    fi
else
    echo -e "${YELLOW}Note: ./results directory will be created by experiments${NC}"
fi
echo ""

# Create summary file
SUMMARY_FILE="$RESULTS_DIR/summary.txt"
{
    echo "Counting Bloom Filter Experiments - Summary"
    echo "==========================================="
    echo ""
    echo "Timestamp: $TIMESTAMP"
    echo "Total runtime: ${TOTAL_MINUTES}m ${TOTAL_SECONDS}s"
    echo ""
    echo "Passed: ${#PASSED[@]}"
    for exp in "${PASSED[@]}"; do
        echo "  ✓ $exp"
    done
    echo ""
    if [ ${#FAILED[@]} -gt 0 ]; then
        echo "Failed: ${#FAILED[@]}"
        for exp in "${FAILED[@]}"; do
            echo "  ✗ $exp"
        done
        echo ""
    fi
} > "$SUMMARY_FILE"

echo -e "${GREEN}Summary written to: $SUMMARY_FILE${NC}"
echo ""

# Exit with error if any experiments failed
if [ ${#FAILED[@]} -gt 0 ]; then
    exit 1
fi

exit 0
