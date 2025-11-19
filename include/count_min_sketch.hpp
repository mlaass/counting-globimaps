#ifndef GLOBIMAP_COUNT_MIN_SKETCH_HPP
#define GLOBIMAP_COUNT_MIN_SKETCH_HPP

#include "common/bf_config.hpp"
#include "common/bf_interface.hpp"
#include "hashfn.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace globimap {

/**
 * @brief Configuration for Count-Min Sketch
 *
 * References:
 * Cormode, G., Muthukrishnan, S. (2005).
 * An Improved Data Stream Summary: The Count-Min Sketch and its Applications.
 * Journal of Algorithms, 55(1), 58-75.
 * Paper name: "Count-Min Sketch" or "CM Sketch"
 */
struct CMSConfig {
    double epsilon;           // Error bound: estimate within epsilon * total_count
    double delta;             // Failure probability: 1 - delta confidence
    bool conservative_update; // Use conservative update (default: false)
    uint counter_bits;        // Bits per counter (8, 16, 32, or 64)

    /**
     * @brief Validate configuration parameters
     */
    void validate() const {
        if (epsilon <= 0.0 || epsilon >= 1.0) {
            throw std::invalid_argument("epsilon must be in (0, 1)");
        }

        if (delta <= 0.0 || delta >= 1.0) {
            throw std::invalid_argument("delta must be in (0, 1)");
        }

        if (counter_bits != 8 && counter_bits != 16 &&
            counter_bits != 32 && counter_bits != 64) {
            throw std::invalid_argument("counter_bits must be 8, 16, 32, or 64");
        }
    }

    /**
     * @brief Calculate optimal width from epsilon
     *
     * From Cormode & Muthukrishnan (2005), Section 2.1:
     * width = ceil(e / epsilon)
     */
    static uint calculate_width(double epsilon) {
        return static_cast<uint>(std::ceil(std::exp(1.0) / epsilon));
    }

    /**
     * @brief Calculate optimal depth from delta
     *
     * From Cormode & Muthukrishnan (2005), Section 2.1:
     * depth = ceil(ln(1 / delta))
     */
    static uint calculate_depth(double delta) {
        return static_cast<uint>(std::ceil(std::log(1.0 / delta)));
    }
};

/**
 * @brief Count-Min Sketch Implementation
 *
 * A probabilistic data structure for frequency estimation in data streams.
 * Provides approximate frequency counts with probabilistic error bounds.
 *
 * Key properties (Cormode & Muthukrishnan, 2005):
 * - Space: O((1/epsilon) * log(1/delta)) counters
 * - Update time: O(log(1/delta))
 * - Query time: O(log(1/delta))
 * - Error guarantee: With probability >= 1-delta, estimate is within epsilon*N of true value
 *   where N is the total number of items inserted
 *
 * Algorithm (Cormode & Muthukrishnan, 2005, Section 2):
 * - Update: Hash element to depth positions, increment all depth counters
 * - Query: Hash element to depth positions, return minimum of depth counters
 * - Conservative update: Only increment counters that equal the current minimum
 *
 * @see CMSConfig for configuration parameters
 */
class CountMinSketch {
private:
    CMSConfig config_;
    uint width_;        // Number of counters per row (calculated from epsilon)
    uint depth_;        // Number of rows (calculated from delta)
    uint64_t total_;    // Total number of items inserted

    // Counter matrix: depth_ rows × width_ columns
    // Use different storage based on counter_bits
    std::vector<std::vector<uint8_t>> counters_8_;
    std::vector<std::vector<uint16_t>> counters_16_;
    std::vector<std::vector<uint32_t>> counters_32_;
    std::vector<std::vector<uint64_t>> counters_64_;

    // Hash seeds for each row
    std::vector<uint64_t> row_seeds_;
    static constexpr uint64_t BASE_SEED = 0x9e3779b97f4a7c15ULL;

    /**
     * @brief Hash a point to get column index for a specific row
     */
    uint hash_to_column(const std::vector<uint64_t> &point, uint row) const {
        uint64_t h1 = row_seeds_[row];
        uint64_t h2 = row_seeds_[row];
        hash(point.data(), point.size(), &h1, &h2);
        return static_cast<uint>(h1 % width_);
    }

    /**
     * @brief Get counter value at (row, col)
     */
    uint64_t get_counter(uint row, uint col) const {
        switch (config_.counter_bits) {
            case 8:  return counters_8_[row][col];
            case 16: return counters_16_[row][col];
            case 32: return counters_32_[row][col];
            case 64: return counters_64_[row][col];
            default: return 0;
        }
    }

    /**
     * @brief Set counter value at (row, col)
     */
    void set_counter(uint row, uint col, uint64_t value) {
        switch (config_.counter_bits) {
            case 8:
                counters_8_[row][col] = static_cast<uint8_t>(
                    std::min(value, static_cast<uint64_t>(UINT8_MAX)));
                break;
            case 16:
                counters_16_[row][col] = static_cast<uint16_t>(
                    std::min(value, static_cast<uint64_t>(UINT16_MAX)));
                break;
            case 32:
                counters_32_[row][col] = static_cast<uint32_t>(
                    std::min(value, static_cast<uint64_t>(UINT32_MAX)));
                break;
            case 64:
                counters_64_[row][col] = value;
                break;
        }
    }

    /**
     * @brief Increment counter at (row, col) with saturation
     */
    void increment_counter(uint row, uint col) {
        uint64_t current = get_counter(row, col);
        uint64_t max_val = (1ULL << config_.counter_bits) - 1;
        if (config_.counter_bits == 64) {
            max_val = UINT64_MAX;
        }

        if (current < max_val) {
            set_counter(row, col, current + 1);
        }
    }

public:
    /**
     * @brief Construct a Count-Min Sketch with given configuration
     */
    explicit CountMinSketch(const CMSConfig &conf) : config_(conf), total_(0) {
        config_.validate();

        // Calculate dimensions from epsilon and delta
        width_ = CMSConfig::calculate_width(config_.epsilon);
        depth_ = CMSConfig::calculate_depth(config_.delta);

        // Initialize counter matrix
        switch (config_.counter_bits) {
            case 8:
                counters_8_.resize(depth_, std::vector<uint8_t>(width_, 0));
                break;
            case 16:
                counters_16_.resize(depth_, std::vector<uint16_t>(width_, 0));
                break;
            case 32:
                counters_32_.resize(depth_, std::vector<uint32_t>(width_, 0));
                break;
            case 64:
                counters_64_.resize(depth_, std::vector<uint64_t>(width_, 0));
                break;
        }

        // Initialize hash seeds for each row
        row_seeds_.resize(depth_);
        for (uint i = 0; i < depth_; ++i) {
            row_seeds_[i] = BASE_SEED + (i * 0x123456789ABCDEFULL);
        }
    }

    /**
     * @brief Insert element (Cormode & Muthukrishnan, 2005, Algorithm UPDATE)
     *
     * Standard update:
     * - Hash element to depth positions
     * - Increment all depth counters
     *
     * Conservative update (Estan & Varghese, 2002):
     * - Hash element to depth positions
     * - Find minimum among depth counters
     * - Only increment counters that equal the minimum
     *
     * Paper reference: Section 2.1 (standard), Section 3.2 (conservative)
     */
    void put(const std::vector<uint64_t> &point) {
        if (config_.conservative_update) {
            // Conservative update: only increment counters at minimum
            std::vector<uint> columns(depth_);
            for (uint i = 0; i < depth_; ++i) {
                columns[i] = hash_to_column(point, i);
            }

            // Find minimum count
            uint64_t min_count = get_counter(0, columns[0]);
            for (uint i = 1; i < depth_; ++i) {
                min_count = std::min(min_count, get_counter(i, columns[i]));
            }

            // Only increment counters that equal minimum
            for (uint i = 0; i < depth_; ++i) {
                if (get_counter(i, columns[i]) == min_count) {
                    increment_counter(i, columns[i]);
                }
            }
        } else {
            // Standard update: increment all counters
            for (uint i = 0; i < depth_; ++i) {
                uint col = hash_to_column(point, i);
                increment_counter(i, col);
            }
        }

        total_++;
    }

    /**
     * @brief Batch insertion
     */
    void put_all(const std::vector<std::vector<uint64_t>> &points) {
        for (const auto &point : points) {
            put(point);
        }
    }

    /**
     * @brief Query count (Cormode & Muthukrishnan, 2005, Algorithm QUERY)
     *
     * Returns minimum count across all depth rows.
     *
     * Error guarantee: With probability >= 1-delta,
     * count[x] <= estimate[x] <= count[x] + epsilon * total
     *
     * Paper reference: Section 2.1, Theorem 1
     */
    uint64_t get_min(const std::vector<uint64_t> &point) const {
        uint64_t min_count = get_counter(0, hash_to_column(point, 0));

        for (uint i = 1; i < depth_; ++i) {
            uint col = hash_to_column(point, i);
            min_count = std::min(min_count, get_counter(i, col));
        }

        return min_count;
    }

    /**
     * @brief Membership test
     */
    bool get_bool(const std::vector<uint64_t> &point) const {
        return get_min(point) > 0;
    }

    /**
     * @brief Get mean estimate (alternative estimator)
     *
     * Returns average count across all depth rows.
     * May be less conservative than minimum but more accurate in some cases.
     */
    uint64_t get_mean(const std::vector<uint64_t> &point) const {
        uint64_t sum = 0;
        for (uint i = 0; i < depth_; ++i) {
            uint col = hash_to_column(point, i);
            sum += get_counter(i, col);
        }
        return sum / depth_;
    }

    /**
     * @brief Get actual epsilon based on current total
     *
     * Actual error bound: estimate <= count + epsilon_actual * total
     */
    double epsilon_actual() const {
        return std::exp(1.0) / static_cast<double>(width_);
    }

    /**
     * @brief Get actual delta (failure probability)
     *
     * Actual confidence: 1 - delta_actual
     */
    double delta_actual() const {
        return std::exp(-static_cast<double>(depth_));
    }

    /**
     * @brief Get total number of items inserted
     */
    uint64_t total_count() const {
        return total_;
    }

    /**
     * @brief Get dimensions
     */
    uint width() const { return width_; }
    uint depth() const { return depth_; }

    /**
     * @brief Get counter configuration
     */
    uint counter_bits() const { return config_.counter_bits; }
    bool conservative() const { return config_.conservative_update; }

    /**
     * @brief Detect heavy hitters (elements with frequency > threshold * total)
     *
     * Returns all elements from a candidate set that have estimated frequency
     * above the threshold.
     *
     * Note: This requires a candidate set since CM Sketch doesn't store elements.
     * In practice, this would be used with a separate data structure to track
     * candidate elements.
     *
     * @param candidates Set of candidate elements to check
     * @param threshold Frequency threshold (0.0 to 1.0)
     * @return Elements with estimated frequency > threshold * total
     */
    std::vector<std::vector<uint64_t>> get_heavy_hitters(
        const std::vector<std::vector<uint64_t>> &candidates,
        double threshold) const {

        std::vector<std::vector<uint64_t>> heavy_hitters;
        uint64_t threshold_count = static_cast<uint64_t>(threshold * total_);

        for (const auto &candidate : candidates) {
            uint64_t count = get_min(candidate);
            if (count >= threshold_count) {
                heavy_hitters.push_back(candidate);
            }
        }

        return heavy_hitters;
    }

    /**
     * @brief Get memory usage in bytes
     */
    uint64_t memory_usage() const {
        uint64_t counter_bytes = 0;
        switch (config_.counter_bits) {
            case 8:  counter_bytes = 1; break;
            case 16: counter_bytes = 2; break;
            case 32: counter_bytes = 4; break;
            case 64: counter_bytes = 8; break;
        }
        return width_ * depth_ * counter_bytes;
    }

    /**
     * @brief Serialize to binary format for WASM/storage
     *
     * Binary format:
     * - Config (28 bytes): width(4) + depth(4) + counter_bits(1) + conservative(1) +
     *                      epsilon(8) + delta(8) + total(8)
     * - Counter data (width * depth * counter_bytes)
     */
    std::vector<uint8_t> to_bytes() const {
        std::vector<uint8_t> buffer;

        // Helper lambda to write values
        auto write_val = [&buffer](const auto& val) {
            const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&val);
            buffer.insert(buffer.end(), bytes, bytes + sizeof(val));
        };

        // Write config
        write_val(width_);
        write_val(depth_);
        write_val(config_.counter_bits);
        uint8_t conservative_byte = config_.conservative_update ? 1 : 0;
        write_val(conservative_byte);
        write_val(config_.epsilon);
        write_val(config_.delta);
        write_val(total_);

        // Write counter data
        for (uint row = 0; row < depth_; ++row) {
            for (uint col = 0; col < width_; ++col) {
                uint64_t count = get_counter(row, col);
                // Write in native counter size
                switch (config_.counter_bits) {
                    case 8: {
                        uint8_t val = static_cast<uint8_t>(count);
                        write_val(val);
                        break;
                    }
                    case 16: {
                        uint16_t val = static_cast<uint16_t>(count);
                        write_val(val);
                        break;
                    }
                    case 32: {
                        uint32_t val = static_cast<uint32_t>(count);
                        write_val(val);
                        break;
                    }
                    case 64: {
                        write_val(count);
                        break;
                    }
                }
            }
        }

        return buffer;
    }

    /**
     * @brief Deserialize from binary format
     *
     * Constructs a CountMinSketch from binary data created by to_bytes()
     */
    static CountMinSketch from_bytes(const std::vector<uint8_t>& bytes) {
        const uint8_t* ptr = bytes.data();

        // Helper lambda to read values
        auto read_val = [&ptr](auto& val) {
            std::memcpy(&val, ptr, sizeof(val));
            ptr += sizeof(val);
        };

        // Read config
        uint width, depth, counter_bits;
        uint8_t conservative_byte;
        double epsilon, delta;
        uint64_t total;

        read_val(width);
        read_val(depth);
        read_val(counter_bits);
        read_val(conservative_byte);
        read_val(epsilon);
        read_val(delta);
        read_val(total);

        bool conservative = conservative_byte != 0;

        // Construct CMS with config
        CMSConfig config{epsilon, delta, conservative, counter_bits};
        CountMinSketch cms(config);

        // Verify dimensions match
        if (cms.width_ != width || cms.depth_ != depth) {
            throw std::runtime_error("Dimension mismatch during deserialization");
        }

        // Restore total count
        cms.total_ = total;

        // Read counter data
        for (uint row = 0; row < depth; ++row) {
            for (uint col = 0; col < width; ++col) {
                uint64_t count = 0;
                switch (counter_bits) {
                    case 8: {
                        uint8_t val;
                        read_val(val);
                        count = val;
                        break;
                    }
                    case 16: {
                        uint16_t val;
                        read_val(val);
                        count = val;
                        break;
                    }
                    case 32: {
                        uint32_t val;
                        read_val(val);
                        count = val;
                        break;
                    }
                    case 64: {
                        read_val(count);
                        break;
                    }
                }
                cms.set_counter(row, col, count);
            }
        }

        return cms;
    }

    /**
     * @brief Get summary statistics
     */
    std::string summary() {
        std::stringstream ss;
        ss << "Count-Min Sketch\\n";
        ss << "  Configuration:\\n";
        ss << "    Epsilon (ε): " << config_.epsilon << " (actual: " << epsilon_actual() << ")\\n";
        ss << "    Delta (δ): " << config_.delta << " (actual: " << delta_actual() << ")\\n";
        ss << "    Dimensions: " << depth_ << " × " << width_ << " (depth × width)\\n";
        ss << "    Total counters: " << (depth_ * width_) << "\\n";
        ss << "    Counter bits: " << config_.counter_bits << "\\n";
        ss << "    Conservative update: " << (config_.conservative_update ? "Yes" : "No") << "\\n";
        ss << "  Statistics:\\n";
        ss << "    Total items inserted: " << total_ << "\\n";
        ss << "    Memory usage: " << config_utils::format_memory(memory_usage()) << "\\n";
        ss << "  Error guarantees:\\n";
        ss << "    With probability >= " << (1.0 - delta_actual()) << ":\\n";
        ss << "    estimate <= true_count + " << epsilon_actual() << " * " << total_ << "\\n";
        ss << "    estimate <= true_count + " << static_cast<uint64_t>(epsilon_actual() * total_) << "\\n";
        return ss.str();
    }
};

}  // namespace globimap

#endif  // GLOBIMAP_COUNT_MIN_SKETCH_HPP
