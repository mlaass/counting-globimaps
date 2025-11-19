/**
 * @file wasm_bindings.cpp
 * @brief Emscripten bindings for CBF implementations
 *
 * Exposes C++ counting bloom filters to JavaScript/WebAssembly.
 * Compiled with: emcc -O3 -s WASM=1
 */

#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstdint>

// Define uint type (not standard in all environments)
typedef unsigned int uint;

// Disable OpenMP for WASM
#ifdef MAKE_PARALLEL
#undef MAKE_PARALLEL
#endif

// Include filter implementations
#include "../../include/count_min_sketch.hpp"
#include "../../include/spectral_bloom_filter.hpp"
#include "../../include/counting_globimap.hpp"
#include "../../include/dleft_counting_bf.hpp"

using namespace emscripten;
using namespace globimap;

// ============================================================================
// Filter Type Enum (matches serialization.hpp)
// ============================================================================

enum class FilterType : uint8_t {
    COUNT_MIN_SKETCH = 0,
    SPECTRAL_BF = 1,
    COUNTING_GLOBIMAP = 2,
    DLEFT_CBF = 3,
    UNKNOWN = 255
};

// ============================================================================
// Filter Handle - Polymorphic wrapper for all filter types
// ============================================================================

class FilterHandle {
public:
    virtual ~FilterHandle() = default;
    virtual uint64_t query(double x, double y, int category = -1) const = 0;
    virtual uint64_t memory_usage() const = 0;
    virtual std::string get_info() const = 0;
    virtual FilterType get_type() const = 0;
};

// ============================================================================
// Count-Min Sketch Handle
// ============================================================================

class CMSHandle : public FilterHandle {
private:
    std::unique_ptr<CountMinSketch> filter_;

public:
    explicit CMSHandle(std::unique_ptr<CountMinSketch> filter)
        : filter_(std::move(filter)) {}

    uint64_t query(double x, double y, int category = -1) const override {
        std::vector<uint64_t> point;
        point.push_back(static_cast<uint64_t>(x));
        point.push_back(static_cast<uint64_t>(y));
        if (category >= 0) {
            point.push_back(static_cast<uint64_t>(category));
        }
        return filter_->get_min(point);
    }

    uint64_t memory_usage() const override {
        return filter_->memory_usage();
    }

    std::string get_info() const override {
        std::stringstream ss;
        ss << "Count-Min Sketch (ε=" << filter_->epsilon_actual()
           << ", δ=" << filter_->delta_actual()
           << ", " << filter_->depth() << "×" << filter_->width() << ")";
        return ss.str();
    }

    FilterType get_type() const override {
        return FilterType::COUNT_MIN_SKETCH;
    }

    const CountMinSketch* get_filter() const { return filter_.get(); }
};

// ============================================================================
// Spectral Bloom Filter Handle (TODO)
// ============================================================================

class SBFHandle : public FilterHandle {
private:
    std::unique_ptr<SpectralBloomFilter> filter_;

public:
    explicit SBFHandle(std::unique_ptr<SpectralBloomFilter> filter)
        : filter_(std::move(filter)) {}

    uint64_t query(double x, double y, int category = -1) const override {
        std::vector<uint64_t> point;
        point.push_back(static_cast<uint64_t>(x));
        point.push_back(static_cast<uint64_t>(y));
        if (category >= 0) {
            point.push_back(static_cast<uint64_t>(category));
        }
        return filter_->get_min(point);
    }

    uint64_t memory_usage() const override {
        return filter_->memory_usage();
    }

    std::string get_info() const override {
        std::stringstream ss;
        ss << "Spectral Bloom Filter ("
           << (filter_->memory_usage() / 1024) << " KB)";
        return ss.str();
    }

    FilterType get_type() const override {
        return FilterType::SPECTRAL_BF;
    }
};

// ============================================================================
// WASM API Functions
// ============================================================================

/**
 * @brief Load a filter from binary data
 *
 * @param bytes Binary data from .cbf file (as JavaScript Uint8Array)
 * @return FilterHandle* Opaque pointer to filter (must be freed with free_filter)
 */
// Base64 decode helper
std::vector<uint8_t> base64_decode(const std::string& encoded) {
    static const std::string base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    std::vector<uint8_t> decoded;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T[base64_chars[i]] = i;

    int val = 0, valb = -8;
    for (uint8_t c : encoded) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            decoded.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return decoded;
}

FilterHandle* load_filter_from_data(const std::vector<uint8_t>& data) {
    if (data.size() < 7) {
        throw std::runtime_error("Invalid filter data: too short");
    }

    // Simple heuristic: try to load as CMS first
    try {
        auto cms = std::make_unique<CountMinSketch>(
            CountMinSketch::from_bytes(data)
        );
        return new CMSHandle(std::move(cms));
    } catch (...) {
        // Try other types...
        throw std::runtime_error("Unable to deserialize filter");
    }
}

FilterHandle* load_filter(const std::string& bytes) {
    try {
        // Convert string to vector<uint8_t>
        std::vector<uint8_t> data(bytes.begin(), bytes.end());
        return load_filter_from_data(data);
    } catch (const std::exception& e) {
        // Return nullptr on error (JavaScript will check)
        return nullptr;
    }
}

FilterHandle* load_filter_base64(const std::string& base64) {
    try {
        std::vector<uint8_t> data = base64_decode(base64);
        return load_filter_from_data(data);
    } catch (const std::exception& e) {
        // Return nullptr on error (JavaScript will check)
        return nullptr;
    }
}

/**
 * @brief Query a point in the filter
 *
 * @param filter Filter handle from load_filter()
 * @param x X coordinate (longitude or pixel x)
 * @param y Y coordinate (latitude or pixel y)
 * @param category Optional category ID (-1 for no category)
 * @return uint64_t Estimated count at (x, y, [category])
 */
uint64_t query_filter(FilterHandle* filter, double x, double y, int category) {
    if (!filter) {
        return 0;
    }
    return filter->query(x, y, category);
}

/**
 * @brief Get filter memory usage in bytes
 */
uint64_t get_memory(FilterHandle* filter) {
    if (!filter) {
        return 0;
    }
    return filter->memory_usage();
}

/**
 * @brief Get filter information string
 */
std::string get_info(FilterHandle* filter) {
    if (!filter) {
        return "null filter";
    }
    return filter->get_info();
}

/**
 * @brief Get filter type as integer
 */
int get_type(FilterHandle* filter) {
    if (!filter) {
        return static_cast<int>(FilterType::UNKNOWN);
    }
    return static_cast<int>(filter->get_type());
}

/**
 * @brief Free a filter handle
 */
void free_filter(FilterHandle* filter) {
    delete filter;
}

/**
 * @brief Create a test CMS for development
 *
 * Creates a small Count-Min Sketch for testing without needing serialized data.
 */
FilterHandle* create_test_cms() {
    CMSConfig config{0.01, 0.01, true, 16};
    auto cms = std::make_unique<CountMinSketch>(config);

    // Insert some test data
    cms->put({100, 200, 1});  // Category 1 at (100, 200)
    cms->put({100, 200, 1});
    cms->put({100, 200, 2});  // Category 2 at (100, 200)
    cms->put({100, 200, 2});
    cms->put({100, 200, 2});

    return new CMSHandle(std::move(cms));
}

// ============================================================================
// Emscripten Bindings - Expose to JavaScript
// ============================================================================

EMSCRIPTEN_BINDINGS(cbf_wasm) {
    // Filter Handle (opaque pointer)
    class_<FilterHandle>("FilterHandle");

    // Main API
    function("loadFilter", &load_filter, allow_raw_pointers());
    function("loadFilterBase64", &load_filter_base64, allow_raw_pointers());
    function("queryFilter", &query_filter, allow_raw_pointers());
    function("getMemory", &get_memory, allow_raw_pointers());
    function("getInfo", &get_info, allow_raw_pointers());
    function("getType", &get_type, allow_raw_pointers());
    function("freeFilter", &free_filter, allow_raw_pointers());

    // Development/testing
    function("createTestCMS", &create_test_cms, allow_raw_pointers());

    // Filter type enum
    enum_<FilterType>("FilterType")
        .value("COUNT_MIN_SKETCH", FilterType::COUNT_MIN_SKETCH)
        .value("SPECTRAL_BF", FilterType::SPECTRAL_BF)
        .value("COUNTING_GLOBIMAP", FilterType::COUNTING_GLOBIMAP)
        .value("DLEFT_CBF", FilterType::DLEFT_CBF)
        .value("UNKNOWN", FilterType::UNKNOWN);
}
