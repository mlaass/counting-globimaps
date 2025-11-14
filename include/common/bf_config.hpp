#ifndef GLOBIMAP_BF_CONFIG_HPP
#define GLOBIMAP_BF_CONFIG_HPP

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace globimap {

// Common configuration constants
#define EULER_E 2.71828182845904523536  // For Count-Min Sketch calculations

/**
 * @brief Configuration validation and utility functions
 *
 * This namespace provides common helper functions for validating and computing
 * configuration parameters used across different bloom filter implementations.
 */
namespace config_utils {

/**
 * @brief Check if a value is a power of 2
 * @param x Value to check
 * @return true if x is a power of 2, false otherwise
 */
inline bool is_power_of_two(uint x) {
    return x > 0 && (x & (x - 1)) == 0;
}

/**
 * @brief Calculate optimal number of hash functions for standard bloom filter
 *
 * Formula: k = ceil(ln(1/p) / ln(2))
 * where p is the desired false positive rate
 *
 * @param fpr Desired false positive rate (e.g., 0.01 for 1%)
 * @return Optimal number of hash functions
 */
inline uint optimal_hash_k(double fpr) {
    if (fpr <= 0.0 || fpr >= 1.0) {
        throw std::invalid_argument("False positive rate must be in (0, 1)");
    }
    return static_cast<uint>(std::ceil(std::log(1.0 / fpr) / std::log(2.0)));
}

/**
 * @brief Calculate optimal filter size (log2) for given n and fpr
 *
 * Formula: m = ceil((n * k) / ln(2))
 * Returns logsize such that filter has 2^logsize counters
 *
 * @param n Expected number of elements
 * @param fpr Desired false positive rate
 * @return Log2 of filter size
 */
inline uint optimal_logsize(uint64_t n, double fpr) {
    if (n == 0) {
        throw std::invalid_argument("Number of elements must be positive");
    }
    uint k = optimal_hash_k(fpr);
    double m = (static_cast<double>(n) * k) / std::log(2.0);
    return static_cast<uint>(std::ceil(std::log2(m)));
}

/**
 * @brief Calculate required counter bits for maximum count
 *
 * Returns the minimum number of bits needed to represent max_count.
 * Adds 1 bit of headroom for safety.
 *
 * @param max_count Maximum expected count value
 * @return Number of bits required
 */
inline uint counter_bits_for_max(uint64_t max_count) {
    if (max_count == 0) {
        return 1;
    }
    return static_cast<uint>(std::ceil(std::log2(static_cast<double>(max_count)))) + 1;
}

/**
 * @brief Calculate required counter bits for VI-CBF
 *
 * VI-CBF uses variable increments in range [L, 2L-1], so counters grow faster.
 * Formula: bits = ceil(log2(max_count * 2L))
 *
 * @param max_count Maximum expected count value
 * @param increment_L Base increment value
 * @return Number of bits required
 */
inline uint vicbf_counter_bits(uint64_t max_count, uint increment_L) {
    if (max_count == 0) {
        return 1;
    }
    if (!is_power_of_two(increment_L)) {
        throw std::invalid_argument("increment_L must be a power of 2");
    }
    uint64_t max_counter_value = max_count * 2 * increment_L;
    return static_cast<uint>(std::ceil(std::log2(static_cast<double>(max_counter_value)))) + 1;
}

/**
 * @brief Calculate Count-Min Sketch width from epsilon
 *
 * Formula: width = ceil(e / epsilon)
 * where e = 2.71828... (Euler's number)
 *
 * @param epsilon Error parameter (e.g., 0.01 for 1% error)
 * @return Width of the Count-Min Sketch
 */
inline uint cms_width(double epsilon) {
    if (epsilon <= 0.0 || epsilon >= 1.0) {
        throw std::invalid_argument("Epsilon must be in (0, 1)");
    }
    return static_cast<uint>(std::ceil(EULER_E / epsilon));
}

/**
 * @brief Calculate Count-Min Sketch depth from delta
 *
 * Formula: depth = ceil(ln(1 / delta))
 *
 * @param delta Confidence parameter (e.g., 0.01 for 99% confidence)
 * @return Depth (number of rows) of the Count-Min Sketch
 */
inline uint cms_depth(double delta) {
    if (delta <= 0.0 || delta >= 1.0) {
        throw std::invalid_argument("Delta must be in (0, 1)");
    }
    return static_cast<uint>(std::ceil(std::log(1.0 / delta)));
}

/**
 * @brief Calculate maximum counter value for given bit depth
 *
 * @param bits Number of bits per counter
 * @return Maximum value that can be stored
 */
inline uint64_t max_counter_value(uint bits) {
    if (bits == 0 || bits > 64) {
        throw std::invalid_argument("Bits must be in range [1, 64]");
    }
    if (bits == 64) {
        return UINT64_MAX;
    }
    return (1ULL << bits) - 1;
}

/**
 * @brief Validate configuration parameter is positive
 *
 * @param value Value to check
 * @param name Parameter name for error message
 * @throws std::invalid_argument if value is not positive
 */
inline void validate_positive(uint value, const std::string &name) {
    if (value == 0) {
        throw std::invalid_argument(name + " must be positive");
    }
}

/**
 * @brief Validate configuration parameter is in valid range
 *
 * @param value Value to check
 * @param min Minimum allowed value (inclusive)
 * @param max Maximum allowed value (inclusive)
 * @param name Parameter name for error message
 * @throws std::invalid_argument if value is out of range
 */
inline void validate_range(uint value, uint min, uint max, const std::string &name) {
    if (value < min || value > max) {
        throw std::invalid_argument(name + " must be in range [" +
                                   std::to_string(min) + ", " +
                                   std::to_string(max) + "]");
    }
}

/**
 * @brief Validate that a value is a power of 2
 *
 * @param value Value to check
 * @param name Parameter name for error message
 * @throws std::invalid_argument if value is not a power of 2
 */
inline void validate_power_of_two(uint value, const std::string &name) {
    if (!is_power_of_two(value)) {
        throw std::invalid_argument(name + " must be a power of 2");
    }
}

/**
 * @brief Format memory size in human-readable units
 *
 * Converts bytes to KB, MB, GB as appropriate.
 *
 * @param bytes Memory size in bytes
 * @return Formatted string (e.g., "16.5 MB")
 */
inline std::string format_memory(uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_idx = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024.0 && unit_idx < 4) {
        size /= 1024.0;
        unit_idx++;
    }

    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%.2f %s", size, units[unit_idx]);
    return std::string(buffer);
}

}  // namespace config_utils

}  // namespace globimap

#endif  // GLOBIMAP_BF_CONFIG_HPP
