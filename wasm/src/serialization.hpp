#pragma once

#include <vector>
#include <cstring>
#include <stdexcept>
#include <cstdint>

// Include all filter implementations
#include "../../include/count_min_sketch.hpp"
#include "../../include/spectral_bloom_filter.hpp"
#include "../../include/counting_globimap.hpp"
#include "../../include/dleft_counting_bf.hpp"

namespace globimap {
namespace serialization {

// Binary format constants
constexpr uint32_t MAGIC = 0x46424343; // "CCBF" (Counting Bloom Filter)
constexpr uint16_t VERSION = 0x0001;

// Filter type identifiers
enum class FilterType : uint8_t {
    COUNT_MIN_SKETCH = 0,
    SPECTRAL_BF = 1,
    COUNTING_GLOBIMAP = 2,
    DLEFT_CBF = 3
};

// Helper functions for serialization
namespace detail {
    template<typename T>
    void write_value(std::vector<uint8_t>& buffer, const T& value) {
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);
        buffer.insert(buffer.end(), bytes, bytes + sizeof(T));
    }

    template<typename T>
    T read_value(const uint8_t*& ptr) {
        T value;
        std::memcpy(&value, ptr, sizeof(T));
        ptr += sizeof(T);
        return value;
    }

    template<typename T>
    void write_vector(std::vector<uint8_t>& buffer, const std::vector<T>& vec) {
        uint64_t size = vec.size();
        write_value(buffer, size);
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(vec.data());
        buffer.insert(buffer.end(), bytes, bytes + size * sizeof(T));
    }

    template<typename T>
    std::vector<T> read_vector(const uint8_t*& ptr) {
        uint64_t size = read_value<uint64_t>(ptr);
        std::vector<T> vec(size);
        std::memcpy(vec.data(), ptr, size * sizeof(T));
        ptr += size * sizeof(T);
        return vec;
    }
}

// ============================================================================
// Count-Min Sketch Serialization
// ============================================================================

inline std::vector<uint8_t> cms_to_bytes(const CountMinSketch& cms) {
    std::vector<uint8_t> buffer;

    // Header
    detail::write_value(buffer, MAGIC);
    detail::write_value(buffer, VERSION);
    detail::write_value(buffer, FilterType::COUNT_MIN_SKETCH);

    // Config
    detail::write_value(buffer, cms.depth());
    detail::write_value(buffer, cms.width());
    detail::write_value(buffer, cms.counter_bits());
    detail::write_value(buffer, cms.conservative());
    detail::write_value(buffer, cms.epsilon_actual());
    detail::write_value(buffer, cms.delta_actual());

    // State - access private members via public methods
    // We need to serialize the counter table
    // For now, we'll need to add public accessor methods to CountMinSketch
    // Placeholder: serialize width * depth counters
    for (uint32_t i = 0; i < cms.depth(); ++i) {
        for (uint32_t j = 0; j < cms.width(); ++j) {
            // NOTE: This requires adding a get_counter(row, col) method to CountMinSketch
            // For now, we'll mark this as TODO and handle in the next iteration
            detail::write_value<uint64_t>(buffer, 0); // Placeholder
        }
    }

    return buffer;
}

inline CMSConfig cms_from_bytes(const std::vector<uint8_t>& bytes, std::vector<uint64_t>& counters_out) {
    const uint8_t* ptr = bytes.data();

    // Verify header
    uint32_t magic = detail::read_value<uint32_t>(ptr);
    if (magic != MAGIC) {
        throw std::runtime_error("Invalid magic number");
    }

    uint16_t version = detail::read_value<uint16_t>(ptr);
    if (version != VERSION) {
        throw std::runtime_error("Unsupported version");
    }

    FilterType type = detail::read_value<FilterType>(ptr);
    if (type != FilterType::COUNT_MIN_SKETCH) {
        throw std::runtime_error("Not a Count-Min Sketch");
    }

    // Read config
    uint32_t depth = detail::read_value<uint32_t>(ptr);
    uint32_t width = detail::read_value<uint32_t>(ptr);
    uint8_t counter_bits = detail::read_value<uint8_t>(ptr);
    bool conservative = detail::read_value<bool>(ptr);
    double epsilon = detail::read_value<double>(ptr);
    double delta = detail::read_value<double>(ptr);

    // Read state
    counters_out.resize(depth * width);
    for (uint32_t i = 0; i < depth * width; ++i) {
        counters_out[i] = detail::read_value<uint64_t>(ptr);
    }

    // Create config (using epsilon/delta constructor)
    CMSConfig config{epsilon, delta, conservative, counter_bits};
    return config;
}

// ============================================================================
// Spectral Bloom Filter Serialization
// ============================================================================

inline std::vector<uint8_t> sbf_to_bytes(const SpectralBloomFilter& sbf) {
    std::vector<uint8_t> buffer;

    // Header
    detail::write_value(buffer, MAGIC);
    detail::write_value(buffer, VERSION);
    detail::write_value(buffer, FilterType::SPECTRAL_BF);

    // Config
    detail::write_value(buffer, sbf.hash_k());
    detail::write_value(buffer, sbf.logsize());
    detail::write_value(buffer, sbf.counter_bits());
    detail::write_value(buffer, sbf.variant());

    // State - counter array
    // Similar issue: need accessor method
    // Placeholder for now
    uint64_t num_counters = 1ULL << sbf.logsize();
    for (uint64_t i = 0; i < num_counters; ++i) {
        detail::write_value<uint64_t>(buffer, 0); // Placeholder
    }

    return buffer;
}

// ============================================================================
// CountingGloBiMap Serialization
// ============================================================================

inline std::vector<uint8_t> globimap_to_bytes(const CountingGloBiMap<>& gbm) {
    std::vector<uint8_t> buffer;

    // Header
    detail::write_value(buffer, MAGIC);
    detail::write_value(buffer, VERSION);
    detail::write_value(buffer, FilterType::COUNTING_GLOBIMAP);

    // Config
    const auto& config = gbm.config();
    detail::write_value(buffer, config.hash_k);
    detail::write_value(buffer, static_cast<uint32_t>(config.layers.size()));

    for (const auto& layer_config : config.layers) {
        detail::write_value(buffer, layer_config.bits);
        detail::write_value(buffer, layer_config.logsize);
    }

    detail::write_value(buffer, config.minimal_increment);
    detail::write_value(buffer, config.cascade_factor);

    // State - use existing as_bytes() method from Layer class
    // This is more complex due to multi-layer structure
    // We'll need to iterate through layers and serialize each
    const auto& layers = gbm.layers();
    for (const auto& layer : layers) {
        // Call layer.as_bytes() for each bit depth
        // This requires access to the Layer objects
        // Placeholder for now
    }

    return buffer;
}

// ============================================================================
// d-Left CBF Serialization
// ============================================================================

inline std::vector<uint8_t> dleft_to_bytes(const DLeftCountingBloomFilter& dleft) {
    std::vector<uint8_t> buffer;

    // Header
    detail::write_value(buffer, MAGIC);
    detail::write_value(buffer, VERSION);
    detail::write_value(buffer, FilterType::DLEFT_CBF);

    // Config
    detail::write_value(buffer, dleft.buckets_per_subtable());
    detail::write_value(buffer, dleft.slots_per_bucket());
    detail::write_value(buffer, dleft.d_subtables());
    detail::write_value(buffer, dleft.fingerprint_bits());
    detail::write_value(buffer, dleft.counter_bits());

    // State - bucket structure with fingerprints and counters
    // Placeholder

    return buffer;
}

} // namespace serialization
} // namespace globimap
