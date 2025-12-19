# Less Hashing, Same Performance: Building a Better Bloom Filter

**Authors:** Adam Kirsch, Michael Mitzenmacher
**Venue/Year:** Random Structures and Algorithms, Volume 33, Pages 187-218, 2008
**DOI:** 10.1002/rsa.20208
**PDF:** rsa2008.pdf

## Abstract/Overview

This seminal paper demonstrates that Bloom filters can be implemented using only **two hash functions** instead of k hash functions without any loss in asymptotic false positive probability. The technique uses double hashing: two base hash functions h₁(x) and h₂(x) simulate k hash functions via gᵢ(x) = h₁(x) + i·h₂(x) mod m. This reduces computational overhead and randomness requirements in practice while maintaining the same theoretical guarantees as standard Bloom filters.

The paper provides rigorous theoretical analysis proving that this optimization achieves the same asymptotic false positive probability (1 - e^(-k/c))^k as standard Bloom filters, where c = m/n is the bits-per-element ratio. The analysis introduces a novel convergence framework based on Poisson approximation that generalizes to various Bloom filter variants.

## Key Contributions

- **Two-hash optimization**: Proves that only 2 hash functions are necessary for Bloom filters without increasing false positive probability asymptotically
- **General analytical framework**: Develops a balls-and-bins formulation with Poisson convergence that unifies the analysis of standard and modified Bloom filters
- **Scheme variations**: Analyzes three practical schemes:
  - Partition scheme (partitioned bit arrays, each subarray uses gᵢ(x) = h₁(x) + i·h₂(x) mod m')
  - Double hashing scheme (shared array, gᵢ(x) = h₁(x) + i·h₂(x) mod m)
  - Enhanced double hashing (adds offset function: gᵢ(x) = h₁(x) + i·h₂(x) + f(i) mod m)
- **Rate of convergence**: Provides concrete bounds on how quickly asymptotic results apply for practical parameter settings
- **False positive rate analysis**: Distinguishes between false positive probability (single query) and false positive rate (many queries), proving concentration results
- **Count-Min Sketch extension**: Shows the technique applies beyond Bloom filters, reducing Count-Min sketches from d hash functions to 2⌈(ln 1/δ)/(ln 1/ε)⌉ hash functions
- **Empirical validation**: Experiments confirm the theoretical results hold for realistic parameter values (n = 100 to 50,000, c = 4 to 16)

## Algorithm/Data Structure Details

### Standard Bloom Filter (Baseline)

- **Structure**: Array of m bits, initially all 0
- **Parameters**:
  - n elements to insert
  - m bits total (typically m = cn where c is bits per element)
  - k independent hash functions h₁, ..., hₖ with range [m]
- **Insert**: For element x ∈ S, set bits hᵢ(x) to 1 for all i ∈ [k]
- **Query**: Element y present if all hᵢ(y) are 1
- **False positive probability**: f = (1 - e^(-kn/m))^k, minimized at k = ln(2) · (m/n) giving f ≈ (0.6185)^(m/n)

### Double Hashing Scheme (Main Innovation)

**Core Idea**: Use linear combination of two hash functions
- **Hash generation**: gᵢ(x) = h₁(x) + i·h₂(x) mod m for i ∈ {0, 1, ..., k-1}
- **Base functions**: h₁, h₂ are independent uniform random hash functions on [m]
- **Computation savings**: O(k) hash evaluations reduced to O(1) (just 2 hash computations)
- **Space for randomness**: Reduced from k seeds to 2 seeds

**Theoretical Guarantees** (Theorem 5.2):
- Asymptotic FPP: lim_{n→∞} Pr(F) = (1 - e^(-k/c))^k (identical to standard)
- Works for any table size m (not just primes)
- Works with arbitrary offset function f(i) (enhanced double hashing)

### Partition Scheme

**Structure**: m bits divided into k disjoint subarrays of m' = m/k bits each
- **Hash for subarray i**: gᵢ(x) = h₁(x) + i·h₂(x) mod m'
- **Requirement**: m' should be prime for best performance
- **Advantage**: Natural parallelization (k independent subarrays)
- **Disadvantage**: Slightly worse practical performance for large c and small n

### General Framework (Theorem 4.1)

The paper introduces a unified framework for analyzing schemes where:

1. **Hash locations**: Each element u gets multiset H(u) of hash locations
2. **Collisions**: For x ∈ S and z ∉ S, define C(x) = H(x) ∩ H(z)
3. **Key conditions**:
   - H(u) has size k for all u
   - Pr(|C(x)| = 0) ≈ 1 - λ/n
   - Pr(|C(x)| = 1) ≈ λ/n
   - Pr(|C(x)| > 1) = o(1/n)
   - When |C(x)| = 1, the collision location is nearly uniform over H(z)

4. **Poisson convergence**: Under these conditions, the number of elements in S colliding with each hash location of z converges to k independent Poisson(λ/k) random variables

5. **False positive probability**: lim_{n→∞} Pr(F) = (1 - e^(-λ/k))^k

### Count-Min Sketch Modification (Section 9)

**Original Count-Min Sketch**: d × w array with d pairwise independent hash functions
- Error bound: Pr(âᵢ ≤ aᵢ + ε‖a‖₁) ≥ 1 - δ
- Parameters: w = ⌈e/ε⌉, d = ⌈ln(1/δ)⌉

**Modified Count-Min Sketch**: Uses gⱼ(x) = h₁(x) + j·h₂(x) mod w
- **Hash functions**: Only 2 pairwise independent functions (for δ = ε case)
- **General case**: 2⌈(ln 1/δ)/(ln 1/ε)⌉ hash functions (vs. d in original)
- **Requirement**: w must be prime
- **Error bound** (Theorem 9.1): For w ≥ 2e/ε, Pr(âᵢ > aᵢ + ε‖a‖₁) ≤ 2/w² + (2/w)^d

## Key Findings/Results

### Theoretical Results

1. **Asymptotic equivalence**: All analyzed schemes (partition, double hashing, enhanced double hashing) achieve the same asymptotic false positive probability as standard Bloom filters: (1 - e^(-k/c))^k

2. **Rate of convergence** (Theorem 6.1): For schemes satisfying framework conditions,
   - |Pr(F) - (1 - e^(-λ/k))^k| = O(nγ(n) + 1/n)
   - For partition scheme: γ(n) = 1/n², so error is O(1/n)
   - For double hashing: γ(n) = 1/n², so error is O(1/n)

3. **False positive rate concentration** (Theorem 7.1):
   - Define R = Pr(F | H(x₁), ..., H(xₙ)) (false positive rate given hash locations)
   - For any ε = ω(|Pr(F) - p|), Pr(|R - p| > ε) ≤ 2 exp(-2n(ε - |Pr(F) - p|)²/(λ² + ξ))
   - R converges to p = (1 - e^(-λ/k))^k in probability
   - Implies false positive probability acts as a false positive rate in practice

4. **Multiple queries convergence** (Theorem 7.2): For many distinct queries:
   - Strong law: Fraction of false positives converges to R almost surely
   - Weak law: Fraction converges to p in probability
   - Central limit theorem: Distribution approaches normal with mean p
   - Hoeffding bound: Exponential tail bounds for finite query sets

### Experimental Results

**Setup**: 10,000 trials for each configuration, testing n ∈ {100, 500, 1000, 5000, 10000, 50000} and c ∈ {4, 8, 12, 16}

**Key Findings**:

1. **Small c (4, 8)**: All schemes essentially indistinguishable from each other and from theoretical FPP
   - Example: n = 5000, c = 8, k = 6 → theoretical p ≈ 0.021577
   - All schemes empirically match this within statistical noise

2. **Larger c (12, 16)**:
   - Partition scheme shows small degradation for small n (due to higher probability of multiple collisions)
   - Double hashing and enhanced variants maintain performance
   - Difference typically < 10% even in worst cases

3. **Enhanced schemes**: For very large c, schemes with f(i) = i² or f(i) = i³ slightly outperform basic double hashing (likely due to better constants in convergence rate)

4. **Distribution of false positives**: Empirical distribution of query results matches normal approximation predicted by central limit theorem

5. **Practical recommendation**: Double hashing scheme is safe for all practical parameter ranges; partition scheme best when parallelization is valuable

## Relevance to Project

### Direct Applications to CountingGloBiMap

This paper is **foundational** for the hashing strategy already implemented in this project:

1. **Current implementation verification**:
   - The codebase uses `hash[i] = (h1 + (i+1) * h2) & mask` in `counting_globimap.hpp`
   - This is **exactly** the double hashing scheme analyzed in this paper
   - **Theoretical guarantee**: This approach is proven to achieve optimal asymptotic false positive probability

2. **Performance justification**:
   - Computing k hash functions reduced from O(k) to O(1) time
   - Critical for spatial applications where k = 8 (typical in project configs)
   - MurmurHash3 (used in `murmur.hpp`) provides the two base hashes h₁, h₂

3. **Multi-layer hierarchy**:
   - CountingGloBiMap uses multiple layers with different bit depths (1, 8, 16, 32, 64)
   - Each layer essentially acts as a separate counting Bloom filter
   - Paper's framework (Theorem 4.1) extends to prove correctness of this approach

### Insights for Implementation Improvements

1. **Enhanced hashing for large filters**:
   - For very large layers (2^20+ counters), could experiment with f(i) = i² offset
   - Paper suggests this improves constants for large c (bits per element)
   - Minimal implementation cost: just add `+ i*i` to hash computation

2. **Partition scheme for parallelization**:
   - Current implementation uses OpenMP for parallelization
   - Could partition layers into k subarrays for better cache locality
   - Trade-off: Slightly higher FPP vs. better parallel scaling

3. **Prime-sized tables**:
   - Paper shows prime table sizes perform slightly better (eliminate certain collision patterns)
   - Current implementation uses power-of-2 sizes (2^logsize) for fast masking
   - Trade-off: Masking speed vs. slightly better distribution

4. **Error bounds**:
   - Theorem 6.1 provides O(1/n) convergence rate
   - For n = 1.9M (GDELT dataset), asymptotic guarantees are very accurate
   - Can use concentration bounds (Theorem 7.1) to estimate confidence intervals

### Relevance to Other Implementations in Project

1. **Variable-Increment CBF** (`variable_increment_bf.hpp`):
   - Also uses double hashing (same optimization applies)
   - This paper's analysis explains why VI-CBF has ~4x overcounting (variable increments break Poisson assumptions)

2. **Spectral Bloom Filter** (`spectral_bloom_filter.hpp`):
   - Can apply same double hashing optimization
   - Minimal Increment (MI) variant aligns with paper's framework (conservative updates preserve Poisson structure)

3. **d-Left Counting BF** (`dleft_counting_bf.hpp`):
   - Uses d independent hash tables (similar to partition scheme)
   - Could reduce to 2 hash functions per table

4. **Count-Min Sketch** (`count_min_sketch.hpp`):
   - **Section 9 directly applies!**
   - Can implement "modified Count-Min Sketch" from this paper
   - Reduce from d = ⌈ln(1/δ)⌉ hash functions to 2 hash functions

5. **Cache-Optimal Bloom Filters**:
   - Can apply double hashing to BlockedBloomFilter
   - RegisterBlockedBF already uses minimal hashing (single 64-bit mask)
   - SimdBloomFilter could reduce gather overhead with double hashing

### Key Takeaways

- **Current implementation is theoretically sound**: The double hashing technique used in this codebase is proven optimal
- **Hash function matters**: MurmurHash3 must produce high-quality uniform distributions for theoretical guarantees to hold
- **Asymptotic results apply**: With n > 10^6 elements (typical in GDELT/COVID datasets), FPP predictions are very accurate
- **Framework is extensible**: Poisson convergence framework can potentially analyze multi-layer cascade behavior
- **Prime vs. power-of-2**: Current implementation uses 2^logsize for speed; paper suggests primes give slightly better distribution
