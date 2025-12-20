/*
 * Spatial-Blocked Bloom Filter Benchmark
 *
 * Compares SBBF variants with traditional bloom filters:
 * - SBBF (Morton 2D/3D)
 * - SBBF (Hilbert 2D/3D)
 * - BlockedBloomFilter (cache-line aligned baseline)
 * - RegisterBlockedBloomFilter (64-bit atomic baseline)
 *
 * Key metrics:
 * - Insert throughput (ns/op)
 * - Query throughput (ns/op)
 * - Neighborhood query throughput (ns/op) - SBBF advantage
 * - False positive rate
 * - Memory usage
 *
 * Output: JSON files in results/sbbf/ for Jupyter analysis
 */

#include "spatial_blocked_bloom_filter.hpp"
#include "blocked_bloom_filter.hpp"
#include "register_blocked_bf.hpp"

#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <vector>

using namespace sbbf;
using namespace globimap;

const std::string results_path = "./results/sbbf/";

// ============================================================================
// Result Structures
// ============================================================================

struct BenchmarkResult {
    std::string impl_name;
    std::string sfc_type;
    uint64_t memory_bytes;
    double insert_ns_per_op;
    double query_ns_per_op;
    double neighborhood_ns_per_op;  // Only for SBBF
    double fpr;
    size_t num_elements;
};

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

    // Generate cluster centers
    std::vector<Point2D> centers;
    for (size_t i = 0; i < num_clusters; ++i) {
        centers.push_back({center_dist(rng), center_dist(rng)});
    }

    // Generate points around clusters
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
        uint32_t x = static_cast<uint32_t>(std::clamp<int32_t>(
            center.x + ox, 0, max_coord));
        uint32_t y = static_cast<uint32_t>(std::clamp<int32_t>(
            center.y + oy, 0, max_coord));
        uint32_t z = static_cast<uint32_t>(std::clamp<int32_t>(
            center.z + oz, 0, max_coord));
        points.push_back({x, y, z});
    }
    return points;
}

// ============================================================================
// Benchmark Functions
// ============================================================================

template<unsigned SFCBits>
BenchmarkResult benchmark_sbbf_2d(const std::string& name, SFCType sfc_type,
                                   const std::vector<Point2D>& insert_data,
                                   const std::vector<Point2D>& query_data,
                                   unsigned log_blocks, unsigned hash_k) {
    using Clock = std::chrono::high_resolution_clock;
    BenchmarkResult result;
    result.impl_name = name;
    result.num_elements = insert_data.size();

    switch (sfc_type) {
        case SFCType::MORTON_2D: result.sfc_type = "MORTON_2D"; break;
        case SFCType::HILBERT_2D: result.sfc_type = "HILBERT_2D"; break;
        default: result.sfc_type = "UNKNOWN"; break;
    }

    // Create filter
    SBBFConfig conf;
    conf.sfc_type = sfc_type;
    conf.sfc_bits = SFCBits;
    conf.log_num_blocks = log_blocks;
    conf.hash_k = hash_k;
    conf.bits_per_block = 64;

    SpatialBlockedBloomFilter64<SFCBits> filter(conf);
    result.memory_bytes = filter.memory_usage();

    // Insert benchmark
    auto t1 = Clock::now();
    for (const auto& p : insert_data) {
        filter.put2D(p.x, p.y);
    }
    auto t2 = Clock::now();
    auto insert_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count();
    result.insert_ns_per_op = static_cast<double>(insert_ns) / insert_data.size();

    // Query benchmark
    volatile size_t found = 0;
    t1 = Clock::now();
    for (const auto& p : query_data) {
        if (filter.get_bool_2D(p.x, p.y)) ++found;
    }
    t2 = Clock::now();
    auto query_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count();
    result.query_ns_per_op = static_cast<double>(query_ns) / query_data.size();

    // Neighborhood query benchmark (SBBF advantage)
    volatile uint64_t neighbor_bits = 0;
    t1 = Clock::now();
    for (const auto& p : insert_data) {
        neighbor_bits += filter.query_neighborhood_2D(p.x, p.y, 1);
    }
    t2 = Clock::now();
    auto nbr_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count();
    result.neighborhood_ns_per_op = static_cast<double>(nbr_ns) / insert_data.size();

    // FPR (query non-inserted data)
    size_t false_positives = 0;
    for (const auto& p : query_data) {
        if (filter.get_bool_2D(p.x, p.y)) {
            // Check if it was actually inserted
            bool was_inserted = false;
            for (const auto& ip : insert_data) {
                if (ip.x == p.x && ip.y == p.y) {
                    was_inserted = true;
                    break;
                }
            }
            if (!was_inserted) ++false_positives;
        }
    }
    result.fpr = static_cast<double>(false_positives) / query_data.size();

    return result;
}

template<unsigned SFCBits>
BenchmarkResult benchmark_sbbf_3d(const std::string& name, SFCType sfc_type,
                                   const std::vector<Point3D>& insert_data,
                                   const std::vector<Point3D>& query_data,
                                   unsigned log_blocks, unsigned hash_k) {
    using Clock = std::chrono::high_resolution_clock;
    BenchmarkResult result;
    result.impl_name = name;
    result.num_elements = insert_data.size();

    switch (sfc_type) {
        case SFCType::MORTON_3D: result.sfc_type = "MORTON_3D"; break;
        case SFCType::HILBERT_3D: result.sfc_type = "HILBERT_3D"; break;
        default: result.sfc_type = "UNKNOWN"; break;
    }

    // Create filter
    SBBFConfig conf;
    conf.sfc_type = sfc_type;
    conf.sfc_bits = SFCBits;
    conf.log_num_blocks = log_blocks;
    conf.hash_k = hash_k;
    conf.bits_per_block = 64;

    SpatialBlockedBloomFilter64<SFCBits> filter(conf);
    result.memory_bytes = filter.memory_usage();

    // Insert benchmark
    auto t1 = Clock::now();
    for (const auto& p : insert_data) {
        filter.put3D(p.x, p.y, p.z);
    }
    auto t2 = Clock::now();
    auto insert_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count();
    result.insert_ns_per_op = static_cast<double>(insert_ns) / insert_data.size();

    // Query benchmark
    volatile size_t found = 0;
    t1 = Clock::now();
    for (const auto& p : query_data) {
        if (filter.get_bool_3D(p.x, p.y, p.z)) ++found;
    }
    t2 = Clock::now();
    auto query_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count();
    result.query_ns_per_op = static_cast<double>(query_ns) / query_data.size();

    // Neighborhood query benchmark
    volatile unsigned neighbor_count = 0;
    t1 = Clock::now();
    for (const auto& p : insert_data) {
        neighbor_count += filter.query_neighborhood_3D(p.x, p.y, p.z, false);
    }
    t2 = Clock::now();
    auto nbr_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count();
    result.neighborhood_ns_per_op = static_cast<double>(nbr_ns) / insert_data.size();

    // Simple FPR check (sample-based)
    size_t fp_checks = std::min(query_data.size(), size_t(10000));
    size_t false_positives = 0;
    for (size_t i = 0; i < fp_checks; ++i) {
        const auto& p = query_data[i];
        if (filter.get_bool_3D(p.x, p.y, p.z)) {
            ++false_positives;  // All query data is non-inserted
        }
    }
    result.fpr = static_cast<double>(false_positives) / fp_checks;

    return result;
}

BenchmarkResult benchmark_blocked_bf_2d(const std::vector<Point2D>& insert_data,
                                         const std::vector<Point2D>& query_data,
                                         size_t expected_items,
                                         double target_fpr) {
    using Clock = std::chrono::high_resolution_clock;
    BenchmarkResult result;
    result.impl_name = "BlockedBloomFilter";
    result.sfc_type = "HASH";
    result.num_elements = insert_data.size();

    BlockedBFConfig conf{expected_items, target_fpr};
    BlockedBloomFilter filter(conf);
    result.memory_bytes = filter.memory_usage();

    // Insert
    auto t1 = Clock::now();
    for (const auto& p : insert_data) {
        filter.put({static_cast<uint64_t>(p.x), static_cast<uint64_t>(p.y)});
    }
    auto t2 = Clock::now();
    result.insert_ns_per_op = static_cast<double>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count()) / insert_data.size();

    // Query
    volatile size_t found = 0;
    t1 = Clock::now();
    for (const auto& p : query_data) {
        if (filter.get_bool({static_cast<uint64_t>(p.x), static_cast<uint64_t>(p.y)})) ++found;
    }
    t2 = Clock::now();
    result.query_ns_per_op = static_cast<double>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count()) / query_data.size();

    // Neighborhood (simulate 3x3 query - no locality advantage)
    t1 = Clock::now();
    for (const auto& p : insert_data) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (filter.get_bool({static_cast<uint64_t>(p.x + dx),
                                     static_cast<uint64_t>(p.y + dy)})) ++found;
            }
        }
    }
    t2 = Clock::now();
    result.neighborhood_ns_per_op = static_cast<double>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count()) / insert_data.size();

    // FPR
    size_t false_positives = 0;
    for (const auto& p : query_data) {
        if (filter.get_bool({static_cast<uint64_t>(p.x), static_cast<uint64_t>(p.y)}))
            ++false_positives;
    }
    result.fpr = static_cast<double>(false_positives) / query_data.size();

    return result;
}

BenchmarkResult benchmark_register_bf_2d(const std::vector<Point2D>& insert_data,
                                          const std::vector<Point2D>& query_data,
                                          size_t expected_items,
                                          double target_fpr) {
    using Clock = std::chrono::high_resolution_clock;
    BenchmarkResult result;
    result.impl_name = "RegisterBlockedBF";
    result.sfc_type = "HASH";
    result.num_elements = insert_data.size();

    RegisterBlockedBFConfig conf{expected_items, target_fpr, 0};
    RegisterBlockedBloomFilter<0> filter(conf);
    result.memory_bytes = filter.memory_usage();

    // Insert
    auto t1 = Clock::now();
    for (const auto& p : insert_data) {
        filter.put({static_cast<uint64_t>(p.x), static_cast<uint64_t>(p.y)});
    }
    auto t2 = Clock::now();
    result.insert_ns_per_op = static_cast<double>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count()) / insert_data.size();

    // Query
    volatile size_t found = 0;
    t1 = Clock::now();
    for (const auto& p : query_data) {
        if (filter.get_bool({static_cast<uint64_t>(p.x), static_cast<uint64_t>(p.y)})) ++found;
    }
    t2 = Clock::now();
    result.query_ns_per_op = static_cast<double>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count()) / query_data.size();

    // Neighborhood
    t1 = Clock::now();
    for (const auto& p : insert_data) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (filter.get_bool({static_cast<uint64_t>(p.x + dx),
                                     static_cast<uint64_t>(p.y + dy)})) ++found;
            }
        }
    }
    t2 = Clock::now();
    result.neighborhood_ns_per_op = static_cast<double>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count()) / insert_data.size();

    // FPR
    size_t false_positives = 0;
    for (const auto& p : query_data) {
        if (filter.get_bool({static_cast<uint64_t>(p.x), static_cast<uint64_t>(p.y)}))
            ++false_positives;
    }
    result.fpr = static_cast<double>(false_positives) / query_data.size();

    return result;
}

// ============================================================================
// JSON Output
// ============================================================================

void save_results_json(const std::vector<BenchmarkResult>& results,
                       const std::string& scenario_name,
                       const std::string& distribution,
                       size_t num_elements,
                       int dimensions) {
    std::stringstream filename;
    filename << results_path << "sbbf_" << scenario_name << ".json";

    std::ofstream out(filename.str());
    out << "{\n";
    out << "  \"experiment\": \"sbbf_benchmark\",\n";
    out << "  \"scenario\": \"" << scenario_name << "\",\n";
    out << "  \"distribution\": \"" << distribution << "\",\n";
    out << "  \"num_elements\": " << num_elements << ",\n";
    out << "  \"dimensions\": " << dimensions << ",\n";
    out << "  \"implementations\": [\n";

    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        out << "    {\n";
        out << "      \"name\": \"" << r.impl_name << "\",\n";
        out << "      \"sfc_type\": \"" << r.sfc_type << "\",\n";
        out << "      \"memory_bytes\": " << r.memory_bytes << ",\n";
        out << "      \"insert_ns_per_op\": " << std::fixed << std::setprecision(2)
            << r.insert_ns_per_op << ",\n";
        out << "      \"query_ns_per_op\": " << std::fixed << std::setprecision(2)
            << r.query_ns_per_op << ",\n";
        out << "      \"neighborhood_ns_per_op\": " << std::fixed << std::setprecision(2)
            << r.neighborhood_ns_per_op << ",\n";
        out << "      \"fpr\": " << std::fixed << std::setprecision(6) << r.fpr << "\n";
        out << "    }" << (i < results.size() - 1 ? ",\n" : "\n");
    }

    out << "  ]\n";
    out << "}\n";
    out.close();

    std::cout << "Results saved to: " << filename.str() << "\n";
}

void print_results_table(const std::vector<BenchmarkResult>& results) {
    std::cout << "\n" << std::left
              << std::setw(25) << "Implementation"
              << std::right
              << std::setw(10) << "Memory"
              << std::setw(12) << "Insert"
              << std::setw(12) << "Query"
              << std::setw(15) << "Neighbor"
              << std::setw(10) << "FPR" << "\n";
    std::cout << std::string(84, '-') << "\n";

    for (const auto& r : results) {
        std::cout << std::left << std::setw(25) << r.impl_name
                  << std::right
                  << std::setw(8) << (r.memory_bytes / 1024) << " KB"
                  << std::setw(10) << std::fixed << std::setprecision(1)
                  << r.insert_ns_per_op << " ns"
                  << std::setw(10) << r.query_ns_per_op << " ns"
                  << std::setw(13) << r.neighborhood_ns_per_op << " ns"
                  << std::setw(9) << std::setprecision(4) << (r.fpr * 100) << "%\n";
    }
}

// ============================================================================
// Benchmark Scenarios
// ============================================================================

void run_2d_uniform_benchmark(size_t n, uint32_t max_coord, unsigned log_blocks) {
    std::cout << "\n========================================\n";
    std::cout << "2D Uniform Benchmark (" << n << " elements)\n";
    std::cout << "========================================\n";

    auto insert_data = generate_uniform_2d(n, max_coord, 42);
    auto query_data = generate_uniform_2d(n, max_coord, 123);  // Different seed for FPR

    std::vector<BenchmarkResult> results;

    // SBBF Morton 2D
    std::cout << "  Testing SBBF (Morton 2D)...\n";
    results.push_back(benchmark_sbbf_2d<16>("SBBF Morton2D", SFCType::MORTON_2D,
                                             insert_data, query_data, log_blocks, 4));

    // SBBF Hilbert 2D
    std::cout << "  Testing SBBF (Hilbert 2D)...\n";
    results.push_back(benchmark_sbbf_2d<16>("SBBF Hilbert2D", SFCType::HILBERT_2D,
                                             insert_data, query_data, log_blocks, 4));

    // BlockedBloomFilter baseline (use same expected items, target 1% FPR)
    std::cout << "  Testing BlockedBloomFilter...\n";
    results.push_back(benchmark_blocked_bf_2d(insert_data, query_data, n, 0.01));

    // RegisterBlockedBF baseline
    std::cout << "  Testing RegisterBlockedBF...\n";
    results.push_back(benchmark_register_bf_2d(insert_data, query_data, n, 0.01));

    print_results_table(results);

    std::stringstream scenario;
    scenario << "2d_uniform_" << n;
    save_results_json(results, scenario.str(), "uniform", n, 2);
}

void run_2d_clustered_benchmark(size_t n, uint32_t max_coord, size_t clusters,
                                 unsigned log_blocks) {
    std::cout << "\n========================================\n";
    std::cout << "2D Clustered Benchmark (" << n << " elements, " << clusters << " clusters)\n";
    std::cout << "========================================\n";

    auto insert_data = generate_clustered_2d(n, max_coord, clusters, 42);
    auto query_data = generate_uniform_2d(n, max_coord, 123);

    std::vector<BenchmarkResult> results;

    std::cout << "  Testing SBBF (Morton 2D)...\n";
    results.push_back(benchmark_sbbf_2d<16>("SBBF Morton2D", SFCType::MORTON_2D,
                                             insert_data, query_data, log_blocks, 4));

    std::cout << "  Testing SBBF (Hilbert 2D)...\n";
    results.push_back(benchmark_sbbf_2d<16>("SBBF Hilbert2D", SFCType::HILBERT_2D,
                                             insert_data, query_data, log_blocks, 4));

    std::cout << "  Testing BlockedBloomFilter...\n";
    results.push_back(benchmark_blocked_bf_2d(insert_data, query_data, n, 0.01));

    std::cout << "  Testing RegisterBlockedBF...\n";
    results.push_back(benchmark_register_bf_2d(insert_data, query_data, n, 0.01));

    print_results_table(results);

    std::stringstream scenario;
    scenario << "2d_clustered_" << n << "_c" << clusters;
    save_results_json(results, scenario.str(), "clustered", n, 2);
}

void run_3d_uniform_benchmark(size_t n, uint32_t max_coord, unsigned log_blocks) {
    std::cout << "\n========================================\n";
    std::cout << "3D Uniform Benchmark (" << n << " elements)\n";
    std::cout << "========================================\n";

    auto insert_data = generate_uniform_3d(n, max_coord, 42);
    auto query_data = generate_uniform_3d(n, max_coord, 123);

    std::vector<BenchmarkResult> results;

    std::cout << "  Testing SBBF (Morton 3D)...\n";
    results.push_back(benchmark_sbbf_3d<10>("SBBF Morton3D", SFCType::MORTON_3D,
                                             insert_data, query_data, log_blocks, 4));

    std::cout << "  Testing SBBF (Hilbert 3D)...\n";
    results.push_back(benchmark_sbbf_3d<10>("SBBF Hilbert3D", SFCType::HILBERT_3D,
                                             insert_data, query_data, log_blocks, 4));

    print_results_table(results);

    std::stringstream scenario;
    scenario << "3d_uniform_" << n;
    save_results_json(results, scenario.str(), "uniform", n, 3);
}

void run_3d_clustered_benchmark(size_t n, uint32_t max_coord, size_t clusters,
                                 unsigned log_blocks) {
    std::cout << "\n========================================\n";
    std::cout << "3D Clustered Benchmark (" << n << " elements, " << clusters << " clusters)\n";
    std::cout << "========================================\n";

    auto insert_data = generate_clustered_3d(n, max_coord, clusters, 42);
    auto query_data = generate_uniform_3d(n, max_coord, 123);

    std::vector<BenchmarkResult> results;

    std::cout << "  Testing SBBF (Morton 3D)...\n";
    results.push_back(benchmark_sbbf_3d<10>("SBBF Morton3D", SFCType::MORTON_3D,
                                             insert_data, query_data, log_blocks, 4));

    std::cout << "  Testing SBBF (Hilbert 3D)...\n";
    results.push_back(benchmark_sbbf_3d<10>("SBBF Hilbert3D", SFCType::HILBERT_3D,
                                             insert_data, query_data, log_blocks, 4));

    print_results_table(results);

    std::stringstream scenario;
    scenario << "3d_clustered_" << n << "_c" << clusters;
    save_results_json(results, scenario.str(), "clustered", n, 3);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    // Create results directory
    mkdir(results_path.c_str(), 0755);

    std::cout << "\n";
    std::cout << "=================================================\n";
    std::cout << "  Spatial-Blocked Bloom Filter Benchmark Suite\n";
    std::cout << "=================================================\n";

    // 2D Benchmarks
    run_2d_uniform_benchmark(10000, 65535, 14);      // 10K, 1MB
    run_2d_uniform_benchmark(100000, 65535, 17);     // 100K, 8MB
    run_2d_clustered_benchmark(100000, 65535, 100, 17);  // 100K, 100 clusters

    // 3D Benchmarks
    run_3d_uniform_benchmark(100000, 1023, 17);      // 100K, 10-bit coords
    run_3d_clustered_benchmark(100000, 1023, 50, 17);  // 100K, 50 clusters

    std::cout << "\n=================================================\n";
    std::cout << "  Benchmark Complete!\n";
    std::cout << "  Results saved to: " << results_path << "\n";
    std::cout << "=================================================\n\n";

    return 0;
}
