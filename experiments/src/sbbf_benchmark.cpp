/*
 * Spatial-Blocked Bloom Filter Benchmark
 *
 * Compares SBBF variants with traditional bloom filters:
 * - SBBF (Morton 2D/3D)
 * - SBBF (Hilbert 2D/3D)
 * - BlockedBloomFilter (cache-line aligned baseline)
 * - RegisterBlockedBloomFilter (64-bit atomic baseline)
 *
 * Key metrics (via hardware performance counters):
 * - Nanoseconds per operation
 * - CPU cycles per operation
 * - Instructions per operation
 * - Cache misses per operation
 * - False positive rate
 * - Memory usage
 *
 * Requirements:
 * - Linux with perf_event support
 * - May need: sudo sysctl kernel.perf_event_paranoid=1
 *
 * Output: JSON files in results/sbbf/ for Jupyter analysis
 */

#define ANKERL_NANOBENCH_IMPLEMENT
#include "external/nanobench.h"

#include "spatial_blocked_bloom_filter.hpp"
#include "blocked_bloom_filter.hpp"
#include "register_blocked_bf.hpp"

#include "json.hpp"
#include <highfive/highfive.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <vector>

using json = nlohmann::json;

using namespace sbbf;
using namespace globimap;

const std::string results_path = "./results/sbbf/";

// ============================================================================
// Utility Functions
// ============================================================================

std::vector<std::string> split_string(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(s);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    return tokens;
}

std::vector<size_t> parse_size_list(const std::string& s) {
    std::vector<size_t> result;
    for (const auto& token : split_string(s, ',')) {
        result.push_back(std::stoull(token));
    }
    return result;
}

std::vector<unsigned> parse_unsigned_list(const std::string& s) {
    std::vector<unsigned> result;
    for (const auto& token : split_string(s, ',')) {
        result.push_back(static_cast<unsigned>(std::stoul(token)));
    }
    return result;
}

std::string get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&time);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%m%d_%H%M%S");
    return oss.str();
}

std::string get_iso_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::gmtime(&time);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

// ============================================================================
// Configuration
// ============================================================================

struct BenchmarkConfig {
    // Existing nanobench settings
    size_t epochs = 11;
    size_t min_iters = 1000;
    size_t warmup = 100;
    size_t epoch_time_ms = 100;
    std::string scenario = "all";

    // Parameter sweep lists
    std::vector<size_t> element_counts = {100000};
    std::vector<unsigned> log_block_sizes = {17};
    std::vector<unsigned> hash_k_values = {4};
    std::vector<unsigned> coord_bits_values = {16};
    std::vector<std::string> sfc_types = {"morton", "hilbert"};
    std::vector<std::string> strategies = {"double_hash", "pattern_lookup"};
    std::vector<std::string> distributions = {"uniform"};
    std::vector<unsigned> dimensions = {2};
    size_t num_clusters = 100;

    // Dataset
    std::string dataset_path;  // Empty = synthetic

    // Output
    std::string output_path;
    std::string output_format = "json";

    // Baseline mode: "none", "reduced", "full"
    std::string baseline_mode = "reduced";

    // Suite preset: "quick", "paper", "full", or empty for custom
    std::string suite;

    void apply_suite_preset() {
        if (suite == "quick") {
            element_counts = {50000};
            log_block_sizes = {17};
            hash_k_values = {4};
            coord_bits_values = {16};
            sfc_types = {"hilbert"};
            strategies = {"pattern_lookup"};
            distributions = {"uniform"};
            dimensions = {2};
            baseline_mode = "none";
        } else if (suite == "paper") {
            element_counts = {10000, 50000, 100000, 500000, 1000000};
            log_block_sizes = {14, 15, 16, 17, 18, 19, 20};
            hash_k_values = {2, 4, 6, 8};
            coord_bits_values = {8, 12, 16, 20};  // Must be multiples of 4 for Hilbert
            sfc_types = {"morton", "hilbert"};
            strategies = {"double_hash", "pattern_lookup"};
            distributions = {"uniform", "clustered"};
            dimensions = {2, 3};
            baseline_mode = "reduced";
        } else if (suite == "full") {
            element_counts = {10000, 50000, 100000, 500000, 1000000};
            log_block_sizes = {14, 15, 16, 17, 18, 19, 20};
            hash_k_values = {2, 4, 6, 8};
            coord_bits_values = {8, 12, 16, 20};  // Must be multiples of 4 for Hilbert
            sfc_types = {"morton", "hilbert"};
            strategies = {"double_hash", "pattern_lookup"};
            distributions = {"uniform", "clustered"};
            dimensions = {2, 3};
            baseline_mode = "full";
        }
    }

    void print() const {
        std::cout << "Benchmark configuration:\n";
        std::cout << "  Epochs: " << epochs << "\n";
        std::cout << "  Min iterations/epoch: " << min_iters << "\n";
        std::cout << "  Warmup iterations: " << warmup << "\n";
        std::cout << "  Max epoch time: " << epoch_time_ms << " ms\n";
        std::cout << "  Scenario: " << scenario << "\n";
        if (!suite.empty()) {
            std::cout << "  Suite: " << suite << "\n";
        }
        std::cout << "  Elements: ";
        for (size_t i = 0; i < element_counts.size(); ++i) {
            if (i > 0) std::cout << ",";
            std::cout << element_counts[i];
        }
        std::cout << "\n";
        std::cout << "  Log blocks: ";
        for (size_t i = 0; i < log_block_sizes.size(); ++i) {
            if (i > 0) std::cout << ",";
            std::cout << log_block_sizes[i];
        }
        std::cout << "\n";
        std::cout << "  Hash k: ";
        for (size_t i = 0; i < hash_k_values.size(); ++i) {
            if (i > 0) std::cout << ",";
            std::cout << hash_k_values[i];
        }
        std::cout << "\n";
        std::cout << "  Coord bits: ";
        for (size_t i = 0; i < coord_bits_values.size(); ++i) {
            if (i > 0) std::cout << ",";
            std::cout << coord_bits_values[i];
        }
        std::cout << "\n";
        std::cout << "  SFC types: ";
        for (size_t i = 0; i < sfc_types.size(); ++i) {
            if (i > 0) std::cout << ",";
            std::cout << sfc_types[i];
        }
        std::cout << "\n";
        std::cout << "  Strategies: ";
        for (size_t i = 0; i < strategies.size(); ++i) {
            if (i > 0) std::cout << ",";
            std::cout << strategies[i];
        }
        std::cout << "\n";
        std::cout << "  Dimensions: ";
        for (size_t i = 0; i < dimensions.size(); ++i) {
            if (i > 0) std::cout << ",";
            std::cout << dimensions[i] << "D";
        }
        std::cout << "\n";
        std::cout << "  Distributions: ";
        for (size_t i = 0; i < distributions.size(); ++i) {
            if (i > 0) std::cout << ",";
            std::cout << distributions[i];
        }
        std::cout << "\n";
        std::cout << "  Baseline mode: " << baseline_mode << "\n";
        if (!dataset_path.empty()) {
            std::cout << "  Dataset: " << dataset_path << "\n";
        }
        if (!output_path.empty()) {
            std::cout << "  Output: " << output_path << "\n";
        }
    }
};

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --epochs N        Number of epochs per benchmark (default: 11)\n";
    std::cout << "  --min-iters N     Minimum iterations per epoch (default: 1000)\n";
    std::cout << "  --warmup N        Warmup iterations (default: 100)\n";
    std::cout << "  --epoch-time MS   Max time per epoch in ms (default: 100)\n";
    std::cout << "  --scenario S      Run only: 2d, 3d, strategy, scan, sweep, all (default: all)\n";
    std::cout << "\nParameter sweep options:\n";
    std::cout << "  --suite S         Preset suite: quick, paper, full\n";
    std::cout << "  --elements L      Comma-separated element counts (default: 100000)\n";
    std::cout << "  --log-blocks L    Comma-separated log2 block counts (default: 17)\n";
    std::cout << "  --hash-k L        Comma-separated hash k values (default: 4)\n";
    std::cout << "  --coord-bits L    Comma-separated coord bits (default: 16)\n";
    std::cout << "  --sfc L           Comma-separated SFC types: morton,hilbert\n";
    std::cout << "  --strategy L      Comma-separated strategies: double_hash,pattern_lookup\n";
    std::cout << "  --dims L          Comma-separated dimensions: 2,3 (default: 2)\n";
    std::cout << "  --distribution L  Comma-separated: uniform,clustered (default: uniform)\n";
    std::cout << "  --clusters N      Number of clusters for clustered distribution (default: 100)\n";
    std::cout << "\nData source options:\n";
    std::cout << "  --dataset PATH    Path to HDF5 or CSV file for real data\n";
    std::cout << "\nOutput options:\n";
    std::cout << "  --output PATH     Output directory or file path\n";
    std::cout << "  --format F        Output format: json, csv (default: json)\n";
    std::cout << "  --baseline M      Baseline mode: none, reduced, full (default: reduced)\n";
    std::cout << "\nExamples:\n";
    std::cout << "  " << prog << " --suite quick\n";
    std::cout << "  " << prog << " --suite paper --output results/sbbf/\n";
    std::cout << "  " << prog << " --scenario sweep --elements 50000,100000 --log-blocks 16,17,18\n";
    std::cout << "  " << prog << " --dataset ./datasets/hdf5/gdelt_events.h5 --output gdelt.json\n";
    std::cout << "\n  --help            Show this help\n";
}

BenchmarkConfig parse_args(int argc, char* argv[]) {
    BenchmarkConfig config;
    bool suite_set = false;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            exit(0);
        } else if (strcmp(argv[i], "--epochs") == 0 && i + 1 < argc) {
            config.epochs = std::stoul(argv[++i]);
        } else if (strcmp(argv[i], "--min-iters") == 0 && i + 1 < argc) {
            config.min_iters = std::stoul(argv[++i]);
        } else if (strcmp(argv[i], "--warmup") == 0 && i + 1 < argc) {
            config.warmup = std::stoul(argv[++i]);
        } else if (strcmp(argv[i], "--epoch-time") == 0 && i + 1 < argc) {
            config.epoch_time_ms = std::stoul(argv[++i]);
        } else if (strcmp(argv[i], "--scenario") == 0 && i + 1 < argc) {
            config.scenario = argv[++i];
        } else if (strcmp(argv[i], "--suite") == 0 && i + 1 < argc) {
            config.suite = argv[++i];
            suite_set = true;
        } else if (strcmp(argv[i], "--elements") == 0 && i + 1 < argc) {
            config.element_counts = parse_size_list(argv[++i]);
        } else if (strcmp(argv[i], "--log-blocks") == 0 && i + 1 < argc) {
            config.log_block_sizes = parse_unsigned_list(argv[++i]);
        } else if (strcmp(argv[i], "--hash-k") == 0 && i + 1 < argc) {
            config.hash_k_values = parse_unsigned_list(argv[++i]);
        } else if (strcmp(argv[i], "--coord-bits") == 0 && i + 1 < argc) {
            config.coord_bits_values = parse_unsigned_list(argv[++i]);
        } else if (strcmp(argv[i], "--sfc") == 0 && i + 1 < argc) {
            config.sfc_types = split_string(argv[++i], ',');
        } else if (strcmp(argv[i], "--strategy") == 0 && i + 1 < argc) {
            config.strategies = split_string(argv[++i], ',');
        } else if (strcmp(argv[i], "--dims") == 0 && i + 1 < argc) {
            config.dimensions = parse_unsigned_list(argv[++i]);
        } else if (strcmp(argv[i], "--distribution") == 0 && i + 1 < argc) {
            config.distributions = split_string(argv[++i], ',');
        } else if (strcmp(argv[i], "--clusters") == 0 && i + 1 < argc) {
            config.num_clusters = std::stoul(argv[++i]);
        } else if (strcmp(argv[i], "--dataset") == 0 && i + 1 < argc) {
            config.dataset_path = argv[++i];
        } else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            config.output_path = argv[++i];
        } else if (strcmp(argv[i], "--format") == 0 && i + 1 < argc) {
            config.output_format = argv[++i];
        } else if (strcmp(argv[i], "--baseline") == 0 && i + 1 < argc) {
            config.baseline_mode = argv[++i];
        } else {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            print_usage(argv[0]);
            exit(1);
        }
    }

    // Apply suite preset first, then individual overrides take effect
    if (suite_set) {
        config.apply_suite_preset();
    }

    return config;
}

BenchmarkConfig g_config;

// ============================================================================
// Helper: Compute expected_items from memory budget
// ============================================================================

// Given target memory (bytes) and FPR, compute expected_items for bloom filters
// Formula: bits = -1.44 * n * log2(fpr), so n = bits / (-1.44 * log2(fpr))
size_t compute_expected_items(size_t memory_bytes, double target_fpr) {
    size_t bits = memory_bytes * 8;
    double factor = -1.44 * std::log2(target_fpr);
    return static_cast<size_t>(bits / factor);
}

// ============================================================================
// Data Generation
// ============================================================================

struct Point2D {
    uint32_t x, y;
};

struct Point3D {
    uint32_t x, y, z;
};

std::vector<Point2D> generate_uniform_2d(size_t n, uint32_t max_coord, uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<uint32_t> dist(0, max_coord);
    std::vector<Point2D> points;
    points.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        points.push_back({dist(rng), dist(rng)});
    }
    return points;
}

std::vector<Point3D> generate_uniform_3d(size_t n, uint32_t max_coord, uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<uint32_t> dist(0, max_coord);
    std::vector<Point3D> points;
    points.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        points.push_back({dist(rng), dist(rng), dist(rng)});
    }
    return points;
}

std::vector<Point2D> generate_clustered_2d(size_t n, uint32_t max_coord,
                                            size_t num_clusters, uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<uint32_t> center_dist(0, max_coord);

    std::vector<Point2D> centers;
    for (size_t i = 0; i < num_clusters; ++i) {
        centers.push_back({center_dist(rng), center_dist(rng)});
    }

    std::vector<Point2D> points;
    points.reserve(n);
    std::uniform_int_distribution<size_t> cluster_dist(0, num_clusters - 1);
    std::normal_distribution<double> offset_dist(0, max_coord / 20.0);

    for (size_t i = 0; i < n; ++i) {
        auto& center = centers[cluster_dist(rng)];
        int32_t ox = static_cast<int32_t>(offset_dist(rng));
        int32_t oy = static_cast<int32_t>(offset_dist(rng));
        uint32_t x = static_cast<uint32_t>(std::clamp<int32_t>(
            center.x + ox, 0, max_coord));
        uint32_t y = static_cast<uint32_t>(std::clamp<int32_t>(
            center.y + oy, 0, max_coord));
        points.push_back({x, y});
    }
    return points;
}

std::vector<Point3D> generate_clustered_3d(size_t n, uint32_t max_coord,
                                            size_t num_clusters, uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<uint32_t> center_dist(0, max_coord);

    std::vector<Point3D> centers;
    for (size_t i = 0; i < num_clusters; ++i) {
        centers.push_back({center_dist(rng), center_dist(rng), center_dist(rng)});
    }

    std::vector<Point3D> points;
    points.reserve(n);
    std::uniform_int_distribution<size_t> cluster_dist(0, num_clusters - 1);
    std::normal_distribution<double> offset_dist(0, max_coord / 20.0);

    for (size_t i = 0; i < n; ++i) {
        auto& center = centers[cluster_dist(rng)];
        int32_t ox = static_cast<int32_t>(offset_dist(rng));
        int32_t oy = static_cast<int32_t>(offset_dist(rng));
        int32_t oz = static_cast<int32_t>(offset_dist(rng));
        uint32_t x = static_cast<uint32_t>(std::clamp<int32_t>(center.x + ox, 0, static_cast<int32_t>(max_coord)));
        uint32_t y = static_cast<uint32_t>(std::clamp<int32_t>(center.y + oy, 0, static_cast<int32_t>(max_coord)));
        uint32_t z = static_cast<uint32_t>(std::clamp<int32_t>(center.z + oz, 0, static_cast<int32_t>(max_coord)));
        points.push_back({x, y, z});
    }
    return points;
}

// ============================================================================
// Unified Data Container
// ============================================================================

struct DataPoints {
    std::vector<Point2D> points_2d;
    std::vector<Point3D> points_3d;
    unsigned dimensions = 2;
    std::string source = "synthetic";  // "uniform", "clustered", or filename

    size_t size() const {
        return dimensions == 2 ? points_2d.size() : points_3d.size();
    }

    void clear() {
        points_2d.clear();
        points_3d.clear();
    }
};

DataPoints generate_synthetic(size_t n, unsigned dims, const std::string& distribution,
                               unsigned coord_bits, size_t num_clusters, uint64_t seed) {
    DataPoints data;
    data.dimensions = dims;
    data.source = distribution;

    uint32_t max_coord = (1U << coord_bits) - 1;

    if (dims == 2) {
        if (distribution == "clustered") {
            data.points_2d = generate_clustered_2d(n, max_coord, num_clusters, seed);
        } else {
            data.points_2d = generate_uniform_2d(n, max_coord, seed);
        }
    } else {
        if (distribution == "clustered") {
            data.points_3d = generate_clustered_3d(n, max_coord, num_clusters, seed);
        } else {
            data.points_3d = generate_uniform_3d(n, max_coord, seed);
        }
    }

    return data;
}

// Load HDF5 dataset (expects 'coordinates' or 'points' dataset with Nx2 or Nx3 array)
DataPoints load_hdf5_dataset(const std::string& path, unsigned coord_bits) {
    DataPoints data;
    uint32_t max_coord = (1U << coord_bits) - 1;

    try {
        HighFive::File file(path, HighFive::File::ReadOnly);

        // Try different dataset names
        std::vector<std::string> dataset_names = {"coords", "coordinates", "points", "data"};
        std::string found_name;
        for (const auto& name : dataset_names) {
            if (file.exist(name)) {
                found_name = name;
                break;
            }
        }

        if (found_name.empty()) {
            throw std::runtime_error("No 'coords', 'coordinates', 'points', or 'data' dataset found in HDF5 file");
        }

        auto dataset = file.getDataSet(found_name);
        auto dims = dataset.getDimensions();

        if (dims.size() != 2) {
            throw std::runtime_error("Dataset must be 2D (Nx2 or Nx3)");
        }

        data.dimensions = static_cast<unsigned>(dims[1]);

        if (data.dimensions == 2) {
            std::vector<std::vector<double>> raw_data;
            dataset.read(raw_data);
            data.points_2d.reserve(raw_data.size());

            for (const auto& row : raw_data) {
                // Normalize to coord range
                uint32_t x = static_cast<uint32_t>(std::clamp<double>(row[0], 0, max_coord));
                uint32_t y = static_cast<uint32_t>(std::clamp<double>(row[1], 0, max_coord));
                data.points_2d.push_back({x, y});
            }
        } else if (data.dimensions == 3) {
            std::vector<std::vector<double>> raw_data;
            dataset.read(raw_data);
            data.points_3d.reserve(raw_data.size());

            for (const auto& row : raw_data) {
                uint32_t x = static_cast<uint32_t>(std::clamp<double>(row[0], 0, max_coord));
                uint32_t y = static_cast<uint32_t>(std::clamp<double>(row[1], 0, max_coord));
                uint32_t z = static_cast<uint32_t>(std::clamp<double>(row[2], 0, max_coord));
                data.points_3d.push_back({x, y, z});
            }
        } else {
            throw std::runtime_error("Dataset must have 2 or 3 columns");
        }

        // Extract filename for source
        size_t pos = path.find_last_of("/\\");
        data.source = (pos != std::string::npos) ? path.substr(pos + 1) : path;

    } catch (const HighFive::Exception& e) {
        throw std::runtime_error("Failed to read HDF5 file: " + std::string(e.what()));
    }

    return data;
}

// Load CSV dataset (expects x,y or x,y,z columns)
DataPoints load_csv_dataset(const std::string& path, unsigned coord_bits) {
    DataPoints data;
    uint32_t max_coord = (1U << coord_bits) - 1;

    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open CSV file: " + path);
    }

    std::string line;

    // Read header line to determine dimensionality
    if (!std::getline(file, line)) {
        throw std::runtime_error("Empty CSV file");
    }

    auto headers = split_string(line, ',');
    data.dimensions = static_cast<unsigned>(headers.size());

    if (data.dimensions < 2 || data.dimensions > 3) {
        throw std::runtime_error("CSV must have 2 or 3 columns");
    }

    // Read data rows
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        auto tokens = split_string(line, ',');

        if (tokens.size() < data.dimensions) continue;

        if (data.dimensions == 2) {
            uint32_t x = static_cast<uint32_t>(std::clamp<double>(std::stod(tokens[0]), 0, max_coord));
            uint32_t y = static_cast<uint32_t>(std::clamp<double>(std::stod(tokens[1]), 0, max_coord));
            data.points_2d.push_back({x, y});
        } else {
            uint32_t x = static_cast<uint32_t>(std::clamp<double>(std::stod(tokens[0]), 0, max_coord));
            uint32_t y = static_cast<uint32_t>(std::clamp<double>(std::stod(tokens[1]), 0, max_coord));
            uint32_t z = static_cast<uint32_t>(std::clamp<double>(std::stod(tokens[2]), 0, max_coord));
            data.points_3d.push_back({x, y, z});
        }
    }

    // Extract filename for source
    size_t pos = path.find_last_of("/\\");
    data.source = (pos != std::string::npos) ? path.substr(pos + 1) : path;

    return data;
}

// Load dataset from path (auto-detect format)
DataPoints load_dataset(const std::string& path, unsigned coord_bits) {
    if (path.size() >= 3 && path.substr(path.size() - 3) == ".h5") {
        return load_hdf5_dataset(path, coord_bits);
    } else if (path.size() >= 4 && path.substr(path.size() - 4) == ".hdf5") {
        return load_hdf5_dataset(path, coord_bits);
    } else if (path.size() >= 4 && path.substr(path.size() - 4) == ".csv") {
        return load_csv_dataset(path, coord_bits);
    } else {
        // Try HDF5 first, then CSV
        try {
            return load_hdf5_dataset(path, coord_bits);
        } catch (...) {
            return load_csv_dataset(path, coord_bits);
        }
    }
}

// ============================================================================
// Benchmark Functions
// ============================================================================

// Result structure for summary table and JSON output
struct BenchResult {
    // Identity
    std::string name;

    // Configuration metadata
    std::string filter_type;      // "sbbf", "blocked_bf", "register_bf"
    std::string sfc_type;         // "morton", "hilbert", "none"
    unsigned dimensions = 2;
    unsigned coord_bits = 16;
    unsigned log_blocks = 17;
    unsigned hash_k = 4;
    std::string strategy;         // "double_hash", "pattern_lookup"
    std::string distribution;     // "uniform", "clustered", or filename
    std::string dataset;          // null or filename
    size_t num_elements = 0;

    // Performance metrics
    double insert_ns = 0;
    double query_ns = 0;
    double neighbor_ns = 0;
    uint64_t insert_ins = 0;
    uint64_t insert_cyc = 0;
    uint64_t query_ins = 0;
    uint64_t query_cyc = 0;
    uint64_t neighbor_ins = 0;
    uint64_t neighbor_cyc = 0;
    double fpr = 0;
    uint64_t memory = 0;

    // Convert to JSON
    json to_json() const {
        json j;
        j["config"] = {
            {"name", name},
            {"filter_type", filter_type},
            {"sfc_type", sfc_type},
            {"dimensions", dimensions},
            {"coord_bits", coord_bits},
            {"log_blocks", log_blocks},
            {"hash_k", hash_k},
            {"strategy", strategy},
            {"distribution", distribution},
            {"dataset", dataset.empty() ? json(nullptr) : json(dataset)},
            {"num_elements", num_elements}
        };
        j["metrics"] = {
            {"memory_bytes", memory},
            {"insert_ns", insert_ns},
            {"insert_ins", insert_ins},
            {"insert_cyc", insert_cyc},
            {"query_ns", query_ns},
            {"query_ins", query_ins},
            {"query_cyc", query_cyc},
            {"neighbor_ns", neighbor_ns},
            {"neighbor_ins", neighbor_ins},
            {"neighbor_cyc", neighbor_cyc},
            {"fpr", fpr}
        };
        return j;
    }
};

std::vector<BenchResult> g_results;
std::vector<BenchResult> g_sweep_results;  // Results from parameter sweep

template<unsigned SFCBits>
void benchmark_sbbf_2d(ankerl::nanobench::Bench& bench,
                       const std::string& name,
                       SFCType sfc_type,
                       const std::vector<Point2D>& insert_data,
                       const std::vector<Point2D>& query_data,
                       unsigned log_blocks, unsigned hash_k,
                       sbbf::IntraBlockStrategy strategy = sbbf::IntraBlockStrategy::DOUBLE_HASH,
                       sbbf::SeedStrategy seed_strategy = sbbf::SeedStrategy::XOR) {
    // Create filter
    SBBFConfig conf;
    conf.sfc_type = sfc_type;
    conf.sfc_bits = SFCBits;
    conf.log_num_blocks = log_blocks;
    conf.hash_k = hash_k;
    conf.bits_per_block = 64;
    conf.intra_strategy = strategy;
    conf.seed_strategy = seed_strategy;
    if (strategy == sbbf::IntraBlockStrategy::PATTERN_LOOKUP) {
        conf.pattern_table_size = 1024;
    }

    SpatialBlockedBloomFilter64<SFCBits> filter(conf);
    BenchResult result;
    result.name = name;
    result.memory = filter.memory_usage();

    // Insert benchmark
    size_t insert_idx = 0;
    bench.run(name + " insert", [&]() {
        const auto& p = insert_data[insert_idx++ % insert_data.size()];
        filter.put2D(p.x, p.y);
    });
    auto& insert_res = bench.results().back();
    result.insert_ns = insert_res.median(ankerl::nanobench::Result::Measure::elapsed) * 1e9;
    result.insert_ins = static_cast<uint64_t>(insert_res.median(ankerl::nanobench::Result::Measure::instructions));
    result.insert_cyc = static_cast<uint64_t>(insert_res.median(ankerl::nanobench::Result::Measure::cpucycles));

    // Reset and fill for query
    filter.clear();
    for (const auto& p : insert_data) {
        filter.put2D(p.x, p.y);
    }

    // Query benchmark
    size_t query_idx = 0;
    bench.run(name + " query", [&]() {
        const auto& p = insert_data[query_idx++ % insert_data.size()];
        ankerl::nanobench::doNotOptimizeAway(filter.get_bool_2D(p.x, p.y));
    });
    auto& query_res = bench.results().back();
    result.query_ns = query_res.median(ankerl::nanobench::Result::Measure::elapsed) * 1e9;
    result.query_ins = static_cast<uint64_t>(query_res.median(ankerl::nanobench::Result::Measure::instructions));
    result.query_cyc = static_cast<uint64_t>(query_res.median(ankerl::nanobench::Result::Measure::cpucycles));

    // Neighborhood query benchmark (8 neighbors, excluding center)
    size_t nbr_idx = 0;
    bench.batch(8).run(name + " neighbor", [&]() {
        const auto& p = insert_data[nbr_idx++ % insert_data.size()];
        ankerl::nanobench::doNotOptimizeAway(filter.query_neighborhood_2D(p.x, p.y, 1));
    });
    bench.batch(1);
    auto& nbr_res = bench.results().back();
    result.neighbor_ns = nbr_res.median(ankerl::nanobench::Result::Measure::elapsed) * 1e9;
    result.neighbor_ins = static_cast<uint64_t>(nbr_res.median(ankerl::nanobench::Result::Measure::instructions)) / 8;
    result.neighbor_cyc = static_cast<uint64_t>(nbr_res.median(ankerl::nanobench::Result::Measure::cpucycles)) / 8;

    // FPR measurement
    size_t false_positives = 0;
    for (const auto& p : query_data) {
        if (filter.get_bool_2D(p.x, p.y)) ++false_positives;
    }
    result.fpr = static_cast<double>(false_positives) / query_data.size();

    g_results.push_back(result);
}

template<unsigned SFCBits>
void benchmark_sbbf_3d(ankerl::nanobench::Bench& bench,
                       const std::string& name,
                       SFCType sfc_type,
                       const std::vector<Point3D>& insert_data,
                       const std::vector<Point3D>& query_data,
                       unsigned log_blocks, unsigned hash_k,
                       sbbf::IntraBlockStrategy strategy = sbbf::IntraBlockStrategy::DOUBLE_HASH) {
    SBBFConfig conf;
    conf.sfc_type = sfc_type;
    conf.sfc_bits = SFCBits;
    conf.log_num_blocks = log_blocks;
    conf.hash_k = hash_k;
    conf.bits_per_block = 64;
    conf.intra_strategy = strategy;
    if (strategy == sbbf::IntraBlockStrategy::PATTERN_LOOKUP) {
        conf.pattern_table_size = 1024;
    }

    SpatialBlockedBloomFilter64<SFCBits> filter(conf);
    BenchResult result;
    result.name = name;
    result.memory = filter.memory_usage();

    // Insert benchmark
    size_t insert_idx = 0;
    bench.run(name + " insert", [&]() {
        const auto& p = insert_data[insert_idx++ % insert_data.size()];
        filter.put3D(p.x, p.y, p.z);
    });
    auto& insert_res = bench.results().back();
    result.insert_ns = insert_res.median(ankerl::nanobench::Result::Measure::elapsed) * 1e9;
    result.insert_ins = static_cast<uint64_t>(insert_res.median(ankerl::nanobench::Result::Measure::instructions));
    result.insert_cyc = static_cast<uint64_t>(insert_res.median(ankerl::nanobench::Result::Measure::cpucycles));

    // Reset and fill
    filter.clear();
    for (const auto& p : insert_data) {
        filter.put3D(p.x, p.y, p.z);
    }

    // Query benchmark
    size_t query_idx = 0;
    bench.run(name + " query", [&]() {
        const auto& p = insert_data[query_idx++ % insert_data.size()];
        ankerl::nanobench::doNotOptimizeAway(filter.get_bool_3D(p.x, p.y, p.z));
    });
    auto& query_res = bench.results().back();
    result.query_ns = query_res.median(ankerl::nanobench::Result::Measure::elapsed) * 1e9;
    result.query_ins = static_cast<uint64_t>(query_res.median(ankerl::nanobench::Result::Measure::instructions));
    result.query_cyc = static_cast<uint64_t>(query_res.median(ankerl::nanobench::Result::Measure::cpucycles));

    // Neighborhood query (26-connected for batch optimization with Hilbert3D)
    size_t nbr_idx = 0;
    bench.batch(26).run(name + " neighbor", [&]() {
        const auto& p = insert_data[nbr_idx++ % insert_data.size()];
        ankerl::nanobench::doNotOptimizeAway(filter.query_neighborhood_3D(p.x, p.y, p.z, true));
    });
    bench.batch(1);
    auto& nbr_res = bench.results().back();
    result.neighbor_ns = nbr_res.median(ankerl::nanobench::Result::Measure::elapsed) * 1e9;
    result.neighbor_ins = static_cast<uint64_t>(nbr_res.median(ankerl::nanobench::Result::Measure::instructions)) / 26;
    result.neighbor_cyc = static_cast<uint64_t>(nbr_res.median(ankerl::nanobench::Result::Measure::cpucycles)) / 26;

    // FPR
    size_t false_positives = 0;
    for (const auto& p : query_data) {
        if (filter.get_bool_3D(p.x, p.y, p.z)) ++false_positives;
    }
    result.fpr = static_cast<double>(false_positives) / query_data.size();

    g_results.push_back(result);
}

template<typename Hasher>
void benchmark_blocked_bf_2d(ankerl::nanobench::Bench& bench,
                              const std::string& name,
                              const std::vector<Point2D>& insert_data,
                              const std::vector<Point2D>& query_data,
                              size_t memory_bytes, double target_fpr,
                              globimap::IntraBlockStrategy strategy) {
    size_t expected_items = compute_expected_items(memory_bytes, target_fpr);
    BlockedBFConfig conf;
    conf.expected_items = expected_items;
    conf.false_positive_rate = target_fpr;
    conf.intra_strategy = strategy;
    conf.pattern_table_size = 1024;

    BlockedBloomFilter<Hasher> filter(conf);
    BenchResult result;
    result.name = name;
    result.memory = filter.memory_usage();

    // Insert
    size_t insert_idx = 0;
    bench.run(name + " insert", [&]() {
        const auto& p = insert_data[insert_idx++ % insert_data.size()];
        filter.put({static_cast<uint64_t>(p.x), static_cast<uint64_t>(p.y)});
    });
    auto& insert_res = bench.results().back();
    result.insert_ns = insert_res.median(ankerl::nanobench::Result::Measure::elapsed) * 1e9;
    result.insert_ins = static_cast<uint64_t>(insert_res.median(ankerl::nanobench::Result::Measure::instructions));
    result.insert_cyc = static_cast<uint64_t>(insert_res.median(ankerl::nanobench::Result::Measure::cpucycles));

    // Reset and fill
    filter.clear();
    for (const auto& p : insert_data) {
        filter.put({static_cast<uint64_t>(p.x), static_cast<uint64_t>(p.y)});
    }

    // Query
    size_t query_idx = 0;
    bench.run(name + " query", [&]() {
        const auto& p = insert_data[query_idx++ % insert_data.size()];
        ankerl::nanobench::doNotOptimizeAway(
            filter.get_bool({static_cast<uint64_t>(p.x), static_cast<uint64_t>(p.y)}));
    });
    auto& query_res = bench.results().back();
    result.query_ns = query_res.median(ankerl::nanobench::Result::Measure::elapsed) * 1e9;
    result.query_ins = static_cast<uint64_t>(query_res.median(ankerl::nanobench::Result::Measure::instructions));
    result.query_cyc = static_cast<uint64_t>(query_res.median(ankerl::nanobench::Result::Measure::cpucycles));

    // Neighborhood (8 neighbors, excluding center)
    size_t nbr_idx = 0;
    bench.batch(8).run(name + " neighbor", [&]() {
        const auto& p = insert_data[nbr_idx++ % insert_data.size()];
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) continue;
                ankerl::nanobench::doNotOptimizeAway(
                    filter.get_bool({static_cast<uint64_t>(p.x + dx),
                                     static_cast<uint64_t>(p.y + dy)}));
            }
        }
    });
    bench.batch(1);
    auto& nbr_res = bench.results().back();
    result.neighbor_ns = nbr_res.median(ankerl::nanobench::Result::Measure::elapsed) * 1e9;
    result.neighbor_ins = static_cast<uint64_t>(nbr_res.median(ankerl::nanobench::Result::Measure::instructions)) / 8;
    result.neighbor_cyc = static_cast<uint64_t>(nbr_res.median(ankerl::nanobench::Result::Measure::cpucycles)) / 8;

    // FPR
    size_t false_positives = 0;
    for (const auto& p : query_data) {
        if (filter.get_bool({static_cast<uint64_t>(p.x), static_cast<uint64_t>(p.y)}))
            ++false_positives;
    }
    result.fpr = static_cast<double>(false_positives) / query_data.size();

    g_results.push_back(result);
}

template<typename Hasher>
void benchmark_register_bf_2d(ankerl::nanobench::Bench& bench,
                               const std::string& name,
                               const std::vector<Point2D>& insert_data,
                               const std::vector<Point2D>& query_data,
                               size_t memory_bytes, double target_fpr,
                               globimap::IntraBlockStrategy strategy) {
    size_t expected_items = compute_expected_items(memory_bytes, target_fpr);
    RegisterBlockedBFConfig conf;
    conf.expected_items = expected_items;
    conf.false_positive_rate = target_fpr;
    conf.compensation = 0;
    conf.intra_strategy = strategy;
    conf.pattern_table_size = 1024;

    RegisterBlockedBloomFilter<0, Hasher> filter(conf);
    BenchResult result;
    result.name = name;
    result.memory = filter.memory_usage();

    // Insert
    size_t insert_idx = 0;
    bench.run(name + " insert", [&]() {
        const auto& p = insert_data[insert_idx++ % insert_data.size()];
        filter.put({static_cast<uint64_t>(p.x), static_cast<uint64_t>(p.y)});
    });
    auto& insert_res = bench.results().back();
    result.insert_ns = insert_res.median(ankerl::nanobench::Result::Measure::elapsed) * 1e9;
    result.insert_ins = static_cast<uint64_t>(insert_res.median(ankerl::nanobench::Result::Measure::instructions));
    result.insert_cyc = static_cast<uint64_t>(insert_res.median(ankerl::nanobench::Result::Measure::cpucycles));

    // Reset and fill
    filter.clear();
    for (const auto& p : insert_data) {
        filter.put({static_cast<uint64_t>(p.x), static_cast<uint64_t>(p.y)});
    }

    // Query
    size_t query_idx = 0;
    bench.run(name + " query", [&]() {
        const auto& p = insert_data[query_idx++ % insert_data.size()];
        ankerl::nanobench::doNotOptimizeAway(
            filter.get_bool({static_cast<uint64_t>(p.x), static_cast<uint64_t>(p.y)}));
    });
    auto& query_res = bench.results().back();
    result.query_ns = query_res.median(ankerl::nanobench::Result::Measure::elapsed) * 1e9;
    result.query_ins = static_cast<uint64_t>(query_res.median(ankerl::nanobench::Result::Measure::instructions));
    result.query_cyc = static_cast<uint64_t>(query_res.median(ankerl::nanobench::Result::Measure::cpucycles));

    // Neighborhood (8 neighbors, excluding center)
    size_t nbr_idx = 0;
    bench.batch(8).run(name + " neighbor", [&]() {
        const auto& p = insert_data[nbr_idx++ % insert_data.size()];
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) continue;
                ankerl::nanobench::doNotOptimizeAway(
                    filter.get_bool({static_cast<uint64_t>(p.x + dx),
                                     static_cast<uint64_t>(p.y + dy)}));
            }
        }
    });
    bench.batch(1);
    auto& nbr_res = bench.results().back();
    result.neighbor_ns = nbr_res.median(ankerl::nanobench::Result::Measure::elapsed) * 1e9;
    result.neighbor_ins = static_cast<uint64_t>(nbr_res.median(ankerl::nanobench::Result::Measure::instructions)) / 8;
    result.neighbor_cyc = static_cast<uint64_t>(nbr_res.median(ankerl::nanobench::Result::Measure::cpucycles)) / 8;

    // FPR
    size_t false_positives = 0;
    for (const auto& p : query_data) {
        if (filter.get_bool({static_cast<uint64_t>(p.x), static_cast<uint64_t>(p.y)}))
            ++false_positives;
    }
    result.fpr = static_cast<double>(false_positives) / query_data.size();

    g_results.push_back(result);
}

template<typename Hasher>
void benchmark_blocked_bf_3d(ankerl::nanobench::Bench& bench,
                              const std::string& name,
                              const std::vector<Point3D>& insert_data,
                              const std::vector<Point3D>& query_data,
                              size_t memory_bytes, double target_fpr,
                              globimap::IntraBlockStrategy strategy) {
    size_t expected_items = compute_expected_items(memory_bytes, target_fpr);
    BlockedBFConfig conf;
    conf.expected_items = expected_items;
    conf.false_positive_rate = target_fpr;
    conf.intra_strategy = strategy;
    conf.pattern_table_size = 1024;

    BlockedBloomFilter<Hasher> filter(conf);
    BenchResult result;
    result.name = name;
    result.memory = filter.memory_usage();

    // Insert
    size_t insert_idx = 0;
    bench.run(name + " insert", [&]() {
        const auto& p = insert_data[insert_idx++ % insert_data.size()];
        filter.put({static_cast<uint64_t>(p.x), static_cast<uint64_t>(p.y),
                    static_cast<uint64_t>(p.z)});
    });
    auto& insert_res = bench.results().back();
    result.insert_ns = insert_res.median(ankerl::nanobench::Result::Measure::elapsed) * 1e9;
    result.insert_ins = static_cast<uint64_t>(insert_res.median(ankerl::nanobench::Result::Measure::instructions));
    result.insert_cyc = static_cast<uint64_t>(insert_res.median(ankerl::nanobench::Result::Measure::cpucycles));

    // Reset and fill
    filter.clear();
    for (const auto& p : insert_data) {
        filter.put({static_cast<uint64_t>(p.x), static_cast<uint64_t>(p.y),
                    static_cast<uint64_t>(p.z)});
    }

    // Query
    size_t query_idx = 0;
    bench.run(name + " query", [&]() {
        const auto& p = insert_data[query_idx++ % insert_data.size()];
        ankerl::nanobench::doNotOptimizeAway(
            filter.get_bool({static_cast<uint64_t>(p.x), static_cast<uint64_t>(p.y),
                             static_cast<uint64_t>(p.z)}));
    });
    auto& query_res = bench.results().back();
    result.query_ns = query_res.median(ankerl::nanobench::Result::Measure::elapsed) * 1e9;
    result.query_ins = static_cast<uint64_t>(query_res.median(ankerl::nanobench::Result::Measure::instructions));
    result.query_cyc = static_cast<uint64_t>(query_res.median(ankerl::nanobench::Result::Measure::cpucycles));

    // Neighborhood (3x3x3 - 1 = 26 neighbors, excluding center)
    size_t nbr_idx = 0;
    bench.batch(26).run(name + " neighbor", [&]() {
        const auto& p = insert_data[nbr_idx++ % insert_data.size()];
        for (int dz = -1; dz <= 1; ++dz) {
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0 && dz == 0) continue;
                    ankerl::nanobench::doNotOptimizeAway(
                        filter.get_bool({static_cast<uint64_t>(p.x + dx),
                                         static_cast<uint64_t>(p.y + dy),
                                         static_cast<uint64_t>(p.z + dz)}));
                }
            }
        }
    });
    bench.batch(1);
    auto& nbr_res = bench.results().back();
    result.neighbor_ns = nbr_res.median(ankerl::nanobench::Result::Measure::elapsed) * 1e9;
    result.neighbor_ins = static_cast<uint64_t>(nbr_res.median(ankerl::nanobench::Result::Measure::instructions)) / 26;
    result.neighbor_cyc = static_cast<uint64_t>(nbr_res.median(ankerl::nanobench::Result::Measure::cpucycles)) / 26;

    // FPR
    size_t false_positives = 0;
    for (const auto& p : query_data) {
        if (filter.get_bool({static_cast<uint64_t>(p.x), static_cast<uint64_t>(p.y),
                             static_cast<uint64_t>(p.z)}))
            ++false_positives;
    }
    result.fpr = static_cast<double>(false_positives) / query_data.size();

    g_results.push_back(result);
}

template<typename Hasher>
void benchmark_register_bf_3d(ankerl::nanobench::Bench& bench,
                               const std::string& name,
                               const std::vector<Point3D>& insert_data,
                               const std::vector<Point3D>& query_data,
                               size_t memory_bytes, double target_fpr,
                               globimap::IntraBlockStrategy strategy) {
    size_t expected_items = compute_expected_items(memory_bytes, target_fpr);
    RegisterBlockedBFConfig conf;
    conf.expected_items = expected_items;
    conf.false_positive_rate = target_fpr;
    conf.compensation = 0;
    conf.intra_strategy = strategy;
    conf.pattern_table_size = 1024;

    RegisterBlockedBloomFilter<0, Hasher> filter(conf);
    BenchResult result;
    result.name = name;
    result.memory = filter.memory_usage();

    // Insert
    size_t insert_idx = 0;
    bench.run(name + " insert", [&]() {
        const auto& p = insert_data[insert_idx++ % insert_data.size()];
        filter.put({static_cast<uint64_t>(p.x), static_cast<uint64_t>(p.y),
                    static_cast<uint64_t>(p.z)});
    });
    auto& insert_res = bench.results().back();
    result.insert_ns = insert_res.median(ankerl::nanobench::Result::Measure::elapsed) * 1e9;
    result.insert_ins = static_cast<uint64_t>(insert_res.median(ankerl::nanobench::Result::Measure::instructions));
    result.insert_cyc = static_cast<uint64_t>(insert_res.median(ankerl::nanobench::Result::Measure::cpucycles));

    // Reset and fill
    filter.clear();
    for (const auto& p : insert_data) {
        filter.put({static_cast<uint64_t>(p.x), static_cast<uint64_t>(p.y),
                    static_cast<uint64_t>(p.z)});
    }

    // Query
    size_t query_idx = 0;
    bench.run(name + " query", [&]() {
        const auto& p = insert_data[query_idx++ % insert_data.size()];
        ankerl::nanobench::doNotOptimizeAway(
            filter.get_bool({static_cast<uint64_t>(p.x), static_cast<uint64_t>(p.y),
                             static_cast<uint64_t>(p.z)}));
    });
    auto& query_res = bench.results().back();
    result.query_ns = query_res.median(ankerl::nanobench::Result::Measure::elapsed) * 1e9;
    result.query_ins = static_cast<uint64_t>(query_res.median(ankerl::nanobench::Result::Measure::instructions));
    result.query_cyc = static_cast<uint64_t>(query_res.median(ankerl::nanobench::Result::Measure::cpucycles));

    // Neighborhood (3x3x3 - 1 = 26 neighbors, excluding center)
    size_t nbr_idx = 0;
    bench.batch(26).run(name + " neighbor", [&]() {
        const auto& p = insert_data[nbr_idx++ % insert_data.size()];
        for (int dz = -1; dz <= 1; ++dz) {
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0 && dz == 0) continue;
                    ankerl::nanobench::doNotOptimizeAway(
                        filter.get_bool({static_cast<uint64_t>(p.x + dx),
                                         static_cast<uint64_t>(p.y + dy),
                                         static_cast<uint64_t>(p.z + dz)}));
                }
            }
        }
    });
    bench.batch(1);
    auto& nbr_res = bench.results().back();
    result.neighbor_ns = nbr_res.median(ankerl::nanobench::Result::Measure::elapsed) * 1e9;
    result.neighbor_ins = static_cast<uint64_t>(nbr_res.median(ankerl::nanobench::Result::Measure::instructions)) / 26;
    result.neighbor_cyc = static_cast<uint64_t>(nbr_res.median(ankerl::nanobench::Result::Measure::cpucycles)) / 26;

    // FPR
    size_t false_positives = 0;
    for (const auto& p : query_data) {
        if (filter.get_bool({static_cast<uint64_t>(p.x), static_cast<uint64_t>(p.y),
                             static_cast<uint64_t>(p.z)}))
            ++false_positives;
    }
    result.fpr = static_cast<double>(false_positives) / query_data.size();

    g_results.push_back(result);
}

// ============================================================================
// Helper Macros for All Hasher/Strategy Permutations
// ============================================================================

// Benchmark all BlockedBF permutations (6 total: 3 hashers x 2 strategies)
#define BENCH_ALL_BLOCKED_2D(bench, insert, query, mem, fpr) \
    benchmark_blocked_bf_2d<MurmurHasher>(bench, "BlockedBF", insert, query, mem, fpr, globimap::IntraBlockStrategy::DOUBLE_HASH); \
    benchmark_blocked_bf_2d<MurmurHasher>(bench, "BlockedBF+Pat", insert, query, mem, fpr, globimap::IntraBlockStrategy::PATTERN_LOOKUP); \
    benchmark_blocked_bf_2d<XXH3Hasher>(bench, "BlockedBF-XXH3", insert, query, mem, fpr, globimap::IntraBlockStrategy::DOUBLE_HASH); \
    benchmark_blocked_bf_2d<XXH3Hasher>(bench, "BlockedBF-XXH3+Pat", insert, query, mem, fpr, globimap::IntraBlockStrategy::PATTERN_LOOKUP); \
    benchmark_blocked_bf_2d<WyHasher>(bench, "BlockedBF-Wy", insert, query, mem, fpr, globimap::IntraBlockStrategy::DOUBLE_HASH); \
    benchmark_blocked_bf_2d<WyHasher>(bench, "BlockedBF-Wy+Pat", insert, query, mem, fpr, globimap::IntraBlockStrategy::PATTERN_LOOKUP);

#define BENCH_ALL_REGISTER_2D(bench, insert, query, mem, fpr) \
    benchmark_register_bf_2d<MurmurHasher>(bench, "RegisterBF", insert, query, mem, fpr, globimap::IntraBlockStrategy::DOUBLE_HASH); \
    benchmark_register_bf_2d<MurmurHasher>(bench, "RegisterBF+Pat", insert, query, mem, fpr, globimap::IntraBlockStrategy::PATTERN_LOOKUP); \
    benchmark_register_bf_2d<XXH3Hasher>(bench, "RegisterBF-XXH3", insert, query, mem, fpr, globimap::IntraBlockStrategy::DOUBLE_HASH); \
    benchmark_register_bf_2d<XXH3Hasher>(bench, "RegisterBF-XXH3+Pat", insert, query, mem, fpr, globimap::IntraBlockStrategy::PATTERN_LOOKUP); \
    benchmark_register_bf_2d<WyHasher>(bench, "RegisterBF-Wy", insert, query, mem, fpr, globimap::IntraBlockStrategy::DOUBLE_HASH); \
    benchmark_register_bf_2d<WyHasher>(bench, "RegisterBF-Wy+Pat", insert, query, mem, fpr, globimap::IntraBlockStrategy::PATTERN_LOOKUP);

#define BENCH_ALL_BLOCKED_3D(bench, insert, query, mem, fpr) \
    benchmark_blocked_bf_3d<MurmurHasher>(bench, "BlockedBF", insert, query, mem, fpr, globimap::IntraBlockStrategy::DOUBLE_HASH); \
    benchmark_blocked_bf_3d<MurmurHasher>(bench, "BlockedBF+Pat", insert, query, mem, fpr, globimap::IntraBlockStrategy::PATTERN_LOOKUP); \
    benchmark_blocked_bf_3d<XXH3Hasher>(bench, "BlockedBF-XXH3", insert, query, mem, fpr, globimap::IntraBlockStrategy::DOUBLE_HASH); \
    benchmark_blocked_bf_3d<XXH3Hasher>(bench, "BlockedBF-XXH3+Pat", insert, query, mem, fpr, globimap::IntraBlockStrategy::PATTERN_LOOKUP); \
    benchmark_blocked_bf_3d<WyHasher>(bench, "BlockedBF-Wy", insert, query, mem, fpr, globimap::IntraBlockStrategy::DOUBLE_HASH); \
    benchmark_blocked_bf_3d<WyHasher>(bench, "BlockedBF-Wy+Pat", insert, query, mem, fpr, globimap::IntraBlockStrategy::PATTERN_LOOKUP);

#define BENCH_ALL_REGISTER_3D(bench, insert, query, mem, fpr) \
    benchmark_register_bf_3d<MurmurHasher>(bench, "RegisterBF", insert, query, mem, fpr, globimap::IntraBlockStrategy::DOUBLE_HASH); \
    benchmark_register_bf_3d<MurmurHasher>(bench, "RegisterBF+Pat", insert, query, mem, fpr, globimap::IntraBlockStrategy::PATTERN_LOOKUP); \
    benchmark_register_bf_3d<XXH3Hasher>(bench, "RegisterBF-XXH3", insert, query, mem, fpr, globimap::IntraBlockStrategy::DOUBLE_HASH); \
    benchmark_register_bf_3d<XXH3Hasher>(bench, "RegisterBF-XXH3+Pat", insert, query, mem, fpr, globimap::IntraBlockStrategy::PATTERN_LOOKUP); \
    benchmark_register_bf_3d<WyHasher>(bench, "RegisterBF-Wy", insert, query, mem, fpr, globimap::IntraBlockStrategy::DOUBLE_HASH); \
    benchmark_register_bf_3d<WyHasher>(bench, "RegisterBF-Wy+Pat", insert, query, mem, fpr, globimap::IntraBlockStrategy::PATTERN_LOOKUP);

// Print summary table
void print_summary_table() {
    if (g_results.empty()) return;

    std::cout << "\n";
    std::cout << std::left << std::setw(20) << "Implementation"
              << std::right
              << std::setw(8) << "Memory"
              << " |"
              << std::setw(7) << "Insert"
              << std::setw(5) << "ins"
              << std::setw(5) << "cyc"
              << " |"
              << std::setw(7) << "Query"
              << std::setw(5) << "ins"
              << std::setw(5) << "cyc"
              << " |"
              << std::setw(8) << "Neighb"
              << std::setw(5) << "ins"
              << std::setw(5) << "cyc"
              << " |"
              << std::setw(7) << "FPR" << "\n";
    std::cout << std::string(106, '-') << "\n";

    for (const auto& r : g_results) {
        std::cout << std::left << std::setw(20) << r.name
                  << std::right
                  << std::setw(6) << (r.memory / 1024) << "KB"
                  << " |"
                  << std::setw(5) << std::fixed << std::setprecision(1) << r.insert_ns << "ns"
                  << std::setw(5) << r.insert_ins
                  << std::setw(5) << r.insert_cyc
                  << " |"
                  << std::setw(5) << r.query_ns << "ns"
                  << std::setw(5) << r.query_ins
                  << std::setw(5) << r.query_cyc
                  << " |"
                  << std::setw(6) << r.neighbor_ns << "ns"
                  << std::setw(5) << r.neighbor_ins
                  << std::setw(5) << r.neighbor_cyc
                  << " |"
                  << std::setw(6) << std::setprecision(2) << (r.fpr * 100) << "%\n";
    }
    std::cout << "\n";
    g_results.clear();
}

// ============================================================================
// Benchmark Scenarios
// ============================================================================

void run_2d_benchmark(size_t n, uint32_t max_coord, unsigned log_blocks) {
    std::cout << "\n========================================\n";
    std::cout << "2D Uniform Benchmark (" << n << " elements)\n";
    std::cout << "========================================\n";

    auto insert_data = generate_uniform_2d(n, max_coord, 42);
    auto query_data = generate_uniform_2d(n, max_coord, 123);

    ankerl::nanobench::Bench bench;
    bench.performanceCounters(true)
         .epochs(g_config.epochs)
         .minEpochIterations(g_config.min_iters)
         .warmup(g_config.warmup)
         .maxEpochTime(std::chrono::milliseconds(g_config.epoch_time_ms))
         .relative(true);

    std::cout << "\nRunning benchmarks with hardware performance counters...\n\n";

    // SBBF Morton 2D - both strategies
    benchmark_sbbf_2d<16>(bench, "SBBF-Morton2D", SFCType::MORTON_2D,
                          insert_data, query_data, log_blocks, 4,
                          sbbf::IntraBlockStrategy::DOUBLE_HASH);
    benchmark_sbbf_2d<16>(bench, "SBBF-Morton2D+Pat", SFCType::MORTON_2D,
                          insert_data, query_data, log_blocks, 4,
                          sbbf::IntraBlockStrategy::PATTERN_LOOKUP);

    // SBBF Hilbert 2D - both strategies
    benchmark_sbbf_2d<16>(bench, "SBBF-Hilbert2D", SFCType::HILBERT_2D,
                          insert_data, query_data, log_blocks, 4,
                          sbbf::IntraBlockStrategy::DOUBLE_HASH);
    benchmark_sbbf_2d<16>(bench, "SBBF-Hilbert2D+Pat", SFCType::HILBERT_2D,
                          insert_data, query_data, log_blocks, 4,
                          sbbf::IntraBlockStrategy::PATTERN_LOOKUP);

    // SBBF with MULTIPLY_SHIFT seed strategy for comparison
    benchmark_sbbf_2d<16>(bench, "SBBF-Morton2D-MS", SFCType::MORTON_2D,
                          insert_data, query_data, log_blocks, 4,
                          sbbf::IntraBlockStrategy::DOUBLE_HASH,
                          sbbf::SeedStrategy::MULTIPLY_SHIFT);
    benchmark_sbbf_2d<16>(bench, "SBBF-Hilbert2D-MS", SFCType::HILBERT_2D,
                          insert_data, query_data, log_blocks, 4,
                          sbbf::IntraBlockStrategy::DOUBLE_HASH,
                          sbbf::SeedStrategy::MULTIPLY_SHIFT);

    // Baseline filters (same memory budget as SBBF)
    size_t sbbf_memory = (1ULL << log_blocks) * 8;
    double target_fpr = 0.001;  // 0.1% target to match SBBF's ~0.07%
    BENCH_ALL_BLOCKED_2D(bench, insert_data, query_data, sbbf_memory, target_fpr);
    BENCH_ALL_REGISTER_2D(bench, insert_data, query_data, sbbf_memory, target_fpr);

    print_summary_table();
}

void run_2d_clustered_benchmark(size_t n, uint32_t max_coord, size_t clusters,
                                 unsigned log_blocks) {
    std::cout << "\n========================================\n";
    std::cout << "2D Clustered Benchmark (" << n << " elements, " << clusters << " clusters)\n";
    std::cout << "========================================\n";

    auto insert_data = generate_clustered_2d(n, max_coord, clusters, 42);
    auto query_data = generate_uniform_2d(n, max_coord, 123);

    ankerl::nanobench::Bench bench;
    bench.performanceCounters(true)
         .epochs(g_config.epochs)
         .minEpochIterations(g_config.min_iters)
         .warmup(g_config.warmup)
         .maxEpochTime(std::chrono::milliseconds(g_config.epoch_time_ms))
         .relative(true);

    std::cout << "\nRunning benchmarks with hardware performance counters...\n\n";

    // SBBF Morton 2D - both strategies
    benchmark_sbbf_2d<16>(bench, "SBBF-Morton2D", SFCType::MORTON_2D,
                          insert_data, query_data, log_blocks, 4,
                          sbbf::IntraBlockStrategy::DOUBLE_HASH);
    benchmark_sbbf_2d<16>(bench, "SBBF-Morton2D+Pat", SFCType::MORTON_2D,
                          insert_data, query_data, log_blocks, 4,
                          sbbf::IntraBlockStrategy::PATTERN_LOOKUP);

    // SBBF Hilbert 2D - both strategies
    benchmark_sbbf_2d<16>(bench, "SBBF-Hilbert2D", SFCType::HILBERT_2D,
                          insert_data, query_data, log_blocks, 4,
                          sbbf::IntraBlockStrategy::DOUBLE_HASH);
    benchmark_sbbf_2d<16>(bench, "SBBF-Hilbert2D+Pat", SFCType::HILBERT_2D,
                          insert_data, query_data, log_blocks, 4,
                          sbbf::IntraBlockStrategy::PATTERN_LOOKUP);

    // Baseline filters (same memory budget as SBBF)
    size_t sbbf_memory = (1ULL << log_blocks) * 8;
    double target_fpr = 0.001;
    BENCH_ALL_BLOCKED_2D(bench, insert_data, query_data, sbbf_memory, target_fpr);
    BENCH_ALL_REGISTER_2D(bench, insert_data, query_data, sbbf_memory, target_fpr);

    print_summary_table();
}

void run_3d_benchmark(size_t n, uint32_t max_coord, unsigned log_blocks) {
    std::cout << "\n========================================\n";
    std::cout << "3D Uniform Benchmark (" << n << " elements)\n";
    std::cout << "========================================\n";

    auto insert_data = generate_uniform_3d(n, max_coord, 42);
    auto query_data = generate_uniform_3d(n, max_coord, 123);

    // First, benchmark raw SFC encoding performance
    {
        std::cout << "\n--- 3D Space-Filling Curve Encoding Comparison ---\n\n";
        ankerl::nanobench::Bench bench;
        bench.performanceCounters(true)
             .epochs(g_config.epochs)
             .minEpochIterations(g_config.min_iters)
             .warmup(g_config.warmup)
             .relative(true);

        size_t idx = 0;
        bench.run("Morton3D encode", [&]() {
            const auto& p = insert_data[idx++ % insert_data.size()];
            ankerl::nanobench::doNotOptimizeAway(sfc::Morton3D<10>::encode(p.x, p.y, p.z));
        });

        idx = 0;
        bench.run("Hilbert3D LUT encode", [&]() {
            const auto& p = insert_data[idx++ % insert_data.size()];
            ankerl::nanobench::doNotOptimizeAway(sfc::Hilbert3D<10>::encode(p.x, p.y, p.z));
        });

        idx = 0;
        bench.run("Hilbert3D ref encode", [&]() {
            const auto& p = insert_data[idx++ % insert_data.size()];
            ankerl::nanobench::doNotOptimizeAway(sfc::Hilbert3D<10>::encode_reference(p.x, p.y, p.z));
        });
    }

    // Then, benchmark SBBF operations
    ankerl::nanobench::Bench bench;
    bench.performanceCounters(true)
         .epochs(g_config.epochs)
         .minEpochIterations(g_config.min_iters)
         .warmup(g_config.warmup)
         .maxEpochTime(std::chrono::milliseconds(g_config.epoch_time_ms))
         .relative(true);

    std::cout << "\n--- SBBF Operations ---\n\n";

    // SBBF Morton 3D - both strategies
    benchmark_sbbf_3d<10>(bench, "SBBF-Morton3D", SFCType::MORTON_3D,
                          insert_data, query_data, log_blocks, 4,
                          sbbf::IntraBlockStrategy::DOUBLE_HASH);
    benchmark_sbbf_3d<10>(bench, "SBBF-Morton3D+Pat", SFCType::MORTON_3D,
                          insert_data, query_data, log_blocks, 4,
                          sbbf::IntraBlockStrategy::PATTERN_LOOKUP);

    // SBBF Hilbert 3D - both strategies
    benchmark_sbbf_3d<10>(bench, "SBBF-Hilbert3D", SFCType::HILBERT_3D,
                          insert_data, query_data, log_blocks, 4,
                          sbbf::IntraBlockStrategy::DOUBLE_HASH);
    benchmark_sbbf_3d<10>(bench, "SBBF-Hilbert3D+Pat", SFCType::HILBERT_3D,
                          insert_data, query_data, log_blocks, 4,
                          sbbf::IntraBlockStrategy::PATTERN_LOOKUP);

    std::cout << "\n--- Baseline Bloom Filters (same memory budget) ---\n\n";

    // Match SBBF memory: 2^log_blocks * 64 bits = 2^log_blocks * 8 bytes
    size_t sbbf_memory = (1ULL << log_blocks) * 8;
    // Use same FPR target - actual FPR depends on load factor
    double target_fpr = 0.001;  // 0.1% target to match SBBF's ~0.07%

    BENCH_ALL_BLOCKED_3D(bench, insert_data, query_data, sbbf_memory, target_fpr);
    BENCH_ALL_REGISTER_3D(bench, insert_data, query_data, sbbf_memory, target_fpr);

    print_summary_table();
}

void run_3d_clustered_benchmark(size_t n, uint32_t max_coord, size_t clusters,
                                 unsigned log_blocks) {
    std::cout << "\n========================================\n";
    std::cout << "3D Clustered Benchmark (" << n << " elements, " << clusters << " clusters)\n";
    std::cout << "========================================\n";

    auto insert_data = generate_clustered_3d(n, max_coord, clusters, 42);
    auto query_data = generate_uniform_3d(n, max_coord, 123);

    ankerl::nanobench::Bench bench;
    bench.performanceCounters(true)
         .epochs(g_config.epochs)
         .minEpochIterations(g_config.min_iters)
         .warmup(g_config.warmup)
         .maxEpochTime(std::chrono::milliseconds(g_config.epoch_time_ms))
         .relative(true);

    std::cout << "\nRunning benchmarks with hardware performance counters...\n\n";

    // SBBF Morton 3D - both strategies
    benchmark_sbbf_3d<10>(bench, "SBBF-Morton3D", SFCType::MORTON_3D,
                          insert_data, query_data, log_blocks, 4,
                          sbbf::IntraBlockStrategy::DOUBLE_HASH);
    benchmark_sbbf_3d<10>(bench, "SBBF-Morton3D+Pat", SFCType::MORTON_3D,
                          insert_data, query_data, log_blocks, 4,
                          sbbf::IntraBlockStrategy::PATTERN_LOOKUP);

    // SBBF Hilbert 3D - both strategies
    benchmark_sbbf_3d<10>(bench, "SBBF-Hilbert3D", SFCType::HILBERT_3D,
                          insert_data, query_data, log_blocks, 4,
                          sbbf::IntraBlockStrategy::DOUBLE_HASH);
    benchmark_sbbf_3d<10>(bench, "SBBF-Hilbert3D+Pat", SFCType::HILBERT_3D,
                          insert_data, query_data, log_blocks, 4,
                          sbbf::IntraBlockStrategy::PATTERN_LOOKUP);

    // Baseline filters (same memory budget as SBBF)
    size_t sbbf_memory = (1ULL << log_blocks) * 8;
    double target_fpr = 0.001;
    BENCH_ALL_BLOCKED_3D(bench, insert_data, query_data, sbbf_memory, target_fpr);
    BENCH_ALL_REGISTER_3D(bench, insert_data, query_data, sbbf_memory, target_fpr);

    print_summary_table();
}

// ============================================================================
// Scan Benchmarks (Volume, Raster, Batch)
// ============================================================================

// Volume scan benchmark: query all points in a 3D cubic region
// Compares SFC-ordered scan vs random scan to show cache locality benefit
void run_volume_scan_benchmark(size_t region_size, unsigned log_blocks) {
    std::cout << "\n========================================\n";
    std::cout << "3D Volume Scan Benchmark (" << region_size << "^3 = "
              << (region_size * region_size * region_size) << " queries)\n";
    std::cout << "========================================\n";

    // Generate all points in the cubic region
    std::vector<Point3D> points;
    points.reserve(region_size * region_size * region_size);
    for (uint32_t z = 0; z < region_size; ++z) {
        for (uint32_t y = 0; y < region_size; ++y) {
            for (uint32_t x = 0; x < region_size; ++x) {
                points.push_back({x, y, z});
            }
        }
    }

    // Create SFC-sorted version (Morton order) - use 12 bits (multiple of 4 for Hilbert)
    auto morton_sorted = points;
    std::sort(morton_sorted.begin(), morton_sorted.end(),
              [](const Point3D& a, const Point3D& b) {
                  return sfc::Morton3D<12>::encode(a.x, a.y, a.z) <
                         sfc::Morton3D<12>::encode(b.x, b.y, b.z);
              });

    // Create Hilbert-sorted version
    auto hilbert_sorted = points;
    std::sort(hilbert_sorted.begin(), hilbert_sorted.end(),
              [](const Point3D& a, const Point3D& b) {
                  return sfc::Hilbert3D<12>::encode(a.x, a.y, a.z) <
                         sfc::Hilbert3D<12>::encode(b.x, b.y, b.z);
              });

    // Create random order
    auto random_order = points;
    std::mt19937_64 rng(42);
    std::shuffle(random_order.begin(), random_order.end(), rng);

    // SBBF Morton (use 12 bits for Hilbert compatibility)
    SBBFConfig sbbf_conf;
    sbbf_conf.log_num_blocks = log_blocks;
    sbbf_conf.hash_k = 4;
    sbbf_conf.sfc_type = sbbf::SFCType::MORTON_3D;
    sbbf_conf.intra_strategy = sbbf::IntraBlockStrategy::PATTERN_LOOKUP;
    sbbf_conf.pattern_table_size = 1024;
    SpatialBlockedBloomFilter64<12> sbbf_morton(sbbf_conf);

    // SBBF Hilbert
    sbbf_conf.sfc_type = sbbf::SFCType::HILBERT_3D;
    SpatialBlockedBloomFilter64<12> sbbf_hilbert(sbbf_conf);

    // Insert data using put3D
    for (const auto& p : points) {
        sbbf_morton.put3D(p.x, p.y, p.z);
        sbbf_hilbert.put3D(p.x, p.y, p.z);
    }

    ankerl::nanobench::Bench bench;
    bench.performanceCounters(true)
         .epochs(g_config.epochs)
         .minEpochIterations(5)  // Fewer iterations since each run queries many points
         .warmup(2)
         .relative(true);

    std::cout << "\n--- Volume Scan Comparison ---\n\n";

    // SBBF Morton - SFC ordered scan
    bench.run("SBBF-Morton SFC-scan", [&]() {
        size_t count = 0;
        for (const auto& p : morton_sorted) {
            if (sbbf_morton.get_bool_3D(p.x, p.y, p.z)) count++;
        }
        ankerl::nanobench::doNotOptimizeAway(count);
    });

    // SBBF Morton - random scan
    bench.run("SBBF-Morton random-scan", [&]() {
        size_t count = 0;
        for (const auto& p : random_order) {
            if (sbbf_morton.get_bool_3D(p.x, p.y, p.z)) count++;
        }
        ankerl::nanobench::doNotOptimizeAway(count);
    });

    // SBBF Hilbert - SFC ordered scan
    bench.run("SBBF-Hilbert SFC-scan", [&]() {
        size_t count = 0;
        for (const auto& p : hilbert_sorted) {
            if (sbbf_hilbert.get_bool_3D(p.x, p.y, p.z)) count++;
        }
        ankerl::nanobench::doNotOptimizeAway(count);
    });

    // SBBF Hilbert - random scan
    bench.run("SBBF-Hilbert random-scan", [&]() {
        size_t count = 0;
        for (const auto& p : random_order) {
            if (sbbf_hilbert.get_bool_3D(p.x, p.y, p.z)) count++;
        }
        ankerl::nanobench::doNotOptimizeAway(count);
    });

    std::cout << "\nKey insight: SFC-ordered scans benefit from cache locality.\n";
    std::cout << "Adjacent SFC keys map to nearby memory addresses.\n";
}

// Raster scan benchmark: query all points in a 2D rectangular region
void run_raster_scan_benchmark(size_t width, size_t height, unsigned log_blocks) {
    std::cout << "\n========================================\n";
    std::cout << "2D Raster Scan Benchmark (" << width << "x" << height << " = "
              << (width * height) << " queries)\n";
    std::cout << "========================================\n";

    // Generate all points in the rectangular region
    std::vector<Point2D> points;
    points.reserve(width * height);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            points.push_back({x, y});
        }
    }

    // Row-major order (already in this order)
    auto row_major = points;

    // SFC-sorted version (Hilbert order)
    auto hilbert_sorted = points;
    std::sort(hilbert_sorted.begin(), hilbert_sorted.end(),
              [](const Point2D& a, const Point2D& b) {
                  return sfc::Hilbert2D<16>::encode(a.x, a.y) <
                         sfc::Hilbert2D<16>::encode(b.x, b.y);
              });

    // Morton-sorted version
    auto morton_sorted = points;
    std::sort(morton_sorted.begin(), morton_sorted.end(),
              [](const Point2D& a, const Point2D& b) {
                  return sfc::Morton2D<16>::encode(a.x, a.y) <
                         sfc::Morton2D<16>::encode(b.x, b.y);
              });

    // Random order
    auto random_order = points;
    std::mt19937_64 rng(42);
    std::shuffle(random_order.begin(), random_order.end(), rng);

    // SBBF Hilbert
    SBBFConfig sbbf_conf;
    sbbf_conf.log_num_blocks = log_blocks;
    sbbf_conf.hash_k = 4;
    sbbf_conf.sfc_type = sbbf::SFCType::HILBERT_2D;
    sbbf_conf.intra_strategy = sbbf::IntraBlockStrategy::PATTERN_LOOKUP;
    sbbf_conf.pattern_table_size = 1024;
    SpatialBlockedBloomFilter64<16> sbbf_hilbert(sbbf_conf);

    // SBBF Morton
    sbbf_conf.sfc_type = sbbf::SFCType::MORTON_2D;
    SpatialBlockedBloomFilter64<16> sbbf_morton(sbbf_conf);

    // Insert data
    for (const auto& p : points) {
        sbbf_hilbert.put2D(p.x, p.y);
        sbbf_morton.put2D(p.x, p.y);
    }

    ankerl::nanobench::Bench bench;
    bench.performanceCounters(true)
         .epochs(g_config.epochs)
         .minEpochIterations(5)
         .warmup(2)
         .relative(true);

    std::cout << "\n--- Raster Scan Comparison ---\n\n";

    // SBBF Hilbert - SFC ordered
    bench.run("SBBF-Hilbert SFC-scan", [&]() {
        size_t count = 0;
        for (const auto& p : hilbert_sorted) {
            if (sbbf_hilbert.get_bool_2D(p.x, p.y)) count++;
        }
        ankerl::nanobench::doNotOptimizeAway(count);
    });

    // SBBF Hilbert - row-major
    bench.run("SBBF-Hilbert row-major", [&]() {
        size_t count = 0;
        for (const auto& p : row_major) {
            if (sbbf_hilbert.get_bool_2D(p.x, p.y)) count++;
        }
        ankerl::nanobench::doNotOptimizeAway(count);
    });

    // SBBF Hilbert - random
    bench.run("SBBF-Hilbert random", [&]() {
        size_t count = 0;
        for (const auto& p : random_order) {
            if (sbbf_hilbert.get_bool_2D(p.x, p.y)) count++;
        }
        ankerl::nanobench::doNotOptimizeAway(count);
    });

    // SBBF Morton - SFC ordered
    bench.run("SBBF-Morton SFC-scan", [&]() {
        size_t count = 0;
        for (const auto& p : morton_sorted) {
            if (sbbf_morton.get_bool_2D(p.x, p.y)) count++;
        }
        ankerl::nanobench::doNotOptimizeAway(count);
    });

    // SBBF Morton - row-major
    bench.run("SBBF-Morton row-major", [&]() {
        size_t count = 0;
        for (const auto& p : row_major) {
            if (sbbf_morton.get_bool_2D(p.x, p.y)) count++;
        }
        ankerl::nanobench::doNotOptimizeAway(count);
    });

    // SBBF Morton - random
    bench.run("SBBF-Morton random", [&]() {
        size_t count = 0;
        for (const auto& p : random_order) {
            if (sbbf_morton.get_bool_2D(p.x, p.y)) count++;
        }
        ankerl::nanobench::doNotOptimizeAway(count);
    });

    std::cout << "\nKey insight: SFC-ordered scans should be faster than row-major or random.\n";
}

// Batch query benchmark: query many random points, comparing sorted vs unsorted
void run_batch_query_benchmark(size_t n_queries, size_t n_elements,
                               uint32_t max_coord, unsigned log_blocks) {
    std::cout << "\n========================================\n";
    std::cout << "Batch Query Benchmark (" << n_queries << " queries, "
              << n_elements << " elements)\n";
    std::cout << "========================================\n";

    std::mt19937_64 rng(42);

    // Generate random elements to insert
    std::vector<Point2D> elements;
    elements.reserve(n_elements);
    std::uniform_int_distribution<uint32_t> dist(0, max_coord);
    for (size_t i = 0; i < n_elements; ++i) {
        elements.push_back({dist(rng), dist(rng)});
    }

    // Generate random query points (mix of hits and misses)
    std::vector<Point2D> queries;
    queries.reserve(n_queries);
    for (size_t i = 0; i < n_queries; ++i) {
        queries.push_back({dist(rng), dist(rng)});
    }

    // SFC-sorted queries (by Hilbert key)
    auto hilbert_sorted_queries = queries;
    std::sort(hilbert_sorted_queries.begin(), hilbert_sorted_queries.end(),
              [](const Point2D& a, const Point2D& b) {
                  return sfc::Hilbert2D<16>::encode(a.x, a.y) <
                         sfc::Hilbert2D<16>::encode(b.x, b.y);
              });

    // Morton-sorted queries
    auto morton_sorted_queries = queries;
    std::sort(morton_sorted_queries.begin(), morton_sorted_queries.end(),
              [](const Point2D& a, const Point2D& b) {
                  return sfc::Morton2D<16>::encode(a.x, a.y) <
                         sfc::Morton2D<16>::encode(b.x, b.y);
              });

    // Random order queries (shuffle again to ensure randomness)
    auto random_queries = queries;
    std::shuffle(random_queries.begin(), random_queries.end(), rng);

    // SBBF Hilbert
    SBBFConfig sbbf_conf;
    sbbf_conf.log_num_blocks = log_blocks;
    sbbf_conf.hash_k = 4;
    sbbf_conf.sfc_type = sbbf::SFCType::HILBERT_2D;
    sbbf_conf.intra_strategy = sbbf::IntraBlockStrategy::PATTERN_LOOKUP;
    sbbf_conf.pattern_table_size = 1024;
    SpatialBlockedBloomFilter64<16> sbbf_hilbert(sbbf_conf);

    // SBBF Morton
    sbbf_conf.sfc_type = sbbf::SFCType::MORTON_2D;
    SpatialBlockedBloomFilter64<16> sbbf_morton(sbbf_conf);

    // Insert elements
    for (const auto& p : elements) {
        sbbf_hilbert.put2D(p.x, p.y);
        sbbf_morton.put2D(p.x, p.y);
    }

    ankerl::nanobench::Bench bench;
    bench.performanceCounters(true)
         .epochs(g_config.epochs)
         .minEpochIterations(5)
         .warmup(2)
         .relative(true);

    std::cout << "\n--- Batch Query Comparison ---\n\n";

    // SBBF Hilbert - SFC sorted queries
    bench.run("SBBF-Hilbert sorted", [&]() {
        size_t count = 0;
        for (const auto& p : hilbert_sorted_queries) {
            if (sbbf_hilbert.get_bool_2D(p.x, p.y)) count++;
        }
        ankerl::nanobench::doNotOptimizeAway(count);
    });

    // SBBF Hilbert - random queries
    bench.run("SBBF-Hilbert random", [&]() {
        size_t count = 0;
        for (const auto& p : random_queries) {
            if (sbbf_hilbert.get_bool_2D(p.x, p.y)) count++;
        }
        ankerl::nanobench::doNotOptimizeAway(count);
    });

    // SBBF Morton - SFC sorted queries
    bench.run("SBBF-Morton sorted", [&]() {
        size_t count = 0;
        for (const auto& p : morton_sorted_queries) {
            if (sbbf_morton.get_bool_2D(p.x, p.y)) count++;
        }
        ankerl::nanobench::doNotOptimizeAway(count);
    });

    // SBBF Morton - random queries
    bench.run("SBBF-Morton random", [&]() {
        size_t count = 0;
        for (const auto& p : random_queries) {
            if (sbbf_morton.get_bool_2D(p.x, p.y)) count++;
        }
        ankerl::nanobench::doNotOptimizeAway(count);
    });

    std::cout << "\nKey insight: Sorting queries by SFC key before execution\n";
    std::cout << "can improve cache hit rate for batch operations.\n";
}

// ============================================================================
// Strategy Comparison Benchmark
// ============================================================================

struct StrategyResult {
    std::string strategy;
    unsigned k;
    std::string param;
    double insert_ns;
    double query_ns;
    double neighbor_ns;
    uint64_t query_ins;
    uint64_t query_cyc;
    uint64_t neighbor_ins;
    uint64_t neighbor_cyc;
    double fpr;
    size_t memory;
};

std::vector<StrategyResult> g_strategy_results;

template <unsigned SFCBits>
void benchmark_strategy(ankerl::nanobench::Bench& bench,
                        const std::string& name,
                        SBBFConfig conf,
                        const std::vector<Point3D>& insert_data,
                        const std::vector<Point3D>& query_data) {
    SpatialBlockedBloomFilter64<SFCBits> filter(conf);
    StrategyResult result;
    result.strategy = name;
    result.k = conf.hash_k;
    result.memory = filter.memory_usage();

    // Set param string based on strategy
    if (conf.intra_strategy == sbbf::IntraBlockStrategy::PATTERN_LOOKUP) {
        result.param = std::to_string(conf.pattern_table_size);
    } else if (conf.intra_strategy == sbbf::IntraBlockStrategy::MULTIPLEXED) {
        result.param = "x" + std::to_string(conf.multiplex_count);
    } else {
        result.param = "-";
    }

    std::string bench_name = name + " k=" + std::to_string(conf.hash_k);
    if (result.param != "-") bench_name += " " + result.param;

    // Insert benchmark
    size_t insert_idx = 0;
    bench.run(bench_name + " insert", [&]() {
        const auto& p = insert_data[insert_idx++ % insert_data.size()];
        filter.put3D(p.x, p.y, p.z);
    });
    result.insert_ns = bench.results().back().median(ankerl::nanobench::Result::Measure::elapsed) * 1e9;

    // Reset and fill
    filter.clear();
    for (const auto& p : insert_data) {
        filter.put3D(p.x, p.y, p.z);
    }

    // Query benchmark
    size_t query_idx = 0;
    bench.run(bench_name + " query", [&]() {
        const auto& p = insert_data[query_idx++ % insert_data.size()];
        ankerl::nanobench::doNotOptimizeAway(filter.get_bool_3D(p.x, p.y, p.z));
    });
    auto& query_res = bench.results().back();
    result.query_ns = query_res.median(ankerl::nanobench::Result::Measure::elapsed) * 1e9;
    result.query_ins = static_cast<uint64_t>(query_res.median(ankerl::nanobench::Result::Measure::instructions));
    result.query_cyc = static_cast<uint64_t>(query_res.median(ankerl::nanobench::Result::Measure::cpucycles));

    // Neighbor benchmark (26-connected, per-neighbor time)
    size_t nbr_idx = 0;
    bench.batch(26).run(bench_name + " neighbor", [&]() {
        const auto& p = insert_data[nbr_idx++ % insert_data.size()];
        ankerl::nanobench::doNotOptimizeAway(filter.query_neighborhood_3D(p.x, p.y, p.z, true));
    });
    bench.batch(1);
    auto& nbr_res = bench.results().back();
    result.neighbor_ns = nbr_res.median(ankerl::nanobench::Result::Measure::elapsed) * 1e9;
    result.neighbor_ins = static_cast<uint64_t>(nbr_res.median(ankerl::nanobench::Result::Measure::instructions)) / 26;
    result.neighbor_cyc = static_cast<uint64_t>(nbr_res.median(ankerl::nanobench::Result::Measure::cpucycles)) / 26;

    // FPR measurement
    size_t false_positives = 0;
    for (const auto& p : query_data) {
        if (filter.get_bool_3D(p.x, p.y, p.z)) ++false_positives;
    }
    result.fpr = static_cast<double>(false_positives) / query_data.size();

    g_strategy_results.push_back(result);
}

void print_strategy_table() {
    if (g_strategy_results.empty()) return;

    std::cout << "\n";
    std::cout << std::left << std::setw(18) << "Strategy"
              << std::right
              << std::setw(4) << "k"
              << std::setw(6) << "Param"
              << std::setw(8) << "Query"
              << std::setw(6) << "ins"
              << std::setw(6) << "cyc"
              << std::setw(8) << "Neighb"
              << std::setw(6) << "ins"
              << std::setw(6) << "cyc"
              << std::setw(8) << "FPR" << "\n";
    std::cout << std::string(82, '-') << "\n";

    for (const auto& r : g_strategy_results) {
        std::cout << std::left << std::setw(18) << r.strategy
                  << std::right
                  << std::setw(4) << r.k
                  << std::setw(6) << r.param
                  << std::setw(6) << std::fixed << std::setprecision(1) << r.query_ns << " ns"
                  << std::setw(6) << r.query_ins
                  << std::setw(6) << r.query_cyc
                  << std::setw(6) << r.neighbor_ns << " ns"
                  << std::setw(6) << r.neighbor_ins
                  << std::setw(6) << r.neighbor_cyc
                  << std::setw(7) << std::setprecision(3) << (r.fpr * 100) << "%\n";
    }
    std::cout << "\n";
    g_strategy_results.clear();
}

void run_strategy_comparison(size_t n, uint32_t max_coord, unsigned log_blocks) {
    std::cout << "\n========================================\n";
    std::cout << "Intra-Block Strategy Comparison\n";
    std::cout << "========================================\n";
    std::cout << "Memory: " << ((1ULL << log_blocks) * 8 / 1024) << " KB, Elements: " << n << "\n";

    auto insert_data = generate_uniform_3d(n, max_coord, 42);
    auto query_data = generate_uniform_3d(n, max_coord, 123);

    ankerl::nanobench::Bench bench;
    bench.performanceCounters(true)
         .epochs(g_config.epochs)
         .minEpochIterations(g_config.min_iters)
         .warmup(g_config.warmup)
         .maxEpochTime(std::chrono::milliseconds(g_config.epoch_time_ms))
         .relative(true);

    // Base config
    SBBFConfig base_conf;
    base_conf.sfc_type = SFCType::HILBERT_3D;
    base_conf.sfc_bits = 10;
    base_conf.log_num_blocks = log_blocks;
    base_conf.bits_per_block = 64;

    std::cout << "\n--- DOUBLE_HASH (varying k) ---\n\n";

    for (unsigned k : {2, 4, 6, 8}) {
        SBBFConfig conf = base_conf;
        conf.intra_strategy = sbbf::IntraBlockStrategy::DOUBLE_HASH;
        conf.hash_k = k;
        benchmark_strategy<10>(bench, "DOUBLE_HASH", conf, insert_data, query_data);
    }

    std::cout << "\n--- PATTERN_LOOKUP (k=4, varying table_size) ---\n\n";

    for (size_t table_size : {256, 512, 1024, 2048}) {
        SBBFConfig conf = base_conf;
        conf.intra_strategy = sbbf::IntraBlockStrategy::PATTERN_LOOKUP;
        conf.hash_k = 4;
        conf.pattern_table_size = table_size;
        benchmark_strategy<10>(bench, "PATTERN_LOOKUP", conf, insert_data, query_data);
    }

    std::cout << "\n--- MULTIPLEXED (k=4, varying multiplex_count) ---\n\n";

    for (unsigned mux : {1, 2, 4}) {
        SBBFConfig conf = base_conf;
        conf.intra_strategy = sbbf::IntraBlockStrategy::MULTIPLEXED;
        conf.hash_k = 4;
        conf.multiplex_count = mux;
        benchmark_strategy<10>(bench, "MULTIPLEXED", conf, insert_data, query_data);
    }

    print_strategy_table();
}

// ============================================================================
// Parameter Sweep Runner
// ============================================================================

// Run a single SBBF benchmark and return result with full config metadata
template<unsigned SFCBits>
BenchResult run_sbbf_bench(ankerl::nanobench::Bench& bench,
                           const std::string& sfc_name,
                           SFCType sfc_type,
                           const DataPoints& insert_data,
                           const DataPoints& query_data,
                           unsigned log_blocks,
                           unsigned hash_k,
                           const std::string& strategy,
                           unsigned coord_bits,
                           const std::string& distribution) {
    SBBFConfig conf;
    conf.sfc_type = sfc_type;
    conf.sfc_bits = SFCBits;
    conf.log_num_blocks = log_blocks;
    conf.hash_k = hash_k;
    conf.bits_per_block = 64;

    if (strategy == "pattern_lookup") {
        conf.intra_strategy = sbbf::IntraBlockStrategy::PATTERN_LOOKUP;
        conf.pattern_table_size = 1024;
    } else {
        conf.intra_strategy = sbbf::IntraBlockStrategy::DOUBLE_HASH;
    }

    BenchResult result;
    result.name = "SBBF-" + sfc_name;
    result.filter_type = "sbbf";
    result.sfc_type = sfc_name;
    result.dimensions = insert_data.dimensions;
    result.coord_bits = coord_bits;
    result.log_blocks = log_blocks;
    result.hash_k = hash_k;
    result.strategy = strategy;
    result.distribution = distribution;
    result.dataset = insert_data.source;
    result.num_elements = insert_data.size();

    if (insert_data.dimensions == 2) {
        SpatialBlockedBloomFilter64<SFCBits> filter(conf);
        result.memory = filter.memory_usage();

        const auto& ins = insert_data.points_2d;
        const auto& qry = query_data.points_2d;

        // Insert
        size_t idx = 0;
        bench.run(result.name + " insert", [&]() {
            const auto& p = ins[idx++ % ins.size()];
            filter.put2D(p.x, p.y);
        });
        auto& ins_res = bench.results().back();
        result.insert_ns = ins_res.median(ankerl::nanobench::Result::Measure::elapsed) * 1e9;
        result.insert_ins = static_cast<uint64_t>(ins_res.median(ankerl::nanobench::Result::Measure::instructions));
        result.insert_cyc = static_cast<uint64_t>(ins_res.median(ankerl::nanobench::Result::Measure::cpucycles));

        // Fill
        filter.clear();
        for (const auto& p : ins) filter.put2D(p.x, p.y);

        // Query
        idx = 0;
        bench.run(result.name + " query", [&]() {
            const auto& p = ins[idx++ % ins.size()];
            ankerl::nanobench::doNotOptimizeAway(filter.get_bool_2D(p.x, p.y));
        });
        auto& qry_res = bench.results().back();
        result.query_ns = qry_res.median(ankerl::nanobench::Result::Measure::elapsed) * 1e9;
        result.query_ins = static_cast<uint64_t>(qry_res.median(ankerl::nanobench::Result::Measure::instructions));
        result.query_cyc = static_cast<uint64_t>(qry_res.median(ankerl::nanobench::Result::Measure::cpucycles));

        // Neighbor
        idx = 0;
        bench.batch(8).run(result.name + " neighbor", [&]() {
            const auto& p = ins[idx++ % ins.size()];
            ankerl::nanobench::doNotOptimizeAway(filter.query_neighborhood_2D(p.x, p.y, 1));
        });
        bench.batch(1);
        auto& nbr_res = bench.results().back();
        result.neighbor_ns = nbr_res.median(ankerl::nanobench::Result::Measure::elapsed) * 1e9;
        result.neighbor_ins = static_cast<uint64_t>(nbr_res.median(ankerl::nanobench::Result::Measure::instructions)) / 8;
        result.neighbor_cyc = static_cast<uint64_t>(nbr_res.median(ankerl::nanobench::Result::Measure::cpucycles)) / 8;

        // FPR
        size_t fp = 0;
        for (const auto& p : qry) if (filter.get_bool_2D(p.x, p.y)) ++fp;
        result.fpr = static_cast<double>(fp) / qry.size();
    } else {
        SpatialBlockedBloomFilter64<SFCBits> filter(conf);
        result.memory = filter.memory_usage();

        const auto& ins = insert_data.points_3d;
        const auto& qry = query_data.points_3d;

        // Insert
        size_t idx = 0;
        bench.run(result.name + " insert", [&]() {
            const auto& p = ins[idx++ % ins.size()];
            filter.put3D(p.x, p.y, p.z);
        });
        auto& ins_res = bench.results().back();
        result.insert_ns = ins_res.median(ankerl::nanobench::Result::Measure::elapsed) * 1e9;
        result.insert_ins = static_cast<uint64_t>(ins_res.median(ankerl::nanobench::Result::Measure::instructions));
        result.insert_cyc = static_cast<uint64_t>(ins_res.median(ankerl::nanobench::Result::Measure::cpucycles));

        // Fill
        filter.clear();
        for (const auto& p : ins) filter.put3D(p.x, p.y, p.z);

        // Query
        idx = 0;
        bench.run(result.name + " query", [&]() {
            const auto& p = ins[idx++ % ins.size()];
            ankerl::nanobench::doNotOptimizeAway(filter.get_bool_3D(p.x, p.y, p.z));
        });
        auto& qry_res = bench.results().back();
        result.query_ns = qry_res.median(ankerl::nanobench::Result::Measure::elapsed) * 1e9;
        result.query_ins = static_cast<uint64_t>(qry_res.median(ankerl::nanobench::Result::Measure::instructions));
        result.query_cyc = static_cast<uint64_t>(qry_res.median(ankerl::nanobench::Result::Measure::cpucycles));

        // Neighbor
        idx = 0;
        bench.batch(26).run(result.name + " neighbor", [&]() {
            const auto& p = ins[idx++ % ins.size()];
            ankerl::nanobench::doNotOptimizeAway(filter.query_neighborhood_3D(p.x, p.y, p.z, true));
        });
        bench.batch(1);
        auto& nbr_res = bench.results().back();
        result.neighbor_ns = nbr_res.median(ankerl::nanobench::Result::Measure::elapsed) * 1e9;
        result.neighbor_ins = static_cast<uint64_t>(nbr_res.median(ankerl::nanobench::Result::Measure::instructions)) / 26;
        result.neighbor_cyc = static_cast<uint64_t>(nbr_res.median(ankerl::nanobench::Result::Measure::cpucycles)) / 26;

        // FPR
        size_t fp = 0;
        for (const auto& p : qry) if (filter.get_bool_3D(p.x, p.y, p.z)) ++fp;
        result.fpr = static_cast<double>(fp) / qry.size();
    }

    return result;
}

// Run baseline benchmarks (BlockedBF and RegisterBF with best config)
void run_baselines(ankerl::nanobench::Bench& bench,
                   const DataPoints& insert_data,
                   const DataPoints& query_data,
                   unsigned log_blocks,
                   unsigned coord_bits,
                   const std::string& distribution,
                   const std::string& baseline_mode) {
    if (baseline_mode == "none") return;

    size_t sbbf_memory = (1ULL << log_blocks) * 8;
    double target_fpr = 0.001;

    if (insert_data.dimensions == 2) {
        const auto& ins = insert_data.points_2d;
        const auto& qry = query_data.points_2d;

        if (baseline_mode == "reduced") {
            // Only best performers: WyHash + Pattern Lookup
            benchmark_blocked_bf_2d<WyHasher>(bench, "BlockedBF-Wy+Pat", ins, qry, sbbf_memory, target_fpr, globimap::IntraBlockStrategy::PATTERN_LOOKUP);
            benchmark_register_bf_2d<WyHasher>(bench, "RegisterBF-Wy+Pat", ins, qry, sbbf_memory, target_fpr, globimap::IntraBlockStrategy::PATTERN_LOOKUP);
        } else {
            // Full permutations
            BENCH_ALL_BLOCKED_2D(bench, ins, qry, sbbf_memory, target_fpr);
            BENCH_ALL_REGISTER_2D(bench, ins, qry, sbbf_memory, target_fpr);
        }
    } else {
        const auto& ins = insert_data.points_3d;
        const auto& qry = query_data.points_3d;

        if (baseline_mode == "reduced") {
            benchmark_blocked_bf_3d<WyHasher>(bench, "BlockedBF-Wy+Pat", ins, qry, sbbf_memory, target_fpr, globimap::IntraBlockStrategy::PATTERN_LOOKUP);
            benchmark_register_bf_3d<WyHasher>(bench, "RegisterBF-Wy+Pat", ins, qry, sbbf_memory, target_fpr, globimap::IntraBlockStrategy::PATTERN_LOOKUP);
        } else {
            BENCH_ALL_BLOCKED_3D(bench, ins, qry, sbbf_memory, target_fpr);
            BENCH_ALL_REGISTER_3D(bench, ins, qry, sbbf_memory, target_fpr);
        }
    }

    // Add config metadata to baseline results
    for (auto& r : g_results) {
        r.filter_type = r.name.find("Blocked") != std::string::npos ? "blocked_bf" : "register_bf";
        r.sfc_type = "none";
        r.dimensions = insert_data.dimensions;
        r.coord_bits = coord_bits;
        r.log_blocks = log_blocks;
        r.strategy = r.name.find("+Pat") != std::string::npos ? "pattern_lookup" : "double_hash";
        r.distribution = distribution;
        r.num_elements = insert_data.size();
    }
}

// Run parameter sweep
void run_parameter_sweep(const BenchmarkConfig& config) {
    std::cout << "\n========================================\n";
    std::cout << "Parameter Sweep Benchmark\n";
    std::cout << "========================================\n";

    ankerl::nanobench::Bench bench;
    bench.performanceCounters(true)
         .epochs(config.epochs)
         .minEpochIterations(config.min_iters)
         .warmup(config.warmup)
         .maxEpochTime(std::chrono::milliseconds(config.epoch_time_ms));

    // ========================================
    // DATASET MODE: Load once, use native dims
    // ========================================
    if (!config.dataset_path.empty()) {
        unsigned coord_bits = config.coord_bits_values.empty() ? 16 : config.coord_bits_values[0];
        DataPoints insert_data = load_dataset(config.dataset_path, coord_bits);
        unsigned dims = insert_data.dimensions;  // Use native dimensionality

        // Generate query data with MATCHING dimensionality
        DataPoints query_data = generate_synthetic(insert_data.size(), dims, "uniform", coord_bits, 100, 123);

        std::cout << "Dataset: " << config.dataset_path << "\n";
        std::cout << "  Points: " << insert_data.size() << "\n";
        std::cout << "  Dimensions: " << dims << "D\n";
        std::cout << "  Coord bits: " << coord_bits << "\n\n";

        // Only sweep filter parameters (not data parameters)
        size_t total_configs = config.log_block_sizes.size() * config.hash_k_values.size() *
                               config.sfc_types.size() * config.strategies.size();
        std::cout << "Total configurations: " << total_configs << "\n\n";

        size_t completed = 0;
        std::string dist = "dataset";

        for (unsigned log_blocks : config.log_block_sizes) {
            for (unsigned k : config.hash_k_values) {
                for (const auto& sfc : config.sfc_types) {
                    for (const auto& strategy : config.strategies) {
                        ++completed;
                        std::cout << "\r[" << completed << "/" << total_configs << "] "
                                  << dims << "D " << sfc << " " << strategy
                                  << " n=" << insert_data.size() << " log=" << log_blocks
                                  << " k=" << k << " bits=" << coord_bits
                                  << "        " << std::flush;

                        // Run SBBF benchmark with native dims
                        BenchResult result;
                        if (dims == 2) {
                            if (sfc == "morton") {
                                SFCType sfc_type = SFCType::MORTON_2D;
                                switch (coord_bits) {
                                    case 8:  result = run_sbbf_bench<8>(bench, "Morton2D", sfc_type, insert_data, query_data, log_blocks, k, strategy, coord_bits, dist); break;
                                    case 12: result = run_sbbf_bench<12>(bench, "Morton2D", sfc_type, insert_data, query_data, log_blocks, k, strategy, coord_bits, dist); break;
                                    case 16: result = run_sbbf_bench<16>(bench, "Morton2D", sfc_type, insert_data, query_data, log_blocks, k, strategy, coord_bits, dist); break;
                                    case 20: result = run_sbbf_bench<20>(bench, "Morton2D", sfc_type, insert_data, query_data, log_blocks, k, strategy, coord_bits, dist); break;
                                    default: result = run_sbbf_bench<16>(bench, "Morton2D", sfc_type, insert_data, query_data, log_blocks, k, strategy, coord_bits, dist); break;
                                }
                            } else {
                                SFCType sfc_type = SFCType::HILBERT_2D;
                                switch (coord_bits) {
                                    case 8:  result = run_sbbf_bench<8>(bench, "Hilbert2D", sfc_type, insert_data, query_data, log_blocks, k, strategy, coord_bits, dist); break;
                                    case 12: result = run_sbbf_bench<12>(bench, "Hilbert2D", sfc_type, insert_data, query_data, log_blocks, k, strategy, coord_bits, dist); break;
                                    case 16: result = run_sbbf_bench<16>(bench, "Hilbert2D", sfc_type, insert_data, query_data, log_blocks, k, strategy, coord_bits, dist); break;
                                    case 20: result = run_sbbf_bench<20>(bench, "Hilbert2D", sfc_type, insert_data, query_data, log_blocks, k, strategy, coord_bits, dist); break;
                                    default: result = run_sbbf_bench<16>(bench, "Hilbert2D", sfc_type, insert_data, query_data, log_blocks, k, strategy, coord_bits, dist); break;
                                }
                            }
                        } else {
                            // 3D
                            if (sfc == "morton") {
                                SFCType sfc_type = SFCType::MORTON_3D;
                                switch (coord_bits) {
                                    case 8:  result = run_sbbf_bench<8>(bench, "Morton3D", sfc_type, insert_data, query_data, log_blocks, k, strategy, coord_bits, dist); break;
                                    case 12: result = run_sbbf_bench<12>(bench, "Morton3D", sfc_type, insert_data, query_data, log_blocks, k, strategy, coord_bits, dist); break;
                                    case 16: result = run_sbbf_bench<16>(bench, "Morton3D", sfc_type, insert_data, query_data, log_blocks, k, strategy, coord_bits, dist); break;
                                    case 20: result = run_sbbf_bench<20>(bench, "Morton3D", sfc_type, insert_data, query_data, log_blocks, k, strategy, coord_bits, dist); break;
                                    default: result = run_sbbf_bench<12>(bench, "Morton3D", sfc_type, insert_data, query_data, log_blocks, k, strategy, coord_bits, dist); break;
                                }
                            } else {
                                SFCType sfc_type = SFCType::HILBERT_3D;
                                switch (coord_bits) {
                                    case 8:  result = run_sbbf_bench<8>(bench, "Hilbert3D", sfc_type, insert_data, query_data, log_blocks, k, strategy, coord_bits, dist); break;
                                    case 12: result = run_sbbf_bench<12>(bench, "Hilbert3D", sfc_type, insert_data, query_data, log_blocks, k, strategy, coord_bits, dist); break;
                                    case 16: result = run_sbbf_bench<16>(bench, "Hilbert3D", sfc_type, insert_data, query_data, log_blocks, k, strategy, coord_bits, dist); break;
                                    case 20: result = run_sbbf_bench<20>(bench, "Hilbert3D", sfc_type, insert_data, query_data, log_blocks, k, strategy, coord_bits, dist); break;
                                    default: result = run_sbbf_bench<12>(bench, "Hilbert3D", sfc_type, insert_data, query_data, log_blocks, k, strategy, coord_bits, dist); break;
                                }
                            }
                        }
                        g_sweep_results.push_back(result);
                    }
                }
            }

            // Run baselines once per log_blocks
            if (config.baseline_mode != "none") {
                run_baselines(bench, insert_data, query_data, log_blocks, coord_bits, dist, config.baseline_mode);
                for (auto& r : g_results) {
                    g_sweep_results.push_back(r);
                }
                g_results.clear();
            }
        }

        std::cout << "\n\nSweep complete: " << g_sweep_results.size() << " configurations benchmarked.\n";
        return;
    }

    // ========================================
    // SYNTHETIC MODE: Original loop structure
    // ========================================
    size_t total_configs = config.dimensions.size() * config.distributions.size() *
                           config.element_counts.size() * config.log_block_sizes.size() *
                           config.hash_k_values.size() * config.sfc_types.size() *
                           config.strategies.size() * config.coord_bits_values.size();
    std::cout << "Total configurations: " << total_configs << "\n\n";

    size_t completed = 0;

    for (unsigned dims : config.dimensions) {
        for (const auto& dist : config.distributions) {
            for (size_t elements : config.element_counts) {
                for (unsigned coord_bits : config.coord_bits_values) {
                    // Generate synthetic data
                    DataPoints insert_data = generate_synthetic(elements, dims, dist, coord_bits, config.num_clusters, 42);
                    DataPoints query_data = generate_synthetic(elements, dims, "uniform", coord_bits, 100, 123);

                    for (unsigned log_blocks : config.log_block_sizes) {
                        for (unsigned k : config.hash_k_values) {
                            for (const auto& sfc : config.sfc_types) {
                                for (const auto& strategy : config.strategies) {
                                    ++completed;
                                    std::cout << "\r[" << completed << "/" << total_configs << "] "
                                              << dims << "D " << sfc << " " << strategy
                                              << " n=" << elements << " log=" << log_blocks
                                              << " k=" << k << " bits=" << coord_bits
                                              << "        " << std::flush;

                                    // Run SBBF benchmark
                                    // Note: Hilbert requires bits to be multiple of 4, so we only support 8,12,16,20
                                    BenchResult result;
                                    if (dims == 2) {
                                        if (sfc == "morton") {
                                            SFCType sfc_type = SFCType::MORTON_2D;
                                            switch (coord_bits) {
                                                case 8:  result = run_sbbf_bench<8>(bench, "Morton2D", sfc_type, insert_data, query_data, log_blocks, k, strategy, coord_bits, dist); break;
                                                case 12: result = run_sbbf_bench<12>(bench, "Morton2D", sfc_type, insert_data, query_data, log_blocks, k, strategy, coord_bits, dist); break;
                                                case 16: result = run_sbbf_bench<16>(bench, "Morton2D", sfc_type, insert_data, query_data, log_blocks, k, strategy, coord_bits, dist); break;
                                                case 20: result = run_sbbf_bench<20>(bench, "Morton2D", sfc_type, insert_data, query_data, log_blocks, k, strategy, coord_bits, dist); break;
                                                default: result = run_sbbf_bench<16>(bench, "Morton2D", sfc_type, insert_data, query_data, log_blocks, k, strategy, coord_bits, dist); break;
                                            }
                                        } else {
                                            SFCType sfc_type = SFCType::HILBERT_2D;
                                            switch (coord_bits) {
                                                case 8:  result = run_sbbf_bench<8>(bench, "Hilbert2D", sfc_type, insert_data, query_data, log_blocks, k, strategy, coord_bits, dist); break;
                                                case 12: result = run_sbbf_bench<12>(bench, "Hilbert2D", sfc_type, insert_data, query_data, log_blocks, k, strategy, coord_bits, dist); break;
                                                case 16: result = run_sbbf_bench<16>(bench, "Hilbert2D", sfc_type, insert_data, query_data, log_blocks, k, strategy, coord_bits, dist); break;
                                                case 20: result = run_sbbf_bench<20>(bench, "Hilbert2D", sfc_type, insert_data, query_data, log_blocks, k, strategy, coord_bits, dist); break;
                                                default: result = run_sbbf_bench<16>(bench, "Hilbert2D", sfc_type, insert_data, query_data, log_blocks, k, strategy, coord_bits, dist); break;
                                            }
                                        }
                                    } else {
                                        // 3D uses different SFC bits range (typically 8-10 for 3D due to 3x multiplier)
                                        if (sfc == "morton") {
                                            SFCType sfc_type = SFCType::MORTON_3D;
                                            switch (coord_bits) {
                                                case 8:  result = run_sbbf_bench<8>(bench, "Morton3D", sfc_type, insert_data, query_data, log_blocks, k, strategy, coord_bits, dist); break;
                                                case 12: result = run_sbbf_bench<12>(bench, "Morton3D", sfc_type, insert_data, query_data, log_blocks, k, strategy, coord_bits, dist); break;
                                                case 16: result = run_sbbf_bench<16>(bench, "Morton3D", sfc_type, insert_data, query_data, log_blocks, k, strategy, coord_bits, dist); break;
                                                case 20: result = run_sbbf_bench<20>(bench, "Morton3D", sfc_type, insert_data, query_data, log_blocks, k, strategy, coord_bits, dist); break;
                                                default: result = run_sbbf_bench<12>(bench, "Morton3D", sfc_type, insert_data, query_data, log_blocks, k, strategy, coord_bits, dist); break;
                                            }
                                        } else {
                                            SFCType sfc_type = SFCType::HILBERT_3D;
                                            switch (coord_bits) {
                                                case 8:  result = run_sbbf_bench<8>(bench, "Hilbert3D", sfc_type, insert_data, query_data, log_blocks, k, strategy, coord_bits, dist); break;
                                                case 12: result = run_sbbf_bench<12>(bench, "Hilbert3D", sfc_type, insert_data, query_data, log_blocks, k, strategy, coord_bits, dist); break;
                                                case 16: result = run_sbbf_bench<16>(bench, "Hilbert3D", sfc_type, insert_data, query_data, log_blocks, k, strategy, coord_bits, dist); break;
                                                case 20: result = run_sbbf_bench<20>(bench, "Hilbert3D", sfc_type, insert_data, query_data, log_blocks, k, strategy, coord_bits, dist); break;
                                                default: result = run_sbbf_bench<12>(bench, "Hilbert3D", sfc_type, insert_data, query_data, log_blocks, k, strategy, coord_bits, dist); break;
                                            }
                                        }
                                    }
                                    g_sweep_results.push_back(result);
                                }
                            }
                        }

                        // Run baselines once per (dims, dist, elements, coord_bits, log_blocks) combination
                        if (config.baseline_mode != "none") {
                            run_baselines(bench, insert_data, query_data, log_blocks, coord_bits, dist, config.baseline_mode);
                            for (auto& r : g_results) {
                                g_sweep_results.push_back(r);
                            }
                            g_results.clear();
                        }
                    }
                }
            }
        }
    }

    std::cout << "\n\nSweep complete: " << g_sweep_results.size() << " configurations benchmarked.\n";
}

// ============================================================================
// JSON/CSV Output
// ============================================================================

void save_results_json(const std::vector<BenchResult>& results, const BenchmarkConfig& config, const std::string& path) {
    json output;

    // Metadata
    output["metadata"] = {
        {"timestamp", get_iso_timestamp()},
        {"benchmark_version", "1.0"},
        {"suite", config.suite.empty() ? json(nullptr) : json(config.suite)}
    };

    // Parameters
    output["parameters"] = {
        {"elements", config.element_counts},
        {"log_blocks", config.log_block_sizes},
        {"hash_k", config.hash_k_values},
        {"coord_bits", config.coord_bits_values},
        {"sfc_types", config.sfc_types},
        {"strategies", config.strategies},
        {"distributions", config.distributions},
        {"dimensions", config.dimensions}
    };

    // Results
    output["results"] = json::array();
    for (const auto& r : results) {
        output["results"].push_back(r.to_json());
    }

    // Write to file
    std::ofstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open output file: " << path << "\n";
        return;
    }
    file << output.dump(2);
    file.close();

    std::cout << "Results saved to: " << path << "\n";
}

void save_results_csv(const std::vector<BenchResult>& results, const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open output file: " << path << "\n";
        return;
    }

    // Header
    file << "name,filter_type,sfc_type,dimensions,coord_bits,log_blocks,hash_k,strategy,"
         << "distribution,dataset,num_elements,memory_bytes,"
         << "insert_ns,insert_ins,insert_cyc,"
         << "query_ns,query_ins,query_cyc,"
         << "neighbor_ns,neighbor_ins,neighbor_cyc,fpr\n";

    // Data rows
    for (const auto& r : results) {
        file << r.name << ","
             << r.filter_type << ","
             << r.sfc_type << ","
             << r.dimensions << ","
             << r.coord_bits << ","
             << r.log_blocks << ","
             << r.hash_k << ","
             << r.strategy << ","
             << r.distribution << ","
             << (r.dataset.empty() ? "null" : r.dataset) << ","
             << r.num_elements << ","
             << r.memory << ","
             << std::fixed << std::setprecision(3)
             << r.insert_ns << ","
             << r.insert_ins << ","
             << r.insert_cyc << ","
             << r.query_ns << ","
             << r.query_ins << ","
             << r.query_cyc << ","
             << r.neighbor_ns << ","
             << r.neighbor_ins << ","
             << r.neighbor_cyc << ","
             << std::setprecision(6) << r.fpr << "\n";
    }

    file.close();
    std::cout << "Results saved to: " << path << "\n";
}

std::string get_output_path(const BenchmarkConfig& config) {
    std::string base = config.output_path;

    // If it's a directory, generate filename
    struct stat st;
    if (stat(base.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        std::string prefix = config.suite.empty() ? "sweep" : config.suite;
        std::string ext = config.output_format == "csv" ? ".csv" : ".json";
        return base + "/" + prefix + "_" + get_timestamp() + ext;
    }

    // If it already has extension, use as-is
    if (base.find('.') != std::string::npos) {
        return base;
    }

    // Add extension
    return base + (config.output_format == "csv" ? ".csv" : ".json");
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    g_config = parse_args(argc, argv);

    mkdir(results_path.c_str(), 0755);

    std::cout << "\n";
    std::cout << "=================================================\n";
    std::cout << "  Spatial-Blocked Bloom Filter Benchmark Suite\n";
    std::cout << "  With Hardware Performance Counters\n";
    std::cout << "=================================================\n\n";

    std::cout << "Note: For accurate cycle/instruction counts, run with:\n";
    std::cout << "  sudo sysctl kernel.perf_event_paranoid=1\n\n";

    g_config.print();

    bool run_2d = (g_config.scenario == "all" || g_config.scenario == "2d");
    bool run_3d = (g_config.scenario == "all" || g_config.scenario == "3d");
    bool run_strategy = (g_config.scenario == "strategy");
    bool run_scan = (g_config.scenario == "scan");
    bool run_sweep = (g_config.scenario == "sweep" || !g_config.suite.empty());

    if (run_sweep) {
        // Parameter sweep mode
        run_parameter_sweep(g_config);

        // Save results if output path specified
        if (!g_config.output_path.empty()) {
            std::string output_file = get_output_path(g_config);
            if (g_config.output_format == "csv") {
                save_results_csv(g_sweep_results, output_file);
            } else {
                save_results_json(g_sweep_results, g_config, output_file);
            }
        }
    } else {
        // Use config values (with sensible defaults)
        size_t n = g_config.element_counts.empty() ? 100000 : g_config.element_counts[0];
        unsigned log_blocks = g_config.log_block_sizes.empty() ? 17 : g_config.log_block_sizes[0];
        size_t clusters = g_config.num_clusters;

        // Check which distributions to run
        bool run_uniform = std::find(g_config.distributions.begin(),
                                     g_config.distributions.end(), "uniform") != g_config.distributions.end();
        bool run_clustered = std::find(g_config.distributions.begin(),
                                       g_config.distributions.end(), "clustered") != g_config.distributions.end();

        if (run_2d) {
            // 2D Benchmarks
            uint32_t max_coord_2d = 65535;  // 16-bit coords
            if (run_uniform) {
                run_2d_benchmark(n, max_coord_2d, log_blocks);
            }
            if (run_clustered) {
                run_2d_clustered_benchmark(n, max_coord_2d, clusters, log_blocks);
            }
        }

        if (run_3d) {
            // 3D Benchmarks
            uint32_t max_coord_3d = 1023;  // 10-bit coords
            if (run_uniform) {
                run_3d_benchmark(n, max_coord_3d, log_blocks);
            }
            if (run_clustered) {
                run_3d_clustered_benchmark(n, max_coord_3d, clusters, log_blocks);
            }
        }

        if (run_strategy) {
            // Intra-block strategy comparison
            run_strategy_comparison(n, 1023, log_blocks);
        }

        if (run_scan) {
            // Scan benchmarks (volume, raster, batch query)
            run_volume_scan_benchmark(64, log_blocks);      // 64^3 = 262K queries
            run_raster_scan_benchmark(512, 512, log_blocks); // 512x512 = 262K queries
            run_batch_query_benchmark(100000, n, 65535, log_blocks);  // 100K queries
        }
    }

    std::cout << "\n=================================================\n";
    std::cout << "  Benchmark Complete!\n";
    std::cout << "=================================================\n\n";

    return 0;
}
