#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION

#include <functional>
#include <iostream>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

// Include CountingGloBiMap implementation
#include "counting_globimap.hpp"

// Type alias for the default CountingGloBiMap
using CountingGloBiMapDefault = globimap::CountingGloBiMap<>;

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
}
