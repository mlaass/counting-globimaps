#ifndef GLOBIMAP_BF_INTERFACE_HPP
#define GLOBIMAP_BF_INTERFACE_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace globimap {

/**
 * @brief Base interface for all counting bloom filter implementations
 *
 * This interface defines the common API that all counting bloom filter
 * implementations in this library should follow. Individual implementations
 * may add specialized methods beyond this interface.
 *
 * All methods use std::vector<uint64_t> for points to support multi-dimensional
 * data (e.g., spatial coordinates, multi-attribute keys).
 *
 * @tparam ConfigType The configuration struct type for the specific implementation
 */
template<typename ConfigType>
class CountingBloomFilterInterface {
public:
    /**
     * @brief Construct a counting bloom filter with the given configuration
     * @param conf Configuration parameters specific to the implementation
     */
    explicit CountingBloomFilterInterface(const ConfigType &conf) {}

    /**
     * @brief Insert a single element into the filter
     *
     * This is the core insertion method - unified across all implementations.
     * Updates internal counters according to the specific filter strategy.
     *
     * @param point The element to insert (multi-dimensional key)
     */
    virtual void put(const std::vector<uint64_t> &point) = 0;

    /**
     * @brief Batch insertion for performance
     *
     * Inserts multiple elements. Implementations may parallelize this operation
     * using OpenMP when MAKE_PARALLEL is defined.
     *
     * @param points Vector of elements to insert
     */
    virtual void put_all(const std::vector<std::vector<uint64_t>> &points) = 0;

    /**
     * @brief Membership test - returns true if element likely present
     *
     * Checks whether an element was previously inserted. May return false positives
     * but never false negatives (for bloom filters). For sketch structures, may
     * always return true.
     *
     * @param point The element to query
     * @return true if element is likely in the set, false if definitely not
     */
    virtual bool get_bool(const std::vector<uint64_t> &point) = 0;

    /**
     * @brief Cardinality/frequency estimation - returns estimated count
     *
     * Semantics vary by implementation:
     *   - Standard/VI-CBF: min count across k hash positions
     *   - Spectral BF: frequency estimate (more accurate for high-frequency items)
     *   - d-Left: exact count from fingerprint match (or 0 if not found)
     *   - Count-Min: frequency estimate with theoretical bounds
     *
     * @param point The element to query
     * @return Estimated count/frequency of the element
     */
    virtual uint64_t get_min(const std::vector<uint64_t> &point) = 0;

    /**
     * @brief Get summary statistics about the filter
     *
     * Returns a human-readable string with implementation-specific statistics
     * such as configuration parameters, memory usage, load factor, etc.
     *
     * @return String containing summary information
     */
    virtual std::string summary() = 0;

    /**
     * @brief Get memory usage in bytes
     *
     * Returns the total memory consumed by this filter instance, including
     * all internal data structures.
     *
     * @return Memory usage in bytes
     */
    virtual uint64_t memory_usage() const = 0;

    /**
     * @brief Virtual destructor for proper cleanup
     */
    virtual ~CountingBloomFilterInterface() = default;
};

// Optional methods not part of the base interface:
// (Not all implementations support these)
//
// - void remove(const std::vector<uint64_t> &point);
//   Deletion support (Spectral BF RM variant, d-Left CBF only)
//
// - uint64_t get_mean(const std::vector<uint64_t> &point);
//   Mean estimator (Spectral BF only)
//
// - double load_factor() const;
//   Utilization metric (d-Left CBF only)
//
// - std::string error_analysis();
//   Detailed error statistics when collect_input=true (implementation-specific)
//
// - std::vector<std::pair<uint64_t, uint64_t>> get_heavy_hitters(double threshold);
//   Heavy hitter detection (Count-Min Sketch only)

}  // namespace globimap

#endif  // GLOBIMAP_BF_INTERFACE_HPP
