/*
 * SBBF Density vs Compression Benchmark
 *
 * Compares SBBF memory efficiency against exact representations:
 * - Dense Voxel Grid: 1 bit per cell, O(1) query
 * - Coordinate List (COO): 64 bits per point, O(log n) query
 *
 * Tests fill rates from 0.001% to 100% on a fixed grid size.
 *
 * Output: JSON file with memory ratios, bits per element, and query times
 */

#include "spatial_blocked_bloom_filter.hpp"
#include "space_filling_curves.hpp"
#include "json.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <sys/stat.h>
#include <unordered_set>
#include <vector>

using json = nlohmann::json;
using namespace sbbf;

// ============================================================================
// Configuration
// ============================================================================

const unsigned GRID_LOG_SIZE = 8;  // 256^3 grid for 3D, 4096^2 for 2D
const unsigned COORD_BITS = 16;    // Must be multiple of 4 for Hilbert
const size_t NUM_QUERIES = 100000;
const unsigned HASH_K = 8;
const std::string OUTPUT_DIR = "./sbbf_results/density/";

// Fill rates to test (from very sparse to fully dense)
const std::vector<double> FILL_RATES = {
    0.00001,  // 0.001%
    0.0001,   // 0.01%
    0.001,    // 0.1%
    0.005,    // 0.5%
    0.01,     // 1%
    0.05,     // 5%
    0.1,      // 10%
    0.2,      // 20%
    0.3,      // 30%
    0.5,      // 50%
    0.7,      // 70%
    0.9,      // 90%
    1.0       // 100%
};

// ============================================================================
// Baseline: Dense Voxel Grid (Bitset)
// ============================================================================

class DenseVoxelGrid {
public:
    DenseVoxelGrid(size_t dim, unsigned dimensions)
        : dim_(dim), dimensions_(dimensions) {
        size_t total_cells = (dimensions == 2) ? dim * dim : dim * dim * dim;
        bits_.resize((total_cells + 63) / 64, 0);
    }

    void insert(uint32_t x, uint32_t y, uint32_t z = 0) {
        size_t idx = (dimensions_ == 2) ? (y * dim_ + x)
                                        : (z * dim_ * dim_ + y * dim_ + x);
        bits_[idx / 64] |= (1ULL << (idx % 64));
    }

    bool query(uint32_t x, uint32_t y, uint32_t z = 0) const {
        size_t idx = (dimensions_ == 2) ? (y * dim_ + x)
                                        : (z * dim_ * dim_ + y * dim_ + x);
        return (bits_[idx / 64] >> (idx % 64)) & 1;
    }

    size_t memory_bytes() const {
        return bits_.size() * sizeof(uint64_t);
    }

    size_t total_cells() const {
        return (dimensions_ == 2) ? dim_ * dim_ : dim_ * dim_ * dim_;
    }

private:
    std::vector<uint64_t> bits_;
    size_t dim_;
    unsigned dimensions_;
};

// ============================================================================
// Baseline: Coordinate List (COO format)
// ============================================================================

class CoordinateList {
public:
    void insert(uint64_t morton_code) {
        coords_.push_back(morton_code);
        sorted_ = false;
    }

    void finalize() {
        std::sort(coords_.begin(), coords_.end());
        coords_.erase(std::unique(coords_.begin(), coords_.end()), coords_.end());
        sorted_ = true;
    }

    bool query(uint64_t morton_code) {
        if (!sorted_) finalize();
        return std::binary_search(coords_.begin(), coords_.end(), morton_code);
    }

    size_t memory_bytes() const {
        return coords_.capacity() * sizeof(uint64_t);
    }

    size_t size() const { return coords_.size(); }

    double bits_per_element() const {
        return coords_.empty() ? 0.0 : (memory_bytes() * 8.0) / coords_.size();
    }

private:
    std::vector<uint64_t> coords_;
    bool sorted_ = false;
};

// ============================================================================
// Data Structures
// ============================================================================

struct Point3D {
    uint32_t x, y, z;
};

struct BenchResult {
    double fill_rate;
    size_t num_elements;
    unsigned dimensions;

    // Memory metrics
    size_t sbbf_memory_bytes;
    size_t dense_memory_bytes;
    size_t sparse_memory_bytes;

    // Derived memory metrics
    double memory_ratio_vs_dense;
    double memory_ratio_vs_sparse;
    double sbbf_bits_per_element;
    double sparse_bits_per_element;
    double dense_bits_per_element;

    // Performance metrics (nanoseconds)
    double sbbf_query_ns;
    double dense_query_ns;
    double sparse_query_ns;
    double query_ratio_vs_dense;
    double query_ratio_vs_sparse;

    // Accuracy
    double fpr;
};

// ============================================================================
// Helper Functions
// ============================================================================

std::string get_iso_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::gmtime(&time);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

void ensure_dir(const std::string& path) {
    mkdir(path.c_str(), 0755);
}

// Compute SBBF log_num_blocks for target bits-per-element
unsigned compute_log_blocks_for_bpe(size_t n, unsigned k, double target_bpe, unsigned bits_per_block = 64) {
    // memory = num_blocks * bits_per_block / 8
    // bpe = memory * 8 / n = num_blocks * bits_per_block / n
    // num_blocks = n * target_bpe / bits_per_block
    double required_blocks = static_cast<double>(n) * target_bpe / bits_per_block;
    unsigned log_blocks = static_cast<unsigned>(std::ceil(std::log2(required_blocks)));
    return std::max(log_blocks, 10u);  // Minimum 2^10 = 1024 blocks
}

// Generate unique random 3D points
std::vector<Point3D> generate_unique_points_3d(size_t n, uint32_t max_coord, uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<uint32_t> dist(0, max_coord);

    std::unordered_set<uint64_t> seen;
    std::vector<Point3D> points;
    points.reserve(n);

    while (points.size() < n) {
        uint32_t x = dist(rng);
        uint32_t y = dist(rng);
        uint32_t z = dist(rng);
        uint64_t key = (static_cast<uint64_t>(x) << 32) | (static_cast<uint64_t>(y) << 16) | z;
        if (seen.insert(key).second) {
            points.push_back({x, y, z});
        }
    }
    return points;
}

// Simple timing helper
template<typename Func>
double measure_query_ns(Func&& func, size_t iterations) {
    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < iterations; ++i) {
        func(i);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    return static_cast<double>(duration.count()) / iterations;
}

// ============================================================================
// Benchmark Function
// ============================================================================

BenchResult run_density_benchmark(double fill_rate, unsigned dimensions = 3) {
    BenchResult result;
    result.fill_rate = fill_rate;
    result.dimensions = dimensions;

    // Calculate grid and element counts
    size_t grid_dim = 1ULL << GRID_LOG_SIZE;  // 256 for 3D
    size_t total_cells = (dimensions == 2) ? grid_dim * grid_dim
                                           : grid_dim * grid_dim * grid_dim;
    size_t num_elements = static_cast<size_t>(total_cells * fill_rate);
    num_elements = std::max(num_elements, size_t(100));  // Minimum 100 elements
    result.num_elements = num_elements;

    uint32_t max_coord = grid_dim - 1;

    std::cout << "  Fill rate: " << std::fixed << std::setprecision(4)
              << (fill_rate * 100) << "% (" << num_elements << " elements)... ";
    std::cout.flush();

    // Generate data
    auto insert_data = generate_unique_points_3d(num_elements, max_coord, 42);
    auto query_data = generate_unique_points_3d(NUM_QUERIES, max_coord, 123);

    // ========== Setup data structures ==========

    // 1. Dense voxel grid
    DenseVoxelGrid dense(grid_dim, dimensions);

    // 2. Coordinate list
    CoordinateList sparse;

    // 3. SBBF with ~10 bits per element (standard bloom filter sizing)
    const double TARGET_BPE = 10.0;
    unsigned log_blocks = compute_log_blocks_for_bpe(num_elements, HASH_K, TARGET_BPE);

    SBBFConfig conf;
    conf.sfc_type = SFCType::HILBERT_3D;
    conf.sfc_bits = COORD_BITS;
    conf.log_num_blocks = log_blocks;
    conf.hash_k = HASH_K;
    conf.bits_per_block = 64;
    conf.intra_strategy = IntraBlockStrategy::PATTERN_LOOKUP;
    conf.seed_strategy = SeedStrategy::XOR;
    conf.pattern_table_size = 1024;

    SpatialBlockedBloomFilter64<COORD_BITS> sbbf(conf);

    // ========== Insert all elements ==========
    for (const auto& p : insert_data) {
        dense.insert(p.x, p.y, p.z);
        uint64_t mc = sfc::Morton3D<COORD_BITS>::encode(p.x, p.y, p.z);
        sparse.insert(mc);
        sbbf.put3D(p.x, p.y, p.z);
    }
    sparse.finalize();

    // ========== Memory metrics ==========
    result.sbbf_memory_bytes = sbbf.memory_usage();
    result.dense_memory_bytes = dense.memory_bytes();
    result.sparse_memory_bytes = sparse.memory_bytes();

    result.memory_ratio_vs_dense = static_cast<double>(result.sbbf_memory_bytes)
                                 / result.dense_memory_bytes;
    result.memory_ratio_vs_sparse = static_cast<double>(result.sbbf_memory_bytes)
                                  / result.sparse_memory_bytes;

    result.sbbf_bits_per_element = (result.sbbf_memory_bytes * 8.0) / num_elements;
    result.sparse_bits_per_element = sparse.bits_per_element();
    result.dense_bits_per_element = (result.dense_memory_bytes * 8.0) / num_elements;

    // ========== Query performance ==========

    // Prepare query Morton codes
    std::vector<uint64_t> query_morton;
    query_morton.reserve(query_data.size());
    for (const auto& p : query_data) {
        query_morton.push_back(sfc::Morton3D<COORD_BITS>::encode(p.x, p.y, p.z));
    }

    // Benchmark dense queries
    volatile bool sink = false;
    result.dense_query_ns = measure_query_ns([&](size_t i) {
        const auto& p = query_data[i % query_data.size()];
        sink = dense.query(p.x, p.y, p.z);
    }, NUM_QUERIES);

    // Benchmark sparse queries
    result.sparse_query_ns = measure_query_ns([&](size_t i) {
        sink = sparse.query(query_morton[i % query_morton.size()]);
    }, NUM_QUERIES);

    // Benchmark SBBF queries
    result.sbbf_query_ns = measure_query_ns([&](size_t i) {
        const auto& p = query_data[i % query_data.size()];
        sink = sbbf.get_bool_3D(p.x, p.y, p.z);
    }, NUM_QUERIES);
    (void)sink;  // Prevent optimization

    result.query_ratio_vs_dense = result.sbbf_query_ns / result.dense_query_ns;
    result.query_ratio_vs_sparse = result.sbbf_query_ns / result.sparse_query_ns;

    // ========== FPR measurement ==========
    size_t false_positives = 0;
    for (const auto& p : query_data) {
        // Check if this is actually a false positive (not in insert set)
        bool in_dense = dense.query(p.x, p.y, p.z);
        bool in_sbbf = sbbf.get_bool_3D(p.x, p.y, p.z);
        if (in_sbbf && !in_dense) {
            ++false_positives;
        }
    }
    // Count actual negatives for FPR calculation
    size_t actual_negatives = 0;
    for (const auto& p : query_data) {
        if (!dense.query(p.x, p.y, p.z)) {
            ++actual_negatives;
        }
    }
    result.fpr = (actual_negatives > 0)
               ? static_cast<double>(false_positives) / actual_negatives
               : 0.0;

    std::cout << "done (SBBF: " << result.sbbf_memory_bytes << " B, "
              << "FPR: " << std::fixed << std::setprecision(2)
              << (result.fpr * 100) << "%)\n";

    return result;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "========================================\n";
    std::cout << "SBBF Density vs Compression Benchmark\n";
    std::cout << "========================================\n";
    std::cout << "Grid size: " << (1 << GRID_LOG_SIZE) << "^3 = "
              << ((1ULL << GRID_LOG_SIZE) * (1ULL << GRID_LOG_SIZE) * (1ULL << GRID_LOG_SIZE))
              << " cells\n";
    std::cout << "Fill rates: " << FILL_RATES.size() << " levels\n";
    std::cout << "SBBF config: k=" << HASH_K << ", pattern_lookup, Hilbert3D\n";
    std::cout << "========================================\n\n";

    ensure_dir("./sbbf_results");
    ensure_dir(OUTPUT_DIR);

    std::vector<BenchResult> results;

    std::cout << "Running 3D benchmarks:\n";
    for (double fill_rate : FILL_RATES) {
        auto result = run_density_benchmark(fill_rate, 3);
        results.push_back(result);
    }

    // ========== Save results to JSON ==========
    json output;
    output["experiment"] = "density_compression";
    output["timestamp"] = get_iso_timestamp();
    output["config"] = {
        {"grid_log_size", GRID_LOG_SIZE},
        {"grid_dim", (1 << GRID_LOG_SIZE)},
        {"dimensions", 3},
        {"coord_bits", COORD_BITS},
        {"hash_k", HASH_K},
        {"num_queries", NUM_QUERIES},
        {"fill_rates", FILL_RATES},
        {"sbbf_config", {
            {"sfc_type", "HILBERT_3D"},
            {"intra_strategy", "pattern_lookup"},
            {"seed_strategy", "XOR"},
            {"target_bits_per_element", 10.0}
        }}
    };

    json results_json = json::array();
    for (const auto& r : results) {
        results_json.push_back({
            {"config", {
                {"fill_rate", r.fill_rate},
                {"num_elements", r.num_elements},
                {"dimensions", r.dimensions}
            }},
            {"metrics", {
                {"sbbf_memory_bytes", r.sbbf_memory_bytes},
                {"dense_memory_bytes", r.dense_memory_bytes},
                {"sparse_memory_bytes", r.sparse_memory_bytes},
                {"memory_ratio_vs_dense", r.memory_ratio_vs_dense},
                {"memory_ratio_vs_sparse", r.memory_ratio_vs_sparse},
                {"sbbf_bits_per_element", r.sbbf_bits_per_element},
                {"sparse_bits_per_element", r.sparse_bits_per_element},
                {"dense_bits_per_element", r.dense_bits_per_element},
                {"sbbf_query_ns", r.sbbf_query_ns},
                {"dense_query_ns", r.dense_query_ns},
                {"sparse_query_ns", r.sparse_query_ns},
                {"query_ratio_vs_dense", r.query_ratio_vs_dense},
                {"query_ratio_vs_sparse", r.query_ratio_vs_sparse},
                {"fpr", r.fpr}
            }}
        });
    }
    output["results"] = results_json;

    std::string output_path = OUTPUT_DIR + "uniform_3d.json";
    std::ofstream out(output_path);
    out << output.dump(2);
    out.close();

    // ========== Print summary table ==========
    std::cout << "\n========================================\n";
    std::cout << "Summary\n";
    std::cout << "========================================\n";
    std::cout << std::setw(10) << "Fill%"
              << std::setw(12) << "Elements"
              << std::setw(12) << "SBBF(KB)"
              << std::setw(12) << "Dense(KB)"
              << std::setw(12) << "Sparse(KB)"
              << std::setw(10) << "Ratio/D"
              << std::setw(10) << "Ratio/S"
              << std::setw(8) << "FPR%"
              << "\n";
    std::cout << std::string(86, '-') << "\n";

    for (const auto& r : results) {
        std::cout << std::fixed << std::setprecision(3)
                  << std::setw(10) << (r.fill_rate * 100)
                  << std::setw(12) << r.num_elements
                  << std::setw(12) << (r.sbbf_memory_bytes / 1024.0)
                  << std::setw(12) << (r.dense_memory_bytes / 1024.0)
                  << std::setw(12) << (r.sparse_memory_bytes / 1024.0)
                  << std::setprecision(4)
                  << std::setw(10) << r.memory_ratio_vs_dense
                  << std::setw(10) << r.memory_ratio_vs_sparse
                  << std::setprecision(2)
                  << std::setw(8) << (r.fpr * 100)
                  << "\n";
    }

    std::cout << "\n========================================\n";
    std::cout << "Results saved to: " << output_path << "\n";
    std::cout << "========================================\n";

    return 0;
}
