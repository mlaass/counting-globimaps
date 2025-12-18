#!/bin/bash
#
# Run Bloom Filter Benchmark with optimal settings for accurate hardware counter measurements
#
# Usage:
#   ./run_bloom_benchmark.sh [script-options] [-- benchmark-options]
#   ./run_bloom_benchmark.sh --no-tune -- --epochs 51 --scenario large
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BENCHMARK="$SCRIPT_DIR/build/bloom_filter_benchmark"

# Check if benchmark exists
if [ ! -f "$BENCHMARK" ]; then
    echo "Error: Benchmark not found at $BENCHMARK"
    echo "Please build first: cd build && make bloom_filter_benchmark"
    exit 1
fi

# Function to check if we have perf access
check_perf_access() {
    local paranoid=$(cat /proc/sys/kernel/perf_event_paranoid 2>/dev/null || echo "4")
    if [ "$paranoid" -gt 1 ]; then
        return 1
    fi
    return 0
}

# Function to get current CPU governor
get_governor() {
    cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || echo "unknown"
}

# Parse script arguments (before --)
NO_TUNE=false
BENCHMARK_ARGS=()
PARSING_SCRIPT_ARGS=true

for arg in "$@"; do
    if [ "$arg" = "--" ]; then
        PARSING_SCRIPT_ARGS=false
        continue
    fi

    if [ "$PARSING_SCRIPT_ARGS" = true ]; then
        case $arg in
            --no-tune)
                NO_TUNE=true
                ;;
            --help|-h)
                echo "Usage: $0 [script-options] [-- benchmark-options]"
                echo ""
                echo "Script options:"
                echo "  --no-tune    Skip system tuning (run with current settings)"
                echo "  --help       Show this help"
                echo ""
                echo "Benchmark options (after --):"
                echo "  --epochs N        Number of epochs per benchmark (default: 11)"
                echo "  --min-iters N     Minimum iterations per epoch (default: 1000)"
                echo "  --warmup N        Warmup iterations (default: 100)"
                echo "  --epoch-time MS   Max time per epoch in ms (default: 100)"
                echo "  --scenario S      Run only scenario: small, medium, large, all (default: all)"
                echo ""
                echo "Examples:"
                echo "  $0                                    # Run with tuning, default settings"
                echo "  $0 --no-tune                          # Run without tuning"
                echo "  $0 -- --epochs 51 --min-iters 10000   # Longer, more accurate run"
                echo "  $0 -- --scenario large                # Only large scenario"
                echo ""
                echo "For accurate cycle/instruction counts, this script will:"
                echo "  1. Set perf_event_paranoid to 1 (requires sudo)"
                echo "  2. Set CPU governor to 'performance' (requires sudo)"
                echo ""
                exit 0
                ;;
            *)
                # Assume it's a benchmark arg if we haven't seen --
                BENCHMARK_ARGS+=("$arg")
                ;;
        esac
    else
        BENCHMARK_ARGS+=("$arg")
    fi
done

echo "===== Bloom Filter Benchmark Runner ====="
echo ""

# Check current settings
PARANOID=$(cat /proc/sys/kernel/perf_event_paranoid 2>/dev/null || echo "unknown")
GOVERNOR=$(get_governor)

echo "Current system settings:"
echo "  perf_event_paranoid: $PARANOID (need ≤1 for hardware counters)"
echo "  CPU governor: $GOVERNOR (recommend 'performance')"
echo ""

if [ ${#BENCHMARK_ARGS[@]} -gt 0 ]; then
    echo "Benchmark args: ${BENCHMARK_ARGS[*]}"
    echo ""
fi

if [ "$NO_TUNE" = true ]; then
    echo "Running without tuning (--no-tune specified)..."
    echo ""
    exec "$BENCHMARK" "${BENCHMARK_ARGS[@]}"
fi

# Check if we need sudo
NEED_SUDO=false
if [ "$PARANOID" != "1" ] && [ "$PARANOID" != "0" ] && [ "$PARANOID" != "-1" ]; then
    NEED_SUDO=true
fi
if [ "$GOVERNOR" != "performance" ]; then
    NEED_SUDO=true
fi

if [ "$NEED_SUDO" = true ]; then
    echo "System tuning required for accurate measurements."
    echo "This will temporarily:"
    echo "  - Set perf_event_paranoid to 1"
    echo "  - Set CPU governor to 'performance'"
    echo ""

    if [ "$EUID" -ne 0 ]; then
        echo "Re-running with sudo..."
        exec sudo "$0" "$@"
    fi

    # Save original values for restoration
    ORIG_PARANOID="$PARANOID"
    ORIG_GOVERNOR="$GOVERNOR"

    # Tune system
    echo "Tuning system for benchmarking..."

    if [ "$PARANOID" != "1" ]; then
        echo "  Setting perf_event_paranoid=1"
        echo 1 > /proc/sys/kernel/perf_event_paranoid
    fi

    if [ "$GOVERNOR" != "performance" ] && [ -f /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor ]; then
        echo "  Setting CPU governor to 'performance'"
        for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
            echo "performance" > "$cpu" 2>/dev/null || true
        done
    fi

    echo ""
    echo "Running benchmark..."
    echo ""

    # Run benchmark with args
    "$BENCHMARK" "${BENCHMARK_ARGS[@]}"
    RESULT=$?

    # Restore original settings
    echo ""
    echo "Restoring original settings..."

    if [ "$ORIG_PARANOID" != "1" ]; then
        echo "  Restoring perf_event_paranoid=$ORIG_PARANOID"
        echo "$ORIG_PARANOID" > /proc/sys/kernel/perf_event_paranoid
    fi

    if [ "$ORIG_GOVERNOR" != "performance" ] && [ -f /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor ]; then
        echo "  Restoring CPU governor to '$ORIG_GOVERNOR'"
        for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
            echo "$ORIG_GOVERNOR" > "$cpu" 2>/dev/null || true
        done
    fi

    exit $RESULT
else
    echo "System already tuned for benchmarking."
    echo ""
    exec "$BENCHMARK" "${BENCHMARK_ARGS[@]}"
fi
