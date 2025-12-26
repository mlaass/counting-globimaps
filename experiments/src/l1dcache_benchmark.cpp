/*
 * L1-dcache Hit Rate Benchmark for SBBF Paper
 *
 * Measures L1 data cache hit rates during 3D voxel volume traversals.
 * Compares SBBF-Hilbert (SFC-ordered traversal) vs BlockedBF (hash-based).
 *
 * This benchmark fills in the paper paragraph:
 *   "In a 256^3 voxel traversal, SBBF-Hilbert achieves a ??% L1-dcache hit rate,
 *    compared to ??% for standard BlockedBF."
 *
 * Requirements:
 *   - Linux with perf_event support
 *   - kernel.perf_event_paranoid <= 1
 *     Run: sudo sysctl kernel.perf_event_paranoid=1
 *
 * Usage:
 *   ./l1dcache_benchmark --grid-sizes 64,128,256 --output sbbf_results/l1dcache/
 */

#include "perf_l1dcache.hpp"
#include "spatial_blocked_bloom_filter.hpp"
#include "blocked_bloom_filter.hpp"
#include "register_blocked_bf.hpp"
#include "globimap.hpp"
#include "space_filling_curves.hpp"
#include "json.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
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

// Prevent compiler from optimizing away results
template<typename T>
void doNotOptimize(T&& val) {
    asm volatile("" : : "r,m"(val) : "memory");
}

// ============================================================================
// Configuration
// ============================================================================

struct Config {
    std::vector<unsigned> grid_sizes = {64, 128, 256};
    double bits_per_element = 10.0;
    unsigned hash_k = 4;
    unsigned warmup_iterations = 3;
    unsigned measure_iterations = 5;
    std::string output_dir = "sbbf_results/l1dcache";
};

// ============================================================================
// Point Type
// ============================================================================

struct Point3D {
    uint32_t x, y, z;
};

// ============================================================================
// Grid Generation
// ============================================================================

std::vector<Point3D> generate_row_major(unsigned size) {
    std::vector<Point3D> points;
    points.reserve((size_t)size * size * size);
    for (uint32_t z = 0; z < size; ++z) {
        for (uint32_t y = 0; y < size; ++y) {
            for (uint32_t x = 0; x < size; ++x) {
                points.push_back({x, y, z});
            }
        }
    }
    return points;
}

std::vector<Point3D> generate_hilbert_sorted(unsigned size) {
    auto points = generate_row_major(size);
    std::sort(points.begin(), points.end(),
              [](const Point3D& a, const Point3D& b) {
                  return sfc::Hilbert3D<12>::encode(a.x, a.y, a.z) <
                         sfc::Hilbert3D<12>::encode(b.x, b.y, b.z);
              });
    return points;
}

std::vector<Point3D> generate_random_order(unsigned size, uint64_t seed = 42) {
    auto points = generate_row_major(size);
    std::mt19937_64 rng(seed);
    std::shuffle(points.begin(), points.end(), rng);
    return points;
}

// ============================================================================
// Result Structure
// ============================================================================

struct CacheResult {
    std::string filter_name;
    std::string traversal_order;
    unsigned grid_size;
    uint64_t grid_elements;
    uint64_t memory_bytes;

    // L1-dcache metrics (averaged over iterations)
    uint64_t l1d_loads;
    uint64_t l1d_misses;
    double l1d_hit_rate_pct;

    // Timing
    double total_time_ms;
    double query_ns_per_op;

    json to_json() const {
        return {
            {"filter", filter_name},
            {"traversal", traversal_order},
            {"grid_size", grid_size},
            {"grid_elements", grid_elements},
            {"memory_bytes", memory_bytes},
            {"l1d_loads", l1d_loads},
            {"l1d_misses", l1d_misses},
            {"l1d_hit_rate_pct", l1d_hit_rate_pct},
            {"total_time_ms", total_time_ms},
            {"query_ns_per_op", query_ns_per_op}
        };
    }
};

// ============================================================================
// Benchmark Functions
// ============================================================================

// Measure sequential SFC traversal (for(sfc=0;sfc<max;++sfc) pattern)
// This is the key optimization - sequential SFC codes yield sequential block accesses
template<typename SBBFType>
CacheResult measure_sfc_sequential(
    const std::string& filter_name,
    const std::string& traversal_name,
    SBBFType& sbbf,
    uint64_t max_sfc,
    unsigned grid_size,
    unsigned warmup_iters,
    unsigned measure_iters,
    perf::L1DCachePerfCounters& counters
) {
    CacheResult result;
    result.filter_name = filter_name;
    result.traversal_order = traversal_name;
    result.grid_size = grid_size;
    result.grid_elements = max_sfc;
    result.memory_bytes = sbbf.memory_usage();

    // Warmup runs
    for (unsigned i = 0; i < warmup_iters; ++i) {
        size_t count = 0;
        for (uint64_t sfc = 0; sfc < max_sfc; ++sfc) {
            if (sbbf.get_bool_by_sfc(sfc)) ++count;
        }
        doNotOptimize(count);
    }

    // Measurement runs
    uint64_t total_loads = 0;
    uint64_t total_misses = 0;
    double total_time_ns = 0;

    for (unsigned iter = 0; iter < measure_iters; ++iter) {
        counters.reset();
        counters.start();

        auto start = std::chrono::high_resolution_clock::now();

        size_t count = 0;
        for (uint64_t sfc = 0; sfc < max_sfc; ++sfc) {
            if (sbbf.get_bool_by_sfc(sfc)) ++count;
        }
        doNotOptimize(count);

        auto end = std::chrono::high_resolution_clock::now();
        counters.stop();

        auto perf_result = counters.read();
        total_loads += perf_result.loads;
        total_misses += perf_result.misses;
        total_time_ns += std::chrono::duration<double, std::nano>(end - start).count();
    }

    // Average results
    result.l1d_loads = total_loads / measure_iters;
    result.l1d_misses = total_misses / measure_iters;
    result.l1d_hit_rate_pct = result.l1d_loads > 0
        ? (double)(result.l1d_loads - result.l1d_misses) / result.l1d_loads * 100.0
        : 0.0;
    result.total_time_ms = total_time_ns / measure_iters / 1e6;
    result.query_ns_per_op = total_time_ns / measure_iters / result.grid_elements;

    return result;
}

// Measure a full volume traversal with L1-dcache counters (coordinate-based)
template<typename QueryFunc>
CacheResult measure_traversal(
    const std::string& filter_name,
    const std::string& traversal_name,
    const std::vector<Point3D>& traversal_order,
    QueryFunc query_fn,
    unsigned grid_size,
    uint64_t memory_bytes,
    unsigned warmup_iters,
    unsigned measure_iters,
    perf::L1DCachePerfCounters& counters
) {
    CacheResult result;
    result.filter_name = filter_name;
    result.traversal_order = traversal_name;
    result.grid_size = grid_size;
    result.grid_elements = traversal_order.size();
    result.memory_bytes = memory_bytes;

    // Warmup runs
    for (unsigned i = 0; i < warmup_iters; ++i) {
        size_t count = 0;
        for (const auto& p : traversal_order) {
            if (query_fn(p.x, p.y, p.z)) ++count;
        }
        doNotOptimize(count);
    }

    // Measurement runs
    uint64_t total_loads = 0;
    uint64_t total_misses = 0;
    double total_time_ns = 0;

    for (unsigned iter = 0; iter < measure_iters; ++iter) {
        counters.reset();
        counters.start();

        auto start = std::chrono::high_resolution_clock::now();

        size_t count = 0;
        for (const auto& p : traversal_order) {
            if (query_fn(p.x, p.y, p.z)) ++count;
        }
        doNotOptimize(count);

        auto end = std::chrono::high_resolution_clock::now();
        counters.stop();

        auto perf_result = counters.read();
        total_loads += perf_result.loads;
        total_misses += perf_result.misses;
        total_time_ns += std::chrono::duration<double, std::nano>(end - start).count();
    }

    // Average results
    result.l1d_loads = total_loads / measure_iters;
    result.l1d_misses = total_misses / measure_iters;
    result.l1d_hit_rate_pct = result.l1d_loads > 0
        ? (double)(result.l1d_loads - result.l1d_misses) / result.l1d_loads * 100.0
        : 0.0;
    result.total_time_ms = total_time_ns / measure_iters / 1e6;
    result.query_ns_per_op = total_time_ns / measure_iters / result.grid_elements;

    return result;
}

// Run benchmark for a specific grid size
std::vector<CacheResult> run_benchmark_for_size(
    unsigned grid_size,
    const Config& config,
    perf::L1DCachePerfCounters& counters
) {
    std::vector<CacheResult> results;

    const uint64_t n_elements = (uint64_t)grid_size * grid_size * grid_size;

    std::cout << "\n========================================\n";
    std::cout << "Grid size: " << grid_size << "^3 = " << n_elements << " elements\n";
    std::cout << "========================================\n\n";

    // Generate traversal orders
    std::cout << "Generating traversal orders...\n";
    auto row_major = generate_row_major(grid_size);
    auto hilbert_sorted = generate_hilbert_sorted(grid_size);
    auto random_order = generate_random_order(grid_size);

    // Calculate filter sizes (~10 bits per element)
    unsigned log_blocks = static_cast<unsigned>(
        std::ceil(std::log2(n_elements * config.bits_per_element / 64.0))
    );
    uint64_t num_blocks = 1ULL << log_blocks;
    uint64_t sbbf_memory = num_blocks * 8;  // 64 bits per block

    std::cout << "Filter config: log_blocks=" << log_blocks
              << ", num_blocks=" << num_blocks
              << ", memory=" << (sbbf_memory / 1024) << " KB\n";

    // Calculate max SFC code for this grid size (for 8 bits per coord = 256 max)
    // Use min(grid_size, 256) bits per dimension
    unsigned sfc_bits = std::min(8u, static_cast<unsigned>(std::ceil(std::log2(grid_size + 1))));
    uint64_t max_sfc = (uint64_t)grid_size * grid_size * grid_size;

    // ====== SBBF-Hilbert with Sequential SFC Iteration ======
    {
        SBBFConfig sbbf_conf;
        sbbf_conf.sfc_type = SFCType::HILBERT_3D;
        sbbf_conf.sfc_bits = 8;  // 8 bits per coord for 256^3 max
        sbbf_conf.log_num_blocks = log_blocks;
        sbbf_conf.hash_k = config.hash_k;
        sbbf_conf.bits_per_block = 64;
        sbbf_conf.intra_strategy = sbbf::IntraBlockStrategy::PATTERN_LOOKUP;
        sbbf_conf.pattern_table_size = 1024;

        SpatialBlockedBloomFilter64<8> sbbf(sbbf_conf);

        // Fill filter using sequential SFC codes
        std::cout << "Filling SBBF-Hilbert (SFC sequential)...\n";
        for (uint64_t sfc = 0; sfc < max_sfc; ++sfc) {
            sbbf.put_by_sfc(sfc);
        }

        // Measure with sequential SFC iteration (optimal - enables prefetching)
        std::cout << "Measuring SBBF-Hilbert + SFC-sequential...\n";
        auto sbbf_seq = measure_sfc_sequential(
            "SBBF-Hilbert", "sfc_sequential", sbbf, max_sfc,
            grid_size, config.warmup_iterations, config.measure_iterations, counters
        );
        results.push_back(sbbf_seq);
        std::cout << "  L1D hit rate: " << std::fixed << std::setprecision(2)
                  << sbbf_seq.l1d_hit_rate_pct << "% (misses: " << sbbf_seq.l1d_misses << ")\n";

        // Also measure with random SFC codes (worst case)
        std::cout << "Measuring SBBF-Hilbert + random SFC...\n";
        std::vector<uint64_t> random_sfc_codes(max_sfc);
        for (uint64_t i = 0; i < max_sfc; ++i) random_sfc_codes[i] = i;
        std::mt19937_64 rng(42);
        std::shuffle(random_sfc_codes.begin(), random_sfc_codes.end(), rng);

        auto sbbf_random = measure_traversal(
            "SBBF-Hilbert", "random", random_order,
            [&](uint32_t x, uint32_t y, uint32_t z) { return sbbf.get_bool_3D(x, y, z); },
            grid_size, sbbf.memory_usage(),
            config.warmup_iterations, config.measure_iterations, counters
        );
        results.push_back(sbbf_random);
        std::cout << "  L1D hit rate: " << std::fixed << std::setprecision(2)
                  << sbbf_random.l1d_hit_rate_pct << "%\n";
    }

    // ====== SBBF-Morton with Sequential SFC Iteration ======
    {
        SBBFConfig sbbf_conf;
        sbbf_conf.sfc_type = SFCType::MORTON_3D;
        sbbf_conf.sfc_bits = 8;  // 8 bits per coord for 256^3 max
        sbbf_conf.log_num_blocks = log_blocks;
        sbbf_conf.hash_k = config.hash_k;
        sbbf_conf.bits_per_block = 64;
        sbbf_conf.intra_strategy = sbbf::IntraBlockStrategy::PATTERN_LOOKUP;
        sbbf_conf.pattern_table_size = 1024;

        SpatialBlockedBloomFilter64<8> sbbf(sbbf_conf);

        // Fill filter using sequential SFC codes
        std::cout << "Filling SBBF-Morton (SFC sequential)...\n";
        for (uint64_t sfc = 0; sfc < max_sfc; ++sfc) {
            sbbf.put_by_sfc(sfc);
        }

        // Measure with sequential SFC iteration (optimal - enables prefetching)
        std::cout << "Measuring SBBF-Morton + SFC-sequential...\n";
        auto sbbf_seq = measure_sfc_sequential(
            "SBBF-Morton", "sfc_sequential", sbbf, max_sfc,
            grid_size, config.warmup_iterations, config.measure_iterations, counters
        );
        results.push_back(sbbf_seq);
        std::cout << "  L1D hit rate: " << std::fixed << std::setprecision(2)
                  << sbbf_seq.l1d_hit_rate_pct << "% (misses: " << sbbf_seq.l1d_misses << ")\n";
    }

    // ====== BlockedBF ======
    {
        // Configure BlockedBF with same memory budget
        BlockedBFConfig bbf_conf;
        bbf_conf.num_blocks = sbbf_memory / 32;  // 256 bits per block = 32 bytes
        bbf_conf.hash_k = config.hash_k;
        bbf_conf.intra_strategy = globimap::IntraBlockStrategy::PATTERN_LOOKUP;
        bbf_conf.pattern_table_size = 1024;

        BlockedBloomFilter<WyHasher> bbf(bbf_conf);

        // Fill filter with all grid points
        std::cout << "Filling BlockedBF...\n";
        for (const auto& p : row_major) {
            bbf.put({p.x, p.y, p.z});
        }

        // Measure with row-major traversal (realistic use case)
        std::cout << "Measuring BlockedBF + row-major traversal...\n";
        auto bbf_rowmajor = measure_traversal(
            "BlockedBF", "row_major", row_major,
            [&](uint32_t x, uint32_t y, uint32_t z) { return bbf.get_bool({x, y, z}); },
            grid_size, bbf.memory_usage(),
            config.warmup_iterations, config.measure_iterations, counters
        );
        results.push_back(bbf_rowmajor);
        std::cout << "  L1D hit rate: " << std::fixed << std::setprecision(2)
                  << bbf_rowmajor.l1d_hit_rate_pct << "%\n";

        // Measure with Hilbert traversal (control - isolates filter design from traversal)
        std::cout << "Measuring BlockedBF + Hilbert traversal...\n";
        auto bbf_hilbert = measure_traversal(
            "BlockedBF", "hilbert", hilbert_sorted,
            [&](uint32_t x, uint32_t y, uint32_t z) { return bbf.get_bool({x, y, z}); },
            grid_size, bbf.memory_usage(),
            config.warmup_iterations, config.measure_iterations, counters
        );
        results.push_back(bbf_hilbert);
        std::cout << "  L1D hit rate: " << std::fixed << std::setprecision(2)
                  << bbf_hilbert.l1d_hit_rate_pct << "%\n";

        // Measure with random traversal (worst case)
        std::cout << "Measuring BlockedBF + random traversal...\n";
        auto bbf_random = measure_traversal(
            "BlockedBF", "random", random_order,
            [&](uint32_t x, uint32_t y, uint32_t z) { return bbf.get_bool({x, y, z}); },
            grid_size, bbf.memory_usage(),
            config.warmup_iterations, config.measure_iterations, counters
        );
        results.push_back(bbf_random);
        std::cout << "  L1D hit rate: " << std::fixed << std::setprecision(2)
                  << bbf_random.l1d_hit_rate_pct << "%\n";
    }

    // ====== RegisterBF ======
    {
        // Configure RegisterBF with same memory budget (~10 bits per element)
        RegisterBlockedBFConfig reg_conf;
        reg_conf.expected_items = n_elements;
        reg_conf.false_positive_rate = 0.01;  // ~7 bits/element base + compensation
        reg_conf.compensation = 3;  // Bump to ~10 bits/element
        reg_conf.intra_strategy = globimap::IntraBlockStrategy::PATTERN_LOOKUP;
        reg_conf.pattern_table_size = 1024;

        RegisterBlockedBloomFilter<> reg_bf(reg_conf);

        // Fill filter with all grid points
        std::cout << "Filling RegisterBF...\n";
        for (const auto& p : row_major) {
            reg_bf.put({(uint64_t)p.x, (uint64_t)p.y, (uint64_t)p.z});
        }

        // Measure with row-major traversal
        std::cout << "Measuring RegisterBF + row-major traversal...\n";
        auto reg_rowmajor = measure_traversal(
            "RegisterBF", "row_major", row_major,
            [&](uint32_t x, uint32_t y, uint32_t z) {
                return reg_bf.get_bool({(uint64_t)x, (uint64_t)y, (uint64_t)z});
            },
            grid_size, reg_bf.memory_usage(),
            config.warmup_iterations, config.measure_iterations, counters
        );
        results.push_back(reg_rowmajor);
        std::cout << "  L1D hit rate: " << std::fixed << std::setprecision(2)
                  << reg_rowmajor.l1d_hit_rate_pct << "%\n";
    }

    // ====== GloBiMap (Standard Bloom Filter) ======
    {
        // Configure GloBiMap with same hash_k and ~10 bits per element
        GloBiMap<> globimap;
        unsigned log_size = static_cast<unsigned>(
            std::ceil(std::log2(n_elements * config.bits_per_element))
        );
        globimap.configure(config.hash_k, log_size);

        // Fill filter with all grid points
        std::cout << "Filling GloBiMap...\n";
        for (const auto& p : row_major) {
            globimap.put({(uint64_t)p.x, (uint64_t)p.y, (uint64_t)p.z});
        }

        // Memory: filter is std::vector<bool>, but actual memory usage varies
        uint64_t globimap_memory = globimap.filter.size() / 8;  // bits to bytes

        // Measure with row-major traversal
        std::cout << "Measuring GloBiMap + row-major traversal...\n";
        auto gbm_rowmajor = measure_traversal(
            "GloBiMap", "row_major", row_major,
            [&](uint32_t x, uint32_t y, uint32_t z) {
                return globimap.get({(uint64_t)x, (uint64_t)y, (uint64_t)z});
            },
            grid_size, globimap_memory,
            config.warmup_iterations, config.measure_iterations, counters
        );
        results.push_back(gbm_rowmajor);
        std::cout << "  L1D hit rate: " << std::fixed << std::setprecision(2)
                  << gbm_rowmajor.l1d_hit_rate_pct << "%\n";
    }

    return results;
}

// ============================================================================
// Output Functions
// ============================================================================

std::string get_iso_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::gmtime(&time);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

void save_results(const std::vector<CacheResult>& results, const Config& config, const std::string& output_path) {
    json output;

    output["metadata"] = {
        {"benchmark", "l1dcache_volume_traversal"},
        {"timestamp", get_iso_timestamp()},
        {"description", "L1-dcache hit rate during 3D voxel volume traversals"}
    };

    output["parameters"] = {
        {"grid_sizes", config.grid_sizes},
        {"bits_per_element", config.bits_per_element},
        {"hash_k", config.hash_k},
        {"warmup_iterations", config.warmup_iterations},
        {"measure_iterations", config.measure_iterations}
    };

    output["results"] = json::array();
    for (const auto& r : results) {
        output["results"].push_back(r.to_json());
    }

    std::ofstream file(output_path);
    if (!file.is_open()) {
        std::cerr << "Failed to open output file: " << output_path << "\n";
        return;
    }
    file << std::setw(2) << output << std::endl;
    std::cout << "\nResults saved to: " << output_path << "\n";
}

void print_summary(const std::vector<CacheResult>& results) {
    std::cout << "\n========================================\n";
    std::cout << "Summary: L1-dcache Hit Rates\n";
    std::cout << "========================================\n\n";

    // Group by grid size
    std::map<unsigned, std::vector<const CacheResult*>> by_size;
    for (const auto& r : results) {
        by_size[r.grid_size].push_back(&r);
    }

    for (const auto& [size, rs] : by_size) {
        std::cout << "Grid " << size << "^3:\n";
        for (const auto* r : rs) {
            std::cout << "  " << std::setw(20) << std::left << r->filter_name
                      << " + " << std::setw(10) << r->traversal_order
                      << " -> " << std::fixed << std::setprecision(2)
                      << std::setw(6) << r->l1d_hit_rate_pct << "% hit rate"
                      << " (" << std::setprecision(1) << r->query_ns_per_op << " ns/op)\n";
        }
        std::cout << "\n";
    }

    // Print paper-ready values
    std::cout << "Paper values for 256^3 traversal:\n";
    std::cout << "(Miss rate = 100 - hit rate; lower is better)\n\n";
    for (const auto& r : results) {
        if (r.grid_size == 256) {
            double miss_rate = 100.0 - r.l1d_hit_rate_pct;
            if (r.filter_name == "SBBF-Hilbert" && r.traversal_order == "sfc_sequential") {
                std::cout << "  SBBF-Hilbert (SFC-seq): " << std::fixed << std::setprecision(2)
                          << miss_rate << "% miss rate, " << std::setprecision(1)
                          << r.query_ns_per_op << " ns/op\n";
            }
            if (r.filter_name == "SBBF-Morton" && r.traversal_order == "sfc_sequential") {
                std::cout << "  SBBF-Morton  (SFC-seq): " << std::fixed << std::setprecision(2)
                          << miss_rate << "% miss rate, " << std::setprecision(1)
                          << r.query_ns_per_op << " ns/op\n";
            }
            if (r.filter_name == "BlockedBF" && r.traversal_order == "row_major") {
                std::cout << "  BlockedBF (row-major):  " << std::fixed << std::setprecision(2)
                          << miss_rate << "% miss rate, " << std::setprecision(1)
                          << r.query_ns_per_op << " ns/op\n";
            }
            if (r.filter_name == "RegisterBF" && r.traversal_order == "row_major") {
                std::cout << "  RegisterBF (row-major): " << std::fixed << std::setprecision(2)
                          << miss_rate << "% miss rate, " << std::setprecision(1)
                          << r.query_ns_per_op << " ns/op\n";
            }
            if (r.filter_name == "GloBiMap" && r.traversal_order == "row_major") {
                std::cout << "  GloBiMap (row-major):   " << std::fixed << std::setprecision(2)
                          << miss_rate << "% miss rate, " << std::setprecision(1)
                          << r.query_ns_per_op << " ns/op\n";
            }
        }
    }
}

// ============================================================================
// Argument Parsing
// ============================================================================

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n"
              << "\nOptions:\n"
              << "  --grid-sizes LIST   Comma-separated grid sizes (default: 64,128,256)\n"
              << "  --bits-per-elem N   Target bits per element (default: 10)\n"
              << "  --hash-k N          Number of hash functions (default: 4)\n"
              << "  --warmup N          Warmup iterations (default: 3)\n"
              << "  --iterations N      Measurement iterations (default: 5)\n"
              << "  --output DIR        Output directory (default: sbbf_results/l1dcache)\n"
              << "  --help              Show this help\n";
}

std::vector<unsigned> parse_int_list(const std::string& s) {
    std::vector<unsigned> result;
    std::istringstream iss(s);
    std::string token;
    while (std::getline(iss, token, ',')) {
        result.push_back(static_cast<unsigned>(std::stoul(token)));
    }
    return result;
}

bool parse_args(int argc, char** argv, Config& config) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return false;
        }
        else if (arg == "--grid-sizes" && i + 1 < argc) {
            config.grid_sizes = parse_int_list(argv[++i]);
        }
        else if (arg == "--bits-per-elem" && i + 1 < argc) {
            config.bits_per_element = std::stod(argv[++i]);
        }
        else if (arg == "--hash-k" && i + 1 < argc) {
            config.hash_k = static_cast<unsigned>(std::stoul(argv[++i]));
        }
        else if (arg == "--warmup" && i + 1 < argc) {
            config.warmup_iterations = static_cast<unsigned>(std::stoul(argv[++i]));
        }
        else if (arg == "--iterations" && i + 1 < argc) {
            config.measure_iterations = static_cast<unsigned>(std::stoul(argv[++i]));
        }
        else if (arg == "--output" && i + 1 < argc) {
            config.output_dir = argv[++i];
        }
        else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return false;
        }
    }
    return true;
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    std::cout << "=================================================\n";
    std::cout << "L1-dcache Hit Rate Benchmark for SBBF Paper\n";
    std::cout << "=================================================\n";

    Config config;
    if (!parse_args(argc, argv, config)) {
        return 1;
    }

    // Initialize perf counters
    perf::L1DCachePerfCounters counters;
    if (!counters.init()) {
        std::cerr << "\nFailed to initialize L1-dcache performance counters.\n";
        std::cerr << perf::L1DCachePerfCounters::get_requirements_message();
        return 1;
    }

    std::cout << "\nConfiguration:\n";
    std::cout << "  Grid sizes: ";
    for (auto s : config.grid_sizes) std::cout << s << " ";
    std::cout << "\n";
    std::cout << "  Bits per element: " << config.bits_per_element << "\n";
    std::cout << "  Hash k: " << config.hash_k << "\n";
    std::cout << "  Warmup iterations: " << config.warmup_iterations << "\n";
    std::cout << "  Measure iterations: " << config.measure_iterations << "\n";
    std::cout << "  Output directory: " << config.output_dir << "\n";

    // Create output directory
    mkdir(config.output_dir.c_str(), 0755);

    // Run benchmarks
    std::vector<CacheResult> all_results;
    for (unsigned size : config.grid_sizes) {
        auto results = run_benchmark_for_size(size, config, counters);
        all_results.insert(all_results.end(), results.begin(), results.end());
    }

    // Print summary
    print_summary(all_results);

    // Save results
    std::string output_path = config.output_dir + "/l1dcache_results.json";
    save_results(all_results, config, output_path);

    return 0;
}
