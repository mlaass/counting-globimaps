#!/bin/bash
# Setup/revert machine configuration for low-noise benchmarking
# Usage: sudo ./setup_benchmark_env.sh --setup
#        sudo ./setup_benchmark_env.sh --revert

set -e

# Detect CPU vendor
detect_cpu() {
    if grep -q "GenuineIntel" /proc/cpuinfo; then
        echo "intel"
    elif grep -q "AuthenticAMD" /proc/cpuinfo; then
        echo "amd"
    else
        echo "unknown"
    fi
}

CPU_VENDOR=$(detect_cpu)

show_status() {
    echo "Current benchmark environment status:"
    echo "======================================="
    echo "CPU vendor: $CPU_VENDOR"
    echo ""

    echo "perf_event_paranoid: $(cat /proc/sys/kernel/perf_event_paranoid)"

    if [ -f /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor ]; then
        echo "CPU governor: $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor)"
    else
        echo "CPU governor: (not available)"
    fi

    if [ "$CPU_VENDOR" = "intel" ]; then
        if [ -f /sys/devices/system/cpu/intel_pstate/no_turbo ]; then
            val=$(cat /sys/devices/system/cpu/intel_pstate/no_turbo)
            [ "$val" = "1" ] && echo "Turbo boost: disabled" || echo "Turbo boost: enabled"
        else
            echo "Turbo boost: (intel_pstate not available)"
        fi
    elif [ "$CPU_VENDOR" = "amd" ]; then
        if [ -f /sys/devices/system/cpu/cpufreq/boost ]; then
            val=$(cat /sys/devices/system/cpu/cpufreq/boost)
            [ "$val" = "0" ] && echo "Turbo boost: disabled" || echo "Turbo boost: enabled"
        else
            echo "Turbo boost: (not available)"
        fi
    fi

    echo "ASLR: $(cat /proc/sys/kernel/randomize_va_space) (0=off, 2=full)"
    echo ""
}

do_setup() {
    echo "Setting up benchmark environment..."
    echo ""

    # 1. Enable perf counters
    echo "1. Enabling perf counters..."
    sysctl -w kernel.perf_event_paranoid=1

    # 2. Set CPU governor to performance
    echo "2. Setting CPU governor to performance..."
    if [ -f /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor ]; then
        for gov in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
            echo performance > "$gov" 2>/dev/null || true
        done
        echo "   Done"
    else
        echo "   Skipped (cpufreq not available)"
    fi

    # 3. Disable turbo boost
    echo "3. Disabling turbo boost..."
    if [ "$CPU_VENDOR" = "intel" ]; then
        if [ -f /sys/devices/system/cpu/intel_pstate/no_turbo ]; then
            echo 1 > /sys/devices/system/cpu/intel_pstate/no_turbo
            echo "   Done (Intel)"
        else
            echo "   Skipped (intel_pstate not available)"
        fi
    elif [ "$CPU_VENDOR" = "amd" ]; then
        if [ -f /sys/devices/system/cpu/cpufreq/boost ]; then
            echo 0 > /sys/devices/system/cpu/cpufreq/boost
            echo "   Done (AMD)"
        else
            echo "   Skipped (boost not available)"
        fi
    else
        echo "   Skipped (unknown CPU vendor)"
    fi

    # 4. Disable ASLR
    echo "4. Disabling ASLR..."
    sysctl -w kernel.randomize_va_space=0

    echo ""
    echo "Setup complete. Run benchmarks with:"
    echo "  ./run_scalability_benchmark.sh --skip-large"
    echo ""
    echo "When done, run: sudo $0 --revert"
    echo ""
    show_status
}

do_revert() {
    echo "Reverting benchmark environment..."
    echo ""

    # 1. Reset perf counters to default
    echo "1. Resetting perf_event_paranoid..."
    sysctl -w kernel.perf_event_paranoid=4

    # 2. Set CPU governor back to schedutil (or powersave)
    echo "2. Setting CPU governor to schedutil..."
    if [ -f /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor ]; then
        for gov in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
            # Try schedutil first, fall back to powersave
            echo schedutil > "$gov" 2>/dev/null || echo powersave > "$gov" 2>/dev/null || true
        done
        echo "   Done"
    else
        echo "   Skipped (cpufreq not available)"
    fi

    # 3. Re-enable turbo boost
    echo "3. Re-enabling turbo boost..."
    if [ "$CPU_VENDOR" = "intel" ]; then
        if [ -f /sys/devices/system/cpu/intel_pstate/no_turbo ]; then
            echo 0 > /sys/devices/system/cpu/intel_pstate/no_turbo
            echo "   Done (Intel)"
        else
            echo "   Skipped (intel_pstate not available)"
        fi
    elif [ "$CPU_VENDOR" = "amd" ]; then
        if [ -f /sys/devices/system/cpu/cpufreq/boost ]; then
            echo 1 > /sys/devices/system/cpu/cpufreq/boost
            echo "   Done (AMD)"
        else
            echo "   Skipped (boost not available)"
        fi
    else
        echo "   Skipped (unknown CPU vendor)"
    fi

    # 4. Re-enable ASLR
    echo "4. Re-enabling ASLR..."
    sysctl -w kernel.randomize_va_space=2

    echo ""
    echo "Revert complete."
    echo ""
    show_status
}

require_root() {
    if [ "$EUID" -ne 0 ]; then
        echo "This command requires root: sudo $0 $*"
        exit 1
    fi
}

case "${1:-}" in
    --setup)
        require_root --setup
        do_setup
        ;;
    --revert)
        require_root --revert
        do_revert
        ;;
    --status)
        show_status
        ;;
    *)
        echo "Usage: sudo $0 [--setup|--revert|--status]"
        echo ""
        echo "Options:"
        echo "  --setup   Configure machine for low-noise benchmarking"
        echo "  --revert  Restore default settings"
        echo "  --status  Show current configuration"
        echo ""
        echo "Detected CPU: $CPU_VENDOR"
        exit 1
        ;;
esac
