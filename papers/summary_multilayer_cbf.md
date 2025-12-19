# MultiLayer Compressed Counting Bloom Filters

**Authors:** Domenico Ficara, Stefano Giordano, Gregorio Procissi, Fabio Vitucci
**Venue/Year:** IEEE INFOCOM 2008
**PDF:** MultiLayer Compressed Counting Bloom Filters.pdf

## Abstract/Overview

This paper addresses the problem of counters overflow and memory efficiency in Counting Bloom Filters (CBFs). Standard CBFs use fixed-size counters (bins) to support insertions and deletions, but suffer from two key limitations: (1) potential counter overflow when using fixed bit widths, and (2) inefficient memory utilization since counter load distribution varies dramatically across bins. The paper presents a tighter upper bound for overflow probability and proposes three novel data structures that achieve up to 50% memory savings compared to standard CBFs while avoiding overflow through hierarchical multilayer architectures and Huffman coding.

## Key Contributions

- **Tighter overflow probability bound**: A new upper bound P(ϕ ≥ j) < [α(j+1)]/[j(j+1-α)] × P(ϕ = j-1) that is much tighter than the classical bound (enk/jm)^j, enabling more efficient CBF design
- **Huffman coding for CBF counters**: Proof that encoding counter value ϕ with ϕ consecutive ones and a trailing zero is optimal Huffman coding (length = ϕ + 1 bits)
- **Hierarchical multilayer structure**: First introduction of layer hierarchy to CBFs, enabling exploitation of built-in memory hierarchies in systems like Network Processors
- **Three novel data structures**: ML-HCBF (MultiLayer Hash-based CBF), HSBF (Huffman Spectral Bloom Filter), and ML-CCBF (MultiLayer Compressed CBF)
- **Practical implementation**: Designed for systems with limited fast memory such as Network Processors and programmable routers

## Algorithm/Data Structure Details

### 1. ML-HCBF (MultiLayer Hash-based CBF)

**Structure:**
- Main vector: CBFV with m₀ bins of x₀ bits each
- Overflow stack: N hash-based vectors (HBV₁, ..., HBVₙ) with mⱼ bins of xⱼ bits
- Uses k + N hash functions: k standard hash functions for CBFV, N minimal perfect hash functions (MPHFs) for HBVs

**Cascading mechanism:**
- Counter value = sum of CBFV element + all corresponding HBV elements in the cascade chain
- When a counter reaches saturation (2^xⱼ - 1), the position is hashed to find the next layer
- Maximum supported counter value: ϕₘₐₓ = Σ(2^xⱼ) - N - 1

**Properties:**
- Average size: E[S] = m₀x₀ + Σ(xⱼαⱼ) where αⱼ = 2^(log₂ P(ϕ > ϕⱼ₋₁))
- Lookup complexity: O(1) - same as standard CBF (only checks CBFV)
- Insertion/deletion complexity: O(1) average, k(1 + Σ P(ϕ > ϕⱼ)) hash operations
- Memory saving: 33-51% compared to standard CBF with same overflow probability

**Example configuration:** (2, 2, 3, 3) bits across 4 layers achieves P(overflow) = 1.34×10⁻²² with 7.55 KB vs 15.22 KB for standard CBF (50% savings)

### 2. HSBF (Huffman Spectral Bloom Filter)

**Encoding scheme:**
- Counter value ϕ encoded as ϕ consecutive ones + trailing zero (ϕ+1 bits total)
- Theorem: This is optimal Huffman coding for independent symbols (CBF bins)
- Example: value 3 → "1110", value 0 → "0", value 5 → "111110"

**Structure:**
- Divided into B blocks of D bins each
- Index tables address blocks
- Slack bits (ε) at end of each block to reduce shift operations

**Fast lookup via popcount:**
- Uses CPU popcount instruction to count set bits in a word
- Number of cleared bits = number of symbols encoded in word
- No need to decode each value individually

**Properties:**
- Average size: E[S] = m(1 + E[ϕ]) + B(ε + log₂[(m-D)(ϕₘₐₓ+1)])
- Lookup complexity: O(1), k × D(E[ϕ]+1)/(2W) operations where W = word size
- Insertion/deletion complexity: O(1), k × D(E[ϕ]+1)/W operations (includes shift)
- Since E[ϕ] ≈ ln 2 ≈ 0.693, operations are constant

### 3. ML-CCBF (MultiLayer Compressed CBF)

**Structure:**
- Stack of bitmaps L₀, ..., Lₙ where L₀ is a standard Bloom Filter
- Layer Lᵢ stores all i-th binary digits of Huffman-encoded counters
- On layer Lᵢ, the j-th bit belongs to counter whose popcount on Lᵢ₋₁ is j

**Counter lookup algorithm:**
1. Hash function h(σ) points to position in L₀
2. Compute popcount(h(σ)) on L₀ to get index for L₁
3. Repeat: at layer j, compute popcount to get index for layer j+1
4. Stop when encountering a "0" bit (end of Huffman code)
5. Resulting code gives counter value (e.g., "1110" = 3)

**Optimization with tables:**
- Each layer divided into blocks of size D
- Table per layer stores number of ones preceding each block
- Reduces popcount operation to ~D/(2W) words instead of entire layer

**Properties:**
- Size of layer i: mᵢ = m₀ × P(ϕ ≥ i)
- Total size: S = m₀ + Σϕᵢ + Σ(table sizes)
- Table size bound: TS ≤ (m₀/D)(2log₂(m₀) - 1.85) for α = ln 2
- Average size: E[S] = m₀(1 + E[ϕ]) + TS
- **Lookup complexity: O(1)** - only checks L₀ (first layer), same as standard BF
- **Insertion/deletion complexity: O(1)** - average k × E[ϕ] × (1 + D/(2W) + 2) operations
- Memory saving: 56% compared to standard CBF
- **Key advantage:** L₀ in fast local memory enables 83% reduction in lookup clock cycles

## Key Findings/Results

### Theoretical Results

1. **Tighter overflow bound**: For n=1000, k=10, m=nk/ln2, the new bound gives P(j>15) < 1.51×10⁻¹⁶ vs classical bound 1.37×10⁻¹⁵ (order of magnitude improvement)

2. **Huffman optimality**: The unary encoding (ϕ ones + zero) is provably optimal for independent symbols, resulting from a completely unbalanced Huffman tree

3. **Expected counter value**: E[ϕ] ≈ ln 2 ≈ 0.693 when CBF minimizes false positive probability (k = (m/n)ln 2)

### Performance Comparison (Intel IXP2800 Network Processor)

Test configuration: n=2000 elements, k=10 hash functions, m=28,000 bins, P(false positive)=10⁻³

| Algorithm | Size (KB) | Lookup (cycles) | Insert/Delete (cycles) | Memory Savings |
|-----------|-----------|-----------------|------------------------|----------------|
| **Standard CBF** | 14.1 | 700 | 710 | baseline |
| **ML-HCBF** | 7.55 | 700 | 1,043 | **46%** |
| **ML-CCBF** | 6.13 | **120** | 1,064 | **56%** |
| **HSBF** | 6.42 | 606 | 1,058 | **54%** |
| DCF | 14.1 | 700 | 710 | 0% |
| SBF | 12.12 | 801 | 1,217 | 14% |
| dlCBF | 5.2 | 800 | 810 | 63% |

**Key findings:**
- **ML-CCBF achieves 83% reduction in lookup time** (120 vs 700 cycles) by storing L₀ in fast local memory
- All three proposed methods achieve 46-56% memory savings vs standard CBF
- ML-CCBF approaches theoretical minimum (m × entropy)
- Trade-off: 45-50% increase in insertion/deletion complexity, but lookups are far more frequent

### Memory Hierarchy Exploitation

Optimal placement for ML-CCBF on IXP2800:
- **Local memory** (fast, 4KB): L₀ main BF + index tables
- **Scratchpad** (medium, 16KB): Remaining layers L₁, L₂, ...
- **Lookup**: Only accesses local memory (very fast)
- **Insert/delete**: Accesses both memories (acceptable since less frequent)

## Relevance to Project

This paper is **highly relevant** to the CountingGloBiMap implementation in this project. Several key connections:

### 1. Multi-Layer Cascading Architecture

**Direct parallel:** CountingGloBiMap uses the same hierarchical layer design as ML-HCBF:
- CLAUDE.md describes: "Each layer has a different bit depth (1, 8, 16, 32, or 64 bits). When a counter reaches its maximum value, increments cascade to the next layer"
- This is exactly the ML-HCBF approach where saturated counters cascade to higher-bit-depth layers

**Paper's contribution:** Provides the theoretical foundation (tighter overflow bounds) that justifies this design choice and enables optimal layer sizing

### 2. Conservative Update Strategy

**Project feature:** CountingGloBiMap supports `minimal_increment` option for conservative updates
- CLAUDE.md: "Optional `minimal_increment` for conservative updates (reduces overcounting)"

**Paper connection:** ML-HCBF's cascading mechanism inherently provides conservative behavior - counters only increment when current layer allows, preventing premature overflow

### 3. Memory Efficiency Goals

**Shared objective:** Both target compact storage for varying count magnitudes
- Paper: "up to 50% of memory saving" for network processors with limited SRAM
- Project: "compact storage for counts of varying magnitudes" in spatial data structures

**Paper's insight:** The tighter overflow bound P(ϕ ≥ j) enables smaller layer sizes while maintaining safety guarantees

### 4. Hash Function Optimization

**Paper technique:** Uses k+N hash functions with minimal perfect hashing for overflow layers
**Project technique:** "Hashing trick" - computes 2 base hashes (h1, h2) then generates k positions via hash[i] = (h1 + (i+1)×h2) & mask

**Potential improvement:** Project's approach is simpler (no MPHFs needed), but paper's theoretical bounds could optimize layer sizing

### 5. Design Differences

**CountingGloBiMap advantages:**
- More flexible bit depths (1, 8, 16, 32, 64) vs paper's power-of-2 constraints
- Simpler hash generation (no MPHFs)
- Better suited for spatial data (2D/3D coordinates)

**Paper advantages:**
- Formal overflow probability bounds for layer sizing
- Memory hierarchy exploitation (fast/slow memory placement)
- Huffman coding approach (ML-CCBF) could further compress sparse layers

### 6. Potential Enhancements

Based on paper insights, CountingGloBiMap could explore:

1. **Formal layer sizing:** Use paper's bound α(j+1)P(ϕ=j-1)/(j(j+1-α)) to compute optimal layer sizes based on expected load

2. **Huffman compression:** For very large filters, consider ML-CCBF's bitmap approach for layers with E[ϕ] << 1 (most counters zero)

3. **Cascade factor optimization:** Paper shows optimal α = ln 2 for minimizing false positives; project's `cascade_factor` parameter could use this as default

4. **Memory hierarchy:** Place lower layers (frequently accessed) in CPU cache-friendly structures, higher layers in slower memory

5. **Theoretical guarantees:** Adopt paper's corollary P(ϕ > j) < P(ϕ = j-1) to provide provable overflow bounds to users

### 7. Validation Opportunity

The paper's experimental results (Table I) provide benchmark configurations that could validate CountingGloBiMap:
- Configuration (2,2,3,3): P(overflow) = 1.34×10⁻²², 7.55 KB
- Configuration (2,2,2,2,2,2): P(overflow) = 1.1×10⁻¹⁹, 7.55 KB

Testing CountingGloBiMap with equivalent configurations would confirm whether the project's implementation achieves similar compression ratios.

---

**Summary:** This paper provides the foundational theory for multi-layer counting bloom filters with cascading overflow - exactly the architecture CountingGloBiMap implements. The tighter overflow bounds, optimal Huffman encoding, and memory hierarchy insights could directly improve the project's layer configuration, memory footprint, and theoretical guarantees.
