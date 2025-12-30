#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION

#include <functional>
#include <iostream>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

// Include binary GloBiMap implementation (must be first due to hashfn.hpp)
#include "globimap.hpp"

// Include CountingGloBiMap implementation
#include "counting_globimap.hpp"

// Include BlockedBF implementation
#include "blocked_bloom_filter.hpp"

// Type alias for the default CountingGloBiMap
using CountingGloBiMapDefault = globimap::CountingGloBiMap<>;

// Type alias for binary GloBiMap
using GloBiMapDefault = GloBiMap<bool>;

// Python module definition
PYBIND11_MODULE(counting_globimap, m) {
    m.doc() = "CountingGloBiMap: Multi-layer counting bloom filter for cardinality estimation";

    // Expose LayerConfig struct
    py::class_<globimap::LayerConfig>(m, "LayerConfig")
        .def(py::init<>())
        .def(py::init<uint, uint>())
        .def_readwrite("bits", &globimap::LayerConfig::bits,
            "Bit depth of this layer (1, 8, 16, 32, or 64)")
        .def_readwrite("logsize", &globimap::LayerConfig::logsize,
            "Log2 of layer size (layer will have 2^logsize counters)")
        .def("__repr__", [](const globimap::LayerConfig &lc) {
            return "<LayerConfig bits=" + std::to_string(lc.bits) +
                   " logsize=" + std::to_string(lc.logsize) + ">";
        });

    // Expose FilterConfig struct
    py::class_<globimap::FilterConfig>(m, "FilterConfig")
        .def(py::init<>())
        .def(py::init<uint, std::vector<globimap::LayerConfig>>())
        .def_readwrite("hash_k", &globimap::FilterConfig::hash_k,
            "Number of hash functions to use")
        .def_readwrite("layers", &globimap::FilterConfig::layers,
            "List of LayerConfig objects defining the multi-layer structure")
        .def("to_string", &globimap::FilterConfig::to_string,
            "Get string representation of configuration")
        .def("__repr__", [](const globimap::FilterConfig &fc) {
            return "<FilterConfig hash_k=" + std::to_string(fc.hash_k) +
                   " layers=" + std::to_string(fc.layers.size()) + ">";
        });

    // Expose CountingGloBiMap class
    py::class_<CountingGloBiMapDefault>(m, "CountingGloBiMap")
        .def(py::init<const globimap::FilterConfig &, bool>(),
             py::arg("config"),
             py::arg("collect_input") = false,
             "Create a CountingGloBiMap with the given configuration.\n\n"
             "Args:\n"
             "    config (FilterConfig): Multi-layer filter configuration\n"
             "    collect_input (bool): If True, track inserted points for error detection")

        // Insert operations
        .def("put",
             py::overload_cast<const std::vector<uint64_t> &>(&CountingGloBiMapDefault::put),
             py::arg("point"),
             "Insert a point (coordinate pair) into the filter.\n\n"
             "Args:\n"
             "    point (list): [x, y] coordinates as uint64 values")

        .def("put_all",
             &CountingGloBiMapDefault::put_all,
             py::arg("points"),
             "Insert multiple points at once.\n\n"
             "Args:\n"
             "    points (list): Flat list of coordinates [x1, y1, x2, y2, ...]")

        // Query operations
        .def("get_bool",
             &CountingGloBiMapDefault::get_bool,
             py::arg("point"),
             "Check if a point exists (probabilistic membership test).\n\n"
             "Args:\n"
             "    point (list): [x, y] coordinates\n"
             "Returns:\n"
             "    bool: True if point may exist (with false positives), False if definitely not")

        .def("get_min",
             &CountingGloBiMapDefault::get_min,
             py::arg("point"),
             "Get minimum count estimate across all hash functions (min-count estimator).\n\n"
             "Args:\n"
             "    point (list): [x, y] coordinates\n"
             "Returns:\n"
             "    int: Estimated count/cardinality for this point")

        .def("get_mean",
             &CountingGloBiMapDefault::get_mean<double>,
             py::arg("point"),
             "Get mean count estimate across all hash functions.\n\n"
             "Args:\n"
             "    point (list): [x, y] coordinates\n"
             "Returns:\n"
             "    float: Mean estimated count")

        // Bulk query operations
        .def("to_hashfn",
             &CountingGloBiMapDefault::to_hashfn,
             py::arg("points"),
             "Convert points to hash function values.\n\n"
             "Args:\n"
             "    points (list): Flat list of coordinates\n"
             "Returns:\n"
             "    list: Hash function values")

        .def("get_sum_hashfn",
             &CountingGloBiMapDefault::get_sum_hashfn,
             py::arg("hashfn"),
             "Get sum of counts for pre-computed hash values.\n\n"
             "Args:\n"
             "    hashfn (list): Hash function values from to_hashfn()\n"
             "Returns:\n"
             "    int: Sum of estimated counts")

        .def("get_sum_raster_collected",
             &CountingGloBiMapDefault::get_sum_raster_collected,
             py::arg("raster"),
             "Get sum of actual collected counts (requires collect_input=True).\n\n"
             "Args:\n"
             "    raster (list): Flat list of coordinates\n"
             "Returns:\n"
             "    int: Sum of actual counts")

        // Error detection
        .def("detect_errors",
             &CountingGloBiMapDefault::detect_errors,
             py::arg("x"), py::arg("y"), py::arg("width"), py::arg("height"),
             "Detect errors in a rectangular region (requires collect_input=True).\n\n"
             "Args:\n"
             "    x (int): Start x coordinate\n"
             "    y (int): Start y coordinate\n"
             "    width (int): Region width\n"
             "    height (int): Region height")

        .def("error_magnitudes",
             &CountingGloBiMapDefault::error_magnitudes,
             "Get list of error magnitudes detected.\n\n"
             "Returns:\n"
             "    list: Magnitudes of errors found")

        // Statistics and summaries
        .def("byte_size",
             &CountingGloBiMapDefault::byte_size,
             "Get total memory usage in bytes.\n\n"
             "Returns:\n"
             "    int: Total bytes used by all layers")

        .def("summary",
             &CountingGloBiMapDefault::summary,
             "Get detailed summary of filter state as JSON string.\n\n"
             "Returns:\n"
             "    str: JSON-formatted summary with layer statistics")

        .def("error_summary",
             &CountingGloBiMapDefault::error_summary,
             "Get summary of detected errors as JSON string.\n\n"
             "Returns:\n"
             "    str: JSON-formatted error statistics")

        .def("summary_config",
             &CountingGloBiMapDefault::summary_config,
             "Get configuration summary.\n\n"
             "Returns:\n"
             "    str: Configuration summary")

        // Configuration access
        .def_readonly("config", &CountingGloBiMapDefault::config,
            "Filter configuration")
        .def_readonly("hashcount", &CountingGloBiMapDefault::hashcount,
            "Number of hash functions")
        .def_readonly("collect_input", &CountingGloBiMapDefault::collect_input,
            "Whether input collection is enabled")
        .def_readonly("error_rate", &CountingGloBiMapDefault::error_rate,
            "Detected error rate")

        .def("__repr__", [](const CountingGloBiMapDefault &g) {
            return "<CountingGloBiMap k=" + std::to_string(g.hashcount) +
                   " layers=" + std::to_string(g.config.layers.size()) +
                   " size=" + std::to_string(g.byte_size()) + "B>";
        });

    // Helper functions for creating common configurations
    m.def("make_single_layer_config",
          [](uint k, uint bits, uint logsize) {
              globimap::FilterConfig fc;
              fc.hash_k = k;
              fc.layers.push_back({bits, logsize});
              return fc;
          },
          py::arg("k"), py::arg("bits"), py::arg("logsize"),
          "Create a single-layer filter configuration.\n\n"
          "Args:\n"
          "    k (int): Number of hash functions\n"
          "    bits (int): Bit depth (1, 8, 16, 32, or 64)\n"
          "    logsize (int): Log2 of filter size\n"
          "Returns:\n"
          "    FilterConfig: Configuration with one layer");

    m.def("make_multi_layer_config",
          [](uint k, std::vector<std::pair<uint, uint>> layer_specs) {
              globimap::FilterConfig fc;
              fc.hash_k = k;
              for (const auto &spec : layer_specs) {
                  fc.layers.push_back({spec.first, spec.second});
              }
              return fc;
          },
          py::arg("k"), py::arg("layer_specs"),
          "Create a multi-layer filter configuration.\n\n"
          "Args:\n"
          "    k (int): Number of hash functions\n"
          "    layer_specs (list): List of (bits, logsize) tuples\n"
          "Returns:\n"
          "    FilterConfig: Configuration with multiple layers\n"
          "Example:\n"
          "    config = make_multi_layer_config(8, [(8, 24), (16, 20), (32, 16)])");

    // ========================================================================
    // Binary GloBiMap (simple bloom filter) bindings
    // ========================================================================

    py::class_<GloBiMapDefault>(m, "GloBiMap")
        .def(py::init<>(), "Create an empty binary bloom filter")

        .def("configure", &GloBiMapDefault::configure,
             py::arg("k"), py::arg("logm"),
             "Configure the filter with k hash functions and 2^logm bits")

        .def("put", &GloBiMapDefault::put,
             py::arg("point"),
             "Insert a point into the filter")

        .def("get", &GloBiMapDefault::get,
             py::arg("point"),
             "Check if a point exists in the filter")

        .def("clear", &GloBiMapDefault::clear,
             "Clear the filter and error correction data")

        .def("summary", &GloBiMapDefault::summary,
             "Get JSON summary of filter state")

        .def("add_error", &GloBiMapDefault::add_error,
             py::arg("point"),
             "Register a false positive for error correction")

        // Map a 2D numpy array to the filter
        .def("map", [](GloBiMapDefault &self, py::array_t<double> arr, uint64_t x, uint64_t y) {
            auto buf = arr.request();
            if (buf.ndim != 2) {
                throw std::runtime_error("Input array must be 2D");
            }
            double *ptr = static_cast<double*>(buf.ptr);
            size_t rows = buf.shape[0];
            size_t cols = buf.shape[1];

            for (size_t i = 0; i < rows; ++i) {
                for (size_t j = 0; j < cols; ++j) {
                    if (ptr[i * cols + j] != 0) {
                        self.put({x + i, y + j});
                    }
                }
            }
        }, py::arg("arr"), py::arg("x"), py::arg("y"),
           "Map non-zero values from a 2D array into the filter")

        // Add error correction for false positives
        .def("enforce", [](GloBiMapDefault &self, py::array_t<double> arr, uint64_t x, uint64_t y) {
            auto buf = arr.request();
            if (buf.ndim != 2) {
                throw std::runtime_error("Input array must be 2D");
            }
            double *ptr = static_cast<double*>(buf.ptr);
            size_t rows = buf.shape[0];
            size_t cols = buf.shape[1];

            // Find false positives (filter says yes, but array says no)
            for (size_t i = 0; i < rows; ++i) {
                for (size_t j = 0; j < cols; ++j) {
                    if (ptr[i * cols + j] == 0 && self.get({x + i, y + j})) {
                        self.add_error({static_cast<uint32_t>(x + i), static_cast<uint32_t>(y + j)});
                    }
                }
            }
        }, py::arg("arr"), py::arg("x"), py::arg("y"),
           "Find and register false positives for error correction")

        // Rasterize returns a numpy array
        .def("rasterize", [](GloBiMapDefault &self, uint64_t x, uint64_t y, uint32_t width, uint32_t height) {
            auto &storage = self.rasterize(x, y, width, height);
            py::array_t<double> result({height, width});
            auto buf = result.request();
            double *ptr = static_cast<double*>(buf.ptr);
            std::copy(storage.begin(), storage.end(), ptr);
            return result;
        }, py::arg("x"), py::arg("y"), py::arg("width"), py::arg("height"),
           "Rasterize a rectangular region into a numpy array")

        // Apply correction and return numpy array
        .def("correct", [](GloBiMapDefault &self, uint64_t x, uint64_t y, uint32_t width, uint32_t height) {
            auto &storage = self.apply_correction(x, y, width, height);
            py::array_t<double> result({height, width});
            auto buf = result.request();
            double *ptr = static_cast<double*>(buf.ptr);
            std::copy(storage.begin(), storage.end(), ptr);
            return result;
        }, py::arg("x"), py::arg("y"), py::arg("width"), py::arg("height"),
           "Apply error correction to rasterized region")

        // Serialization
        .def("get_buffer", [](GloBiMapDefault &self) {
            std::string buf;
            self.tobuffer(buf);
            return py::bytes(buf);
        }, "Serialize filter to bytes")

        .def("from_buffer", [](GloBiMapDefault &self, py::bytes data) {
            std::string buf = data;
            self._frombuffer(buf);
        }, py::arg("data"),
           "Deserialize filter from bytes")

        .def("__repr__", [](const GloBiMapDefault &g) {
            return "<GloBiMap size=" + std::to_string(g.filter.size()) + " bits>";
        });

    // Alias for compatibility with original globimap API
    m.def("globimap", []() { return GloBiMapDefault(); },
          "Create a new binary bloom filter (alias for GloBiMap())");

    // ========================================================================
    // BlockedBloomFilter Bindings
    // ========================================================================

    // Expose IntraBlockStrategy enum for BlockedBloomFilter
    py::enum_<globimap::IntraBlockStrategy>(m, "IntraBlockStrategy")
        .value("DOUBLE_HASH", globimap::IntraBlockStrategy::DOUBLE_HASH)
        .value("PATTERN_LOOKUP", globimap::IntraBlockStrategy::PATTERN_LOOKUP);

    py::class_<globimap::BlockedBFConfig>(m, "BlockedBFConfig")
        .def(py::init<>())
        .def_readwrite("num_blocks", &globimap::BlockedBFConfig::num_blocks,
            "Number of 256-bit blocks (direct mode, overrides expected_items if > 0)")
        .def_readwrite("hash_k", &globimap::BlockedBFConfig::hash_k,
            "Number of hash functions (direct mode, required when num_blocks > 0)")
        .def_readwrite("expected_items", &globimap::BlockedBFConfig::expected_items,
            "Expected number of items (legacy mode, used if num_blocks == 0)")
        .def_readwrite("false_positive_rate", &globimap::BlockedBFConfig::false_positive_rate,
            "Target false positive rate (legacy mode)")
        .def_readwrite("intra_strategy", &globimap::BlockedBFConfig::intra_strategy,
            "Intra-block hashing strategy (DOUBLE_HASH or PATTERN_LOOKUP)")
        .def_readwrite("pattern_table_size", &globimap::BlockedBFConfig::pattern_table_size,
            "Pattern table size for PATTERN_LOOKUP (256, 512, 1024, or 2048)")
        .def("computed_k", &globimap::BlockedBFConfig::computed_k,
            "Get computed k value")
        .def("computed_num_blocks", &globimap::BlockedBFConfig::computed_num_blocks,
            "Get computed number of blocks")
        .def("computed_bits", &globimap::BlockedBFConfig::computed_bits,
            "Get computed number of bits")
        .def("__repr__", &globimap::BlockedBFConfig::to_string);

    using BlockedBF = globimap::BlockedBloomFilter<>;
    py::class_<BlockedBF>(m, "BlockedBloomFilter")
        .def(py::init<const globimap::BlockedBFConfig&>(),
             py::arg("config"),
             "Create a Blocked Bloom Filter with the given configuration")
        .def("put", &BlockedBF::put,
             py::arg("point"),
             "Insert a point (list of uint64 coordinates)")
        .def("query", &BlockedBF::get_bool,
             py::arg("point"),
             "Query membership for a point")
        .def("clear", &BlockedBF::clear,
             "Clear all blocks")
        .def("memory_bytes", &BlockedBF::memory_usage,
             "Get memory usage in bytes")
        .def("num_blocks", &BlockedBF::num_blocks,
             "Get number of blocks")
        .def("hash_k", &BlockedBF::hash_k,
             "Get number of hash functions")
        .def("summary", &BlockedBF::summary,
             "Get JSON summary of filter state")
        .def_readonly("config", &BlockedBF::config)
        .def("__repr__", [](const BlockedBF &f) {
            return "<BlockedBloomFilter blocks=" + std::to_string(f.num_blocks()) +
                   " k=" + std::to_string(f.hash_k()) +
                   " memory=" + std::to_string(f.memory_usage()) + "B>";
        });
}
