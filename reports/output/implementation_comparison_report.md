# Implementation Comparison Report

**Status**: No results available yet

## Description

This report provides a quick baseline comparison of all counting bloom
filter implementations across three memory budget scenarios:

**Memory Budgets:**
- **Tiny**: ~2 KB - Minimum viable configuration
- **Medium**: ~128 KB - Typical production use
- **Large**: ~1 MB - High-accuracy configuration

**Implementations Compared:**
1. Spectral Bloom Filter (MI variant)
2. d-Left Counting Bloom Filter
3. Count-Min Sketch
4. CountingGloBiMap (MI variant)
5. Variable-Increment Bloom Filter

**Metrics:**
- Memory usage (actual vs target)
- Insert throughput
- Query latency
- Estimation accuracy

## How to Generate Results

```bash
# From project root
./run_all_experiments.sh

# Or run specific experiment from build/
./build/compare_all_implementations
```

Results will be saved to `results/implementation_comparison/`.

---

*Report generated: 2025-12-12 01:31:53*
