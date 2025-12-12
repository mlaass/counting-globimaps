#!/usr/bin/env bash

# Report Generation Script
# Generates all experiment reports (markdown + PDF)

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Counting Bloom Filter Report Generator${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Change to project root for uv to find pyproject.toml
cd "$PROJECT_ROOT"

# Ensure output directories exist
mkdir -p "$SCRIPT_DIR/figures"
mkdir -p "$SCRIPT_DIR/output"

# Track report status
declare -a GENERATED=()
declare -a PLACEHOLDER=()
declare -a FAILED=()

# Function to run a report generator
run_report() {
    local name=$1
    local script=$2
    local description=$3

    echo -e "${YELLOW}---${NC} ${BLUE}$name${NC} ${YELLOW}---${NC}"
    echo "  $description"

    if uv run python "$SCRIPT_DIR/$script" 2>&1 | sed 's/^/  /'; then
        # Check if it was a placeholder or full report
        output_file="$SCRIPT_DIR/output/${script%.py}.md"
        if [ -f "$output_file" ] && grep -q "No results available" "$output_file" 2>/dev/null; then
            PLACEHOLDER+=("$name")
        else
            GENERATED+=("$name")
        fi
    else
        echo -e "  ${RED}ERROR${NC}"
        FAILED+=("$name")
    fi
    echo ""
}

echo "Generating reports..."
echo ""

# Generate all reports
run_report \
    "Dataset Comparison" \
    "dataset_comparison_report.py" \
    "Compare implementations on GDELT/COVID-19 datasets"

run_report \
    "Multi-Category Analysis" \
    "multicategory_report.py" \
    "Category isolation and accuracy analysis"

run_report \
    "K Sensitivity" \
    "k_sensitivity_report.py" \
    "Impact of hash function count on performance"

run_report \
    "Cosine Distribution" \
    "cosine_report.py" \
    "Synthetic cosine distribution benchmark"

run_report \
    "Implementation Comparison" \
    "implementation_comparison_report.py" \
    "Quick baseline across memory budgets"

# Print summary
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Summary${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

if [ ${#GENERATED[@]} -gt 0 ]; then
    echo -e "${GREEN}Generated (with data):${NC} ${#GENERATED[@]}"
    for r in "${GENERATED[@]}"; do
        echo -e "  ${GREEN}✓${NC} $r"
    done
    echo ""
fi

if [ ${#PLACEHOLDER[@]} -gt 0 ]; then
    echo -e "${YELLOW}Placeholder (no data):${NC} ${#PLACEHOLDER[@]}"
    for r in "${PLACEHOLDER[@]}"; do
        echo -e "  ${YELLOW}○${NC} $r"
    done
    echo ""
fi

if [ ${#FAILED[@]} -gt 0 ]; then
    echo -e "${RED}Failed:${NC} ${#FAILED[@]}"
    for r in "${FAILED[@]}"; do
        echo -e "  ${RED}✗${NC} $r"
    done
    echo ""
fi

echo -e "Reports saved to: ${GREEN}$SCRIPT_DIR/output/${NC}"
echo -e "Figures saved to: ${GREEN}$SCRIPT_DIR/figures/${NC}"
echo ""

# List generated files
echo "Generated files:"
if [ -d "$SCRIPT_DIR/output" ]; then
    ls -la "$SCRIPT_DIR/output/" 2>/dev/null | tail -n +2 || echo "  (empty)"
fi
echo ""

# Return non-zero if any failed
if [ ${#FAILED[@]} -gt 0 ]; then
    exit 1
fi

exit 0
