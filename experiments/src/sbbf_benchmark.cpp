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

using namespace sbbf;
using namespace globimap;

const std::string results_path = "./results/sbbf/";

// ============================================================================
// Configuration
// ============================================================================

struct BenchmarkConfig {
    size_t epochs = 11;
    size_t min_iters = 1000;
    size_t warmup = 100;
    size_t epoch_time_ms = 100;
    std::string scenario = "all";

    void print() const {
        std::cout << "Benchmark configuration:\n";
        std::cout << "  Epochs: " << epochs << "\n";
        std::cout << "  Min iterations/epoch: " << min_iters << "\n";
        std::cout << "  Warmup iterations: " << warmup << "\n";
        std::cout << "  Max epoch time: " << epoch_time_ms << " ms\n";
        std::cout << "  Scenario: " << scenario << "\n";
    }
};

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --epochs N        Number of epochs per benchmark (default: 11)\n";
    std::cout << "  --min-iters N     Minimum iterations per epoch (default: 1000)\n";
    std::cout << "  --warmup N        Warmup iterations (default: 100)\n";
    std::cout << "  --epoch-time MS   Max time per epoch in ms (default: 100)\n";
    std::cout << "  --scenario S      Run only: 2d, 3d, all (default: all)\n";
    std::cout << "  --help            Show this help\n";
}

BenchmarkConfig parse_args(int argc, char* argv[]) {
    BenchmarkConfig config;

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
        } else {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            print_usage(argv[0]);
            exit(1);
        }
    }

    return config;
}

BenchmarkConfig g_config;

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

// ============================================================================
// Benchmark Functions
// ============================================================================

// Result structure for summary table
struct BenchResult {
    std::string name;
    double insert_ns;
    double query_ns;
    double neighbor_ns;
    uint64_t insert_ins;
    uint64_t query_ins;
    uint64_t insert_cyc;
    uint64_t query_cyc;
    double fpr;
    uint64_t memory;
};

std::vector<BenchResult> g_results;

template<unsigned SFCBits>
void benchmark_sbbf_2d(ankerl::nanobench::Bench& bench,
                       const std::string& name,
                       SFCType sfc_type,
                       const std::vector<Point2D>& insert_data,
                       const std::vector<Point2D>& query_data,
                       unsigned log_blocks, unsigned hash_k) {
    // Create filter
    SBBFConfig conf;
    conf.sfc_type = sfc_type;
    conf.sfc_bits = SFCBits;
    conf.log_num_blocks = log_blocks;
    conf.hash_k = hash_k;
    conf.bits_per_block = 64;

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
                       unsigned log_blocks, unsigned hash_k) {
    SBBFConfig conf;
    conf.sfc_type = sfc_type;
    conf.sfc_bits = SFCBits;
    conf.log_num_blocks = log_blocks;
    conf.hash_k = hash_k;
    conf.bits_per_block = 64;

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

    // FPR
    size_t false_positives = 0;
    for (const auto& p : query_data) {
        if (filter.get_bool_3D(p.x, p.y, p.z)) ++false_positives;
    }
    result.fpr = static_cast<double>(false_positives) / query_data.size();

    g_results.push_back(result);
}

void benchmark_blocked_bf_2d(ankerl::nanobench::Bench& bench,
                              const std::vector<Point2D>& insert_data,
                              const std::vector<Point2D>& query_data,
                              size_t expected_items, double target_fpr) {
    BlockedBFConfig conf{expected_items, target_fpr};
    BlockedBloomFilter filter(conf);
    BenchResult result;
    result.name = "BlockedBF";
    result.memory = filter.memory_usage();

    // Insert
    size_t insert_idx = 0;
    bench.run("BlockedBF insert", [&]() {
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
    bench.run("BlockedBF query", [&]() {
        const auto& p = insert_data[query_idx++ % insert_data.size()];
        ankerl::nanobench::doNotOptimizeAway(
            filter.get_bool({static_cast<uint64_t>(p.x), static_cast<uint64_t>(p.y)}));
    });
    auto& query_res = bench.results().back();
    result.query_ns = query_res.median(ankerl::nanobench::Result::Measure::elapsed) * 1e9;
    result.query_ins = static_cast<uint64_t>(query_res.median(ankerl::nanobench::Result::Measure::instructions));
    result.query_cyc = static_cast<uint64_t>(query_res.median(ankerl::nanobench::Result::Measure::cpucycles));

    // Neighborhood (3x3 = 9 queries)
    size_t nbr_idx = 0;
    bench.batch(9).run("BlockedBF neighbor", [&]() {
        const auto& p = insert_data[nbr_idx++ % insert_data.size()];
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                ankerl::nanobench::doNotOptimizeAway(
                    filter.get_bool({static_cast<uint64_t>(p.x + dx),
                                     static_cast<uint64_t>(p.y + dy)}));
            }
        }
    });
    bench.batch(1);
    auto& nbr_res = bench.results().back();
    result.neighbor_ns = nbr_res.median(ankerl::nanobench::Result::Measure::elapsed) * 1e9;

    // FPR
    size_t false_positives = 0;
    for (const auto& p : query_data) {
        if (filter.get_bool({static_cast<uint64_t>(p.x), static_cast<uint64_t>(p.y)}))
            ++false_positives;
    }
    result.fpr = static_cast<double>(false_positives) / query_data.size();

    g_results.push_back(result);
}

void benchmark_register_bf_2d(ankerl::nanobench::Bench& bench,
                               const std::vector<Point2D>& insert_data,
                               const std::vector<Point2D>& query_data,
                               size_t expected_items, double target_fpr) {
    RegisterBlockedBFConfig conf{expected_items, target_fpr, 0};
    RegisterBlockedBloomFilter<0> filter(conf);
    BenchResult result;
    result.name = "RegisterBF";
    result.memory = filter.memory_usage();

    // Insert
    size_t insert_idx = 0;
    bench.run("RegisterBF insert", [&]() {
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
    bench.run("RegisterBF query", [&]() {
        const auto& p = insert_data[query_idx++ % insert_data.size()];
        ankerl::nanobench::doNotOptimizeAway(
            filter.get_bool({static_cast<uint64_t>(p.x), static_cast<uint64_t>(p.y)}));
    });
    auto& query_res = bench.results().back();
    result.query_ns = query_res.median(ankerl::nanobench::Result::Measure::elapsed) * 1e9;
    result.query_ins = static_cast<uint64_t>(query_res.median(ankerl::nanobench::Result::Measure::instructions));
    result.query_cyc = static_cast<uint64_t>(query_res.median(ankerl::nanobench::Result::Measure::cpucycles));

    // Neighborhood
    size_t nbr_idx = 0;
    bench.batch(9).run("RegisterBF neighbor", [&]() {
        const auto& p = insert_data[nbr_idx++ % insert_data.size()];
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                ankerl::nanobench::doNotOptimizeAway(
                    filter.get_bool({static_cast<uint64_t>(p.x + dx),
                                     static_cast<uint64_t>(p.y + dy)}));
            }
        }
    });
    bench.batch(1);
    auto& nbr_res = bench.results().back();
    result.neighbor_ns = nbr_res.median(ankerl::nanobench::Result::Measure::elapsed) * 1e9;

    // FPR
    size_t false_positives = 0;
    for (const auto& p : query_data) {
        if (filter.get_bool({static_cast<uint64_t>(p.x), static_cast<uint64_t>(p.y)}))
            ++false_positives;
    }
    result.fpr = static_cast<double>(false_positives) / query_data.size();

    g_results.push_back(result);
}

void benchmark_blocked_bf_3d(ankerl::nanobench::Bench& bench,
                              const std::vector<Point3D>& insert_data,
                              const std::vector<Point3D>& query_data,
                              size_t expected_items, double target_fpr) {
    BlockedBFConfig conf{expected_items, target_fpr};
    BlockedBloomFilter filter(conf);
    BenchResult result;
    result.name = "BlockedBF";
    result.memory = filter.memory_usage();

    // Insert
    size_t insert_idx = 0;
    bench.run("BlockedBF insert", [&]() {
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
    bench.run("BlockedBF query", [&]() {
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
    bench.batch(26).run("BlockedBF neighbor", [&]() {
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

void benchmark_register_bf_3d(ankerl::nanobench::Bench& bench,
                               const std::vector<Point3D>& insert_data,
                               const std::vector<Point3D>& query_data,
                               size_t expected_items, double target_fpr) {
    RegisterBlockedBFConfig conf{expected_items, target_fpr, 0};
    RegisterBlockedBloomFilter<0> filter(conf);
    BenchResult result;
    result.name = "RegisterBF";
    result.memory = filter.memory_usage();

    // Insert
    size_t insert_idx = 0;
    bench.run("RegisterBF insert", [&]() {
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
    bench.run("RegisterBF query", [&]() {
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
    bench.batch(26).run("RegisterBF neighbor", [&]() {
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

// Print summary table
void print_summary_table() {
    if (g_results.empty()) return;

    std::cout << "\n";
    std::cout << std::left << std::setw(20) << "Implementation"
              << std::right
              << std::setw(10) << "Memory"
              << std::setw(10) << "Insert"
              << std::setw(10) << "Query"
              << std::setw(12) << "Neighbor"
              << std::setw(8) << "Ins"
              << std::setw(8) << "Cyc"
              << std::setw(8) << "FPR" << "\n";
    std::cout << std::string(94, '-') << "\n";

    for (const auto& r : g_results) {
        std::cout << std::left << std::setw(20) << r.name
                  << std::right
                  << std::setw(8) << (r.memory / 1024) << " KB"
                  << std::setw(8) << std::fixed << std::setprecision(1) << r.insert_ns << " ns"
                  << std::setw(8) << r.query_ns << " ns"
                  << std::setw(10) << r.neighbor_ns << " ns"
                  << std::setw(8) << r.query_ins
                  << std::setw(8) << r.query_cyc
                  << std::setw(7) << std::setprecision(2) << (r.fpr * 100) << "%\n";
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

    // SBBF Morton 2D
    benchmark_sbbf_2d<16>(bench, "SBBF-Morton2D", SFCType::MORTON_2D,
                          insert_data, query_data, log_blocks, 4);

    // SBBF Hilbert 2D (now with LUT!)
    benchmark_sbbf_2d<16>(bench, "SBBF-Hilbert2D", SFCType::HILBERT_2D,
                          insert_data, query_data, log_blocks, 4);

    // Baseline filters
    benchmark_blocked_bf_2d(bench, insert_data, query_data, n, 0.01);
    benchmark_register_bf_2d(bench, insert_data, query_data, n, 0.01);

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

    benchmark_sbbf_2d<16>(bench, "SBBF-Morton2D", SFCType::MORTON_2D,
                          insert_data, query_data, log_blocks, 4);

    benchmark_sbbf_2d<16>(bench, "SBBF-Hilbert2D", SFCType::HILBERT_2D,
                          insert_data, query_data, log_blocks, 4);

    benchmark_blocked_bf_2d(bench, insert_data, query_data, n, 0.01);
    benchmark_register_bf_2d(bench, insert_data, query_data, n, 0.01);

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

    benchmark_sbbf_3d<10>(bench, "SBBF-Morton3D", SFCType::MORTON_3D,
                          insert_data, query_data, log_blocks, 4);

    benchmark_sbbf_3d<10>(bench, "SBBF-Hilbert3D", SFCType::HILBERT_3D,
                          insert_data, query_data, log_blocks, 4);

    std::cout << "\n--- Baseline Bloom Filters ---\n\n";

    benchmark_blocked_bf_3d(bench, insert_data, query_data, n, 0.01);
    benchmark_register_bf_3d(bench, insert_data, query_data, n, 0.01);

    print_summary_table();
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

    if (run_2d) {
        // 2D Benchmarks
        run_2d_benchmark(100000, 65535, 17);
        run_2d_clustered_benchmark(100000, 65535, 100, 17);
    }

    if (run_3d) {
        // 3D Benchmarks
        run_3d_benchmark(100000, 1023, 17);
    }

    std::cout << "\n=================================================\n";
    std::cout << "  Benchmark Complete!\n";
    std::cout << "=================================================\n\n";

    return 0;
}
