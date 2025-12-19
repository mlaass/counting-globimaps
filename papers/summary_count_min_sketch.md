# An Improved Data Stream Summary: The Count-Min Sketch and Its Applications

**Authors:** Graham Cormode (DIMACS, Rutgers University) and S. Muthukrishnan (Rutgers University and AT&T Research)

**Venue/Year:** Journal of Algorithms, Volume 55, 2005 (Received June 2003, Available online February 2004)

**PDF:** 4.Count_min_sketchl.pdf

## Abstract/Overview

This paper introduces the Count-Min (CM) Sketch, a sublinear space data structure for summarizing data streams. The CM sketch addresses fundamental queries in data stream summarization including point queries, range queries, and inner product queries. The data structure significantly improves upon previous sketch-based methods by reducing space and time complexity from O(1/ε²) to O(1/ε), while using only pairwise-independent hash functions instead of more complex hash families.

## Key Contributions

- **Improved space complexity**: Reduces from O(1/ε²) to O(1/ε) for most streaming problems
- **Fast updates**: O(log(1/δ)) update time, significantly sublinear in sketch size
- **Simpler hash functions**: Uses only pairwise-independent hash functions instead of 4-wise or higher
- **Multiple query types**: Single sketch supports point, range, and inner product queries
- **Explicit small constants**: All constants made explicit (e.g., width w = e/ε, depth d = ln(1/δ))
- **One-sided error guarantees**: Never underestimates counts (critical for many applications)
- **Practical applications**: Improved bounds for finding quantiles, heavy hitters, join sizes, wavelets, histograms

## Algorithm/Data Structure Details

### Core Data Structure
- **2D array**: `count[1..d, 1..w]` with width w = ⌈e/ε⌉ and depth d = ⌈ln(1/δ)⌉
- **Hash functions**: d pairwise-independent hash functions h₁...hₐ: {1...n} → {1...w}
- **Space**: O((e/ε) ln(1/δ)) = O((1/ε) log(1/δ)) words
- **Initialization**: All counters start at zero

### Update Procedure
When update (iₜ, cₜ) arrives (add cₜ to item iₜ):
```
For j = 1 to d:
    count[j, hⱼ(iₜ)] ← count[j, hⱼ(iₜ)] + cₜ
```
- **Time complexity**: O(log(1/δ)) per update

### Query Procedures

**Point Query Q(i)** - Estimate value aᵢ:
- Non-negative case: âᵢ = minⱼ count[j, hⱼ(i)]
- General case (with negatives): âᵢ = medianⱼ count[j, hⱼ(i)]
- **Guarantee (non-negative)**: aᵢ ≤ âᵢ ≤ aᵢ + ε‖a‖₁ with probability ≥ 1 - δ
- **Time**: O(log(1/δ))

**Inner Product Query Q(a,b)** - Estimate a·b:
- Compute (a⊙b)ⱼ = Σₖ countₐ[j,k] × countᵦ[j,k] for each row j
- Return minimum: a⊙b = minⱼ(a⊙b)ⱼ
- **Guarantee**: a·b ≤ a⊙b ≤ a·b + ε‖a‖₁‖b‖₁ with probability ≥ 1 - δ
- **Time**: O((1/ε) log(1/δ))

**Range Query Q(l,r)** - Estimate Σᵢ₌ₗʳ aᵢ:
- Uses dyadic ranges decomposition with log₂ n separate CM sketches
- Decomposes [l,r] into ≤ 2log₂ n dyadic ranges
- **Guarantee**: â[l,r] ≤ a[l,r] + 2ε log n ‖a‖₁ with probability ≥ 1 - δ
- **Space**: O((log n/ε) log(1/δ))
- **Time**: O(log n log(1/δ)) per update or query

### Key Technical Insights

1. **Markov inequality only**: Unlike previous work using Chebyshev + Chernoff bounds, CM sketch uses only Markov inequality for simpler analysis
2. **L₁ norm bounds**: Error bounds in terms of L₁ norm rather than L₂ (tradeoff for simplicity)
3. **Minimum vs median**: Taking minimum (not median) exploits non-negative updates for one-sided error
4. **No norm estimation**: CM sketch doesn't estimate Lₚ norms, but accurately answers queries needed for applications

## Key Findings/Results

### Theoretical Bounds

| Problem | Previous Best | CM Sketch | Improvement |
|---------|---------------|-----------|-------------|
| Point queries | O((1/ε²) log(1/δ)) | O((1/ε) log(1/δ)) | ε factor improvement |
| Range queries | O((log² n/ε²) log(log n/δ)) | O((log² n/ε) log(log n/δ)) | ε factor improvement |
| Inner products | O((1/ε²) log(1/δ)) | O((1/ε) log(1/δ)) | ε factor improvement |
| Heavy hitters (cash register) | O((1/ε²) log(a₁/δ)) | O((1/ε) log(a₁/δ)) | ε factor improvement |
| Quantiles (turnstile) | O((1/ε²) log² n log(log n/(εδ))) | O((1/ε) log² n log(log n/(φδ))) | ε factor improvement |

### Applications Demonstrated

1. **φ-quantiles**: Find approximate quantiles in turnstile model (insertions + deletions)
2. **Heavy hitters**: Find items with frequency ≥ φ‖a‖₁ in both cash register and turnstile models
3. **Join size estimation**: Approximate database join sizes with guarantees
4. **Hierarchical heavy hitters**: Improved bounds from O(h/ε²) to O(h/ε)

### Comparison with Other Sketches

The paper provides a unified framework showing all major sketch types as variants:
- **Tug-of-war** [Alon et al.]: w=1, d=O(1/ε²), uses ±1 with 4-wise independence
- **Count sketches** [Charikar et al.]: w=O(1/ε²), d=O(log(1/δ)), uses ±1 with 2-wise independence
- **Random subset sums** [Gilbert et al.]: w=2, d=24/ε², uses {0,1}
- **Count-Min sketch**: w=e/ε, d=ln(1/δ), uses constant 1 with 2-wise independence

### Constants
- Explicit small constants: width w = ⌈e/ε⌉ ≈ 2.718/ε
- Total space: (2 + e/ε) ln(1/δ) words (minimizing objective)
- Previous work often hid constants >256 in big-O notation

## Relevance to Project

### Direct Applications to CountingGloBiMap

1. **Theoretical foundation**: CM sketch is the closest theoretical match to counting bloom filters for frequency estimation
   - Both use hash-based counting with minimum estimators
   - Both provide one-sided error guarantees (never underestimate)
   - Both use simple pairwise-independent hashing

2. **Error bound insights**: CM sketch's ε‖a‖₁ error bound directly relates to counting filter accuracy
   - Error proportional to total count (not distinct items)
   - Explains why minimal_increment helps (reduces ‖a‖₁ contribution from collisions)
   - Validates min-count estimator approach

3. **Multi-layer motivation**: While CM sketch uses single-bit-depth counters, the paper's analysis validates:
   - Using minimum across hash functions for queries
   - One-sided guarantees (critical for cardinality estimation)
   - Space/accuracy tradeoffs via ε parameter

4. **Implementation comparison**: Project's Count-Min Sketch implementation (`include/count_min_sketch.hpp`) follows this paper:
   - Uses width w = ⌈e/ε⌉, depth d = ⌈ln(1/δ)⌉
   - Implements conservative update (not in original paper, but natural extension)
   - Provides epsilon_actual() and delta_actual() for exact guarantees

5. **Benchmark relevance**: Paper's focus on practical constants matters for dataset comparison experiments
   - GDELT/COVID-19 benchmarks need tight space bounds
   - Update speed critical for 1.9M event datasets
   - Pairwise-independent hashing sufficient (no need for complex hash families)

### Key Differences from Bloom Filters

- **CM sketch**: Designed for L₁ norm queries over streams, general updates (±)
- **Counting bloom filters**: Designed for point frequency/membership, typically non-negative
- **CountingGloBiMap**: Hybrid approach with multi-layer cascade for varying magnitudes

### Practical Takeaways

1. **Hash function simplicity**: Pairwise independence is sufficient (validates MurmurHash approach)
2. **One-sided errors**: Never underestimating is achievable and valuable
3. **Markov inequality**: Simpler analysis than variance-based methods, better constants
4. **Space optimization**: b = e minimizes wd (validates 2.718/ε formula)
5. **L₁ vs L₂ tradeoffs**: Error in terms of total mass (not second moment) acceptable for spatial data
