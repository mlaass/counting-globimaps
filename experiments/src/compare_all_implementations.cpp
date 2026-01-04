#include "cascade_cbf.hpp"
#include "count_min_sketch.hpp"
#include "dleft_counting_bf.hpp"
#include "spectral_bloom_filter.hpp"
#include "variable_increment_bf.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <unordered_map>
#include <vector>

using namespace globimap;

// Timing utility
class Timer {
    std::chrono::high_resolution_clock::time_point start_;

public:
    Timer() : start_(std::chrono::high_resolution_clock::now()) {}

    double elapsed_ms() const {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start_).count();
    }
};

// Generate synthetic dataset with Zipfian distribution
std::vector<std::vector<uint64_t>> generate_zipfian_dataset(
    size_t num_unique, size_t total_items, double alpha, uint64_t seed) {

    std::mt19937_64 rng(seed);
    std::vector<std::vector<uint64_t>> dataset;
    dataset.reserve(total_items);

    // Generate Zipfian probabilities
    std::vector<double> probs(num_unique);
    double sum = 0.0;
    for (size_t i = 0; i < num_unique; ++i) {
        probs[i] = 1.0 / std::pow(i + 1, alpha);
        sum += probs[i];
    }
    for (auto &p : probs) {
        p /= sum;
    }

    // Create cumulative distribution
    std::vector<double> cumulative(num_unique);
    cumulative[0] = probs[0];
    for (size_t i = 1; i < num_unique; ++i) {
        cumulative[i] = cumulative[i - 1] + probs[i];
    }

    // Generate dataset
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    for (size_t i = 0; i < total_items; ++i) {
        double r = dist(rng);
        size_t idx = 0;
        for (size_t j = 0; j < num_unique; ++j) {
            if (r <= cumulative[j]) {
                idx = j;
                break;
            }
        }
        dataset.push_back({idx, idx * 2});
    }

    return dataset;
}

// Generate uniform random dataset
std::vector<std::vector<uint64_t>> generate_uniform_dataset(
    size_t num_items, uint64_t seed) {

    std::mt19937_64 rng(seed);
    std::vector<std::vector<uint64_t>> dataset;
    dataset.reserve(num_items);

    for (size_t i = 0; i < num_items; ++i) {
        dataset.push_back({rng(), rng()});
    }

    return dataset;
}

// Compute ground truth frequencies
std::unordered_map<uint64_t, uint64_t> compute_ground_truth(
    const std::vector<std::vector<uint64_t>> &dataset) {

    std::unordered_map<uint64_t, uint64_t> truth;
    for (const auto &point : dataset) {
        truth[point[0]]++;
    }
    return truth;
}

void print_header() {
    std::cout << "\n========================================\n";
    std::cout << "Counting Bloom Filter Implementation Comparison\n";
    std::cout << "========================================\n\n";
}

void print_section(const std::string &title) {
    std::cout << "\n--- " << title << " ---\n";
}

void print_scenario_header(const std::string &title) {
    std::cout << "\n\n";
    std::cout << "╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  " << std::setw(60) << std::left << title << "║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n";
}

// ============================================================================
// TINY SCENARIO (~3 KB) - IoT/Embedded Use Case
// ============================================================================

void test_memory_tiny() {
    print_section("Memory Efficiency (Tiny Budget ~3 KB)");

    // Variable-Increment CBF
    VICBFConfig vi_conf{8, 11, 8, 4};
    VariableIncrementBloomFilter vi_cbf(vi_conf);
    uint64_t vi_mem = vi_cbf.memory_usage();

    // Spectral BF (MI variant)
    SBFConfig sbf_conf{8, 11, 8, MINIMAL_INCREMENT};
    SpectralBloomFilter sbf(sbf_conf);
    uint64_t sbf_mem = sbf.memory_usage();

    // d-Left CBF
    DLeftCBFConfig dleft_conf{128, 4, 4, 8, 8};
    DLeftCountingBloomFilter dleft(dleft_conf);
    uint64_t dleft_mem = dleft.memory_usage();

    // Count-Min Sketch
    CMSConfig cms_conf{0.01, 0.01, false, 16};
    CountMinSketch cms(cms_conf);
    uint64_t cms_mem = cms.memory_usage();

    // CascadeCBF (16-bit layer for tiny scenario)
    CCBF_16 cbf;
    cbf.configure(8, {11}, true);  // k=8, layer0=2^11, MI mode
    uint64_t cbf_mem = cbf.memory_usage();

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Variable-Increment CBF: " << config_utils::format_memory(vi_mem) << "\n";
    std::cout << "  Spectral BF (MI):       " << config_utils::format_memory(sbf_mem) << "\n";
    std::cout << "  d-Left CBF:             " << config_utils::format_memory(dleft_mem) << "\n";
    std::cout << "  Count-Min Sketch:       " << config_utils::format_memory(cms_mem) << "\n";
    std::cout << "  CascadeCBF (MI):        " << config_utils::format_memory(cbf_mem) << "\n";
}

void test_throughput_tiny() {
    print_section("Insert Throughput (10K uniform items)");

    auto dataset = generate_uniform_dataset(10000, 42);

    // Variable-Increment CBF
    {
        VICBFConfig conf{8, 11, 8, 4};
        VariableIncrementBloomFilter cbf(conf);
        Timer timer;
        for (const auto &point : dataset) {
            cbf.put(point);
        }
        double elapsed = timer.elapsed_ms();
        std::cout << "  VI-CBF:         " << std::setw(8) << std::fixed << std::setprecision(2)
                  << (10000.0 / elapsed * 1000.0) << " inserts/sec (" << elapsed << " ms)\n";
    }

    // Spectral BF
    {
        SBFConfig conf{8, 11, 8, MINIMAL_INCREMENT};
        SpectralBloomFilter sbf(conf);
        Timer timer;
        for (const auto &point : dataset) {
            sbf.put(point);
        }
        double elapsed = timer.elapsed_ms();
        std::cout << "  Spectral BF:    " << std::setw(8) << std::fixed << std::setprecision(2)
                  << (10000.0 / elapsed * 1000.0) << " inserts/sec (" << elapsed << " ms)\n";
    }

    // d-Left CBF
    {
        DLeftCBFConfig conf{128, 4, 4, 8, 8};
        DLeftCountingBloomFilter cbf(conf);
        Timer timer;
        for (const auto &point : dataset) {
            cbf.put(point);
        }
        double elapsed = timer.elapsed_ms();
        std::cout << "  d-Left CBF:     " << std::setw(8) << std::fixed << std::setprecision(2)
                  << (10000.0 / elapsed * 1000.0) << " inserts/sec (" << elapsed << " ms)\n";
    }

    // Count-Min Sketch
    {
        CMSConfig conf{0.01, 0.01, false, 16};
        CountMinSketch cms(conf);
        Timer timer;
        for (const auto &point : dataset) {
            cms.put(point);
        }
        double elapsed = timer.elapsed_ms();
        std::cout << "  Count-Min:      " << std::setw(8) << std::fixed << std::setprecision(2)
                  << (10000.0 / elapsed * 1000.0) << " inserts/sec (" << elapsed << " ms)\n";
    }

    // CascadeCBF
    {
        CCBF_16 cbf;
        cbf.configure(8, {11}, true);
        Timer timer;
        for (const auto &point : dataset) {
            cbf.put(point);
        }
        double elapsed = timer.elapsed_ms();
        std::cout << "  CascadeCBF:    " << std::setw(8) << std::fixed << std::setprecision(2)
                  << (10000.0 / elapsed * 1000.0) << " inserts/sec (" << elapsed << " ms)\n";
    }
}

void test_accuracy_tiny() {
    print_section("Query Accuracy (1K unique, 10K inserts, Zipfian α=1.5)");

    auto dataset = generate_zipfian_dataset(1000, 10000, 1.5, 42);
    auto truth = compute_ground_truth(dataset);

    // Test on top-10 most frequent items
    std::vector<std::pair<uint64_t, uint64_t>> sorted_truth(truth.begin(), truth.end());
    std::sort(sorted_truth.begin(), sorted_truth.end(),
              [](const auto &a, const auto &b) { return a.second > b.second; });

    std::vector<std::vector<uint64_t>> test_points;
    for (size_t i = 0; i < std::min<size_t>(10, sorted_truth.size()); ++i) {
        test_points.push_back({sorted_truth[i].first, sorted_truth[i].first * 2});
    }

    // Variable-Increment CBF
    {
        VICBFConfig conf{8, 11, 8, 4};
        VariableIncrementBloomFilter cbf(conf);
        for (const auto &point : dataset) {
            cbf.put(point);
        }

        double total_error = 0.0;
        for (size_t i = 0; i < test_points.size(); ++i) {
            uint64_t estimated = cbf.get_min(test_points[i]);
            uint64_t actual = sorted_truth[i].second;
            double error = std::abs((double)estimated - (double)actual) / (double)actual;
            total_error += error;
        }
        std::cout << "  VI-CBF:         Avg error = " << std::setw(8) << std::fixed
                  << std::setprecision(4) << (total_error / test_points.size() * 100.0) << "%\n";
    }

    // Spectral BF (MI variant)
    {
        SBFConfig conf{8, 11, 8, MINIMAL_INCREMENT};
        SpectralBloomFilter sbf(conf);
        for (const auto &point : dataset) {
            sbf.put(point);
        }

        double total_error = 0.0;
        for (size_t i = 0; i < test_points.size(); ++i) {
            uint64_t estimated = sbf.get_min(test_points[i]);
            uint64_t actual = sorted_truth[i].second;
            double error = std::abs((double)estimated - (double)actual) / (double)actual;
            total_error += error;
        }
        std::cout << "  Spectral (MI):  Avg error = " << std::setw(8) << std::fixed
                  << std::setprecision(4) << (total_error / test_points.size() * 100.0) << "%\n";
    }

    // d-Left CBF
    {
        DLeftCBFConfig conf{128, 4, 4, 8, 8};
        DLeftCountingBloomFilter cbf(conf);
        for (const auto &point : dataset) {
            cbf.put(point);
        }

        double total_error = 0.0;
        for (size_t i = 0; i < test_points.size(); ++i) {
            uint64_t estimated = cbf.get_min(test_points[i]);
            uint64_t actual = sorted_truth[i].second;
            double error = std::abs((double)estimated - (double)actual) / (double)actual;
            total_error += error;
        }
        std::cout << "  d-Left CBF:     Avg error = " << std::setw(8) << std::fixed
                  << std::setprecision(4) << (total_error / test_points.size() * 100.0) << "%\n";
    }

    // Count-Min Sketch
    {
        CMSConfig conf{0.01, 0.01, false, 16};
        CountMinSketch cms(conf);
        for (const auto &point : dataset) {
            cms.put(point);
        }

        double total_error = 0.0;
        for (size_t i = 0; i < test_points.size(); ++i) {
            uint64_t estimated = cms.get_min(test_points[i]);
            uint64_t actual = sorted_truth[i].second;
            double error = std::abs((double)estimated - (double)actual) / (double)actual;
            total_error += error;
        }
        std::cout << "  Count-Min:      Avg error = " << std::setw(8) << std::fixed
                  << std::setprecision(4) << (total_error / test_points.size() * 100.0) << "%\n";
    }

    // CascadeCBF
    {
        CCBF_16 cbf;
        cbf.configure(8, {11}, true);
        for (const auto &point : dataset) {
            cbf.put(point);
        }

        double total_error = 0.0;
        for (size_t i = 0; i < test_points.size(); ++i) {
            uint64_t estimated = cbf.get_min(test_points[i]);
            uint64_t actual = sorted_truth[i].second;
            double error = std::abs((double)estimated - (double)actual) / (double)actual;
            total_error += error;
        }
        std::cout << "  CascadeCBF:    Avg error = " << std::setw(8) << std::fixed
                  << std::setprecision(4) << (total_error / test_points.size() * 100.0) << "%\n";
    }
}

// ============================================================================
// MEDIUM SCENARIO (~96 KB) - General Use Case
// ============================================================================

void test_memory_medium() {
    print_section("Memory Efficiency (Medium Budget ~96 KB)");

    // Variable-Increment CBF (Fair: ~128 KB to match 96 KB budget)
    VICBFConfig vi_conf{8, 16, 16, 4};  // hash_k=8, logsize=16 (was 20), counter_bits=16, increment_L=4
    VariableIncrementBloomFilter vi_cbf(vi_conf);
    uint64_t vi_mem = vi_cbf.memory_usage();

    // Spectral BF (MS variant) (Fair: ~128 KB to match 96 KB budget)
    SBFConfig sbf_conf{8, 16, 16, MINIMAL_INCREMENT};  // logsize=16 (was 20)
    SpectralBloomFilter sbf(sbf_conf);
    uint64_t sbf_mem = sbf.memory_usage();

    // d-Left CBF (Fair: ~95 KB to match 96 KB budget)
    DLeftCBFConfig dleft_conf{4864, 4, 4, 12, 16};  // buckets=4864 (was 2048)
    DLeftCountingBloomFilter dleft(dleft_conf);
    uint64_t dleft_mem = dleft.memory_usage();

    // Count-Min Sketch (Fair: ~88.5 KB to match 96 KB budget)
    CMSConfig cms_conf{0.0003, 0.01, false, 16};  // epsilon=0.0003 (was 0.01) for equal memory
    CountMinSketch cms(cms_conf);
    uint64_t cms_mem = cms.memory_usage();

    // CascadeCBF (16+16 bit for medium scenario)
    CCBF_16_16 cbf;
    cbf.configure(8, {16, 14}, true);  // k=8, layer0=2^16, layer1=2^14, MI mode
    uint64_t cbf_mem = cbf.memory_usage();

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Variable-Increment CBF: " << config_utils::format_memory(vi_mem) << "\n";
    std::cout << "  Spectral BF (MI):       " << config_utils::format_memory(sbf_mem) << "\n";
    std::cout << "  d-Left CBF:             " << config_utils::format_memory(dleft_mem) << "\n";
    std::cout << "  Count-Min Sketch:       " << config_utils::format_memory(cms_mem) << "\n";
    std::cout << "  CascadeCBF (MI):        " << config_utils::format_memory(cbf_mem) << "\n";
}

void test_throughput_medium() {
    print_section("Insert Throughput (100K uniform items)");

    auto dataset = generate_uniform_dataset(100000, 42);

    // Variable-Increment CBF (Fair: ~128 KB)
    {
        VICBFConfig conf{8, 16, 16, 4};  // logsize=16 (was 20) for fair comparison
        VariableIncrementBloomFilter cbf(conf);
        Timer timer;
        for (const auto &point : dataset) {
            cbf.put(point);
        }
        double elapsed = timer.elapsed_ms();
        std::cout << "  VI-CBF:         " << std::setw(8) << std::fixed << std::setprecision(2)
                  << (100000.0 / elapsed * 1000.0) << " inserts/sec (" << elapsed << " ms)\n";
    }

    // Spectral BF (Fair: ~128 KB)
    {
        SBFConfig conf{8, 16, 16, MINIMUM_SELECTION};  // logsize=16 (was 20) for fair comparison
        SpectralBloomFilter sbf(conf);
        Timer timer;
        for (const auto &point : dataset) {
            sbf.put(point);
        }
        double elapsed = timer.elapsed_ms();
        std::cout << "  Spectral BF:    " << std::setw(8) << std::fixed << std::setprecision(2)
                  << (100000.0 / elapsed * 1000.0) << " inserts/sec (" << elapsed << " ms)\n";
    }

    // d-Left CBF (Fair: ~95 KB)
    {
        DLeftCBFConfig conf{4864, 4, 4, 12, 16};  // buckets=4864 (was 2048) for fair comparison
        DLeftCountingBloomFilter cbf(conf);
        Timer timer;
        for (const auto &point : dataset) {
            cbf.put(point);
        }
        double elapsed = timer.elapsed_ms();
        std::cout << "  d-Left CBF:     " << std::setw(8) << std::fixed << std::setprecision(2)
                  << (100000.0 / elapsed * 1000.0) << " inserts/sec (" << elapsed << " ms)\n";
    }

    // Count-Min Sketch (Fair: ~88.5 KB)
    {
        CMSConfig conf{0.0003, 0.01, false, 16};  // epsilon=0.0003 (was 0.01) for fair comparison
        CountMinSketch cms(conf);
        Timer timer;
        for (const auto &point : dataset) {
            cms.put(point);
        }
        double elapsed = timer.elapsed_ms();
        std::cout << "  Count-Min:      " << std::setw(8) << std::fixed << std::setprecision(2)
                  << (100000.0 / elapsed * 1000.0) << " inserts/sec (" << elapsed << " ms)\n";
    }

    // CascadeCBF
    {
        CCBF_16_16 cbf;
        cbf.configure(8, {16, 14}, true);
        Timer timer;
        for (const auto &point : dataset) {
            cbf.put(point);
        }
        double elapsed = timer.elapsed_ms();
        std::cout << "  CascadeCBF:     " << std::setw(8) << std::fixed << std::setprecision(2)
                  << (100000.0 / elapsed * 1000.0) << " inserts/sec (" << elapsed << " ms)\n";
    }
}

void test_accuracy_medium() {
    print_section("Query Accuracy (100K unique, 100K inserts, Zipfian α=1.5)");

    auto dataset = generate_zipfian_dataset(100000, 100000, 1.5, 42);
    auto truth = compute_ground_truth(dataset);

    // Test on top-10 most frequent items
    std::vector<std::pair<uint64_t, uint64_t>> sorted_truth(truth.begin(), truth.end());
    std::sort(sorted_truth.begin(), sorted_truth.end(),
              [](const auto &a, const auto &b) { return a.second > b.second; });

    std::vector<std::vector<uint64_t>> test_points;
    for (size_t i = 0; i < std::min<size_t>(10, sorted_truth.size()); ++i) {
        test_points.push_back({sorted_truth[i].first, sorted_truth[i].first * 2});
    }

    // Variable-Increment CBF (Fair: ~128 KB)
    {
        VICBFConfig conf{8, 16, 16, 4};  // logsize=16 (was 18) for fair comparison
        VariableIncrementBloomFilter cbf(conf);
        for (const auto &point : dataset) {
            cbf.put(point);
        }

        double total_error = 0.0;
        for (size_t i = 0; i < test_points.size(); ++i) {
            uint64_t estimated = cbf.get_min(test_points[i]);
            uint64_t actual = sorted_truth[i].second;
            double error = std::abs((double)estimated - (double)actual) / (double)actual;
            total_error += error;
        }
        std::cout << "  VI-CBF:         Avg error = " << std::setw(8) << std::fixed
                  << std::setprecision(4) << (total_error / test_points.size() * 100.0) << "%\n";
    }

    // Spectral BF (MI variant for better accuracy) (Fair: ~128 KB)
    {
        SBFConfig conf{8, 16, 16, MINIMAL_INCREMENT};  // logsize=16 (was 18) for fair comparison
        SpectralBloomFilter sbf(conf);
        for (const auto &point : dataset) {
            sbf.put(point);
        }

        double total_error = 0.0;
        for (size_t i = 0; i < test_points.size(); ++i) {
            uint64_t estimated = sbf.get_min(test_points[i]);
            uint64_t actual = sorted_truth[i].second;
            double error = std::abs((double)estimated - (double)actual) / (double)actual;
            total_error += error;
        }
        std::cout << "  Spectral (MI):  Avg error = " << std::setw(8) << std::fixed
                  << std::setprecision(4) << (total_error / test_points.size() * 100.0) << "%\n";
    }

    // d-Left CBF (Fair: ~95 KB)
    {
        DLeftCBFConfig conf{4864, 4, 4, 12, 16};  // buckets=4864 (was 2048) for fair comparison
        DLeftCountingBloomFilter cbf(conf);
        for (const auto &point : dataset) {
            cbf.put(point);
        }

        double total_error = 0.0;
        for (size_t i = 0; i < test_points.size(); ++i) {
            uint64_t estimated = cbf.get_min(test_points[i]);
            uint64_t actual = sorted_truth[i].second;
            double error = std::abs((double)estimated - (double)actual) / (double)actual;
            total_error += error;
        }
        std::cout << "  d-Left CBF:     Avg error = " << std::setw(8) << std::fixed
                  << std::setprecision(4) << (total_error / test_points.size() * 100.0) << "%\n";
    }

    // Count-Min Sketch (Fair: ~88.5 KB)
    {
        CMSConfig conf{0.0003, 0.01, false, 16};  // epsilon=0.0003 (was 0.01) for fair comparison
        CountMinSketch cms(conf);
        for (const auto &point : dataset) {
            cms.put(point);
        }

        double total_error = 0.0;
        for (size_t i = 0; i < test_points.size(); ++i) {
            uint64_t estimated = cms.get_min(test_points[i]);
            uint64_t actual = sorted_truth[i].second;
            double error = std::abs((double)estimated - (double)actual) / (double)actual;
            total_error += error;
        }
        std::cout << "  Count-Min:      Avg error = " << std::setw(8) << std::fixed
                  << std::setprecision(4) << (total_error / test_points.size() * 100.0) << "%\n";
    }

    // CascadeCBF
    {
        CCBF_16_16 cbf;
        cbf.configure(8, {16, 14}, true);
        for (const auto &point : dataset) {
            cbf.put(point);
        }

        double total_error = 0.0;
        for (size_t i = 0; i < test_points.size(); ++i) {
            uint64_t estimated = cbf.get_min(test_points[i]);
            uint64_t actual = sorted_truth[i].second;
            double error = std::abs((double)estimated - (double)actual) / (double)actual;
            total_error += error;
        }
        std::cout << "  CascadeCBF:    Avg error = " << std::setw(8) << std::fixed
                  << std::setprecision(4) << (total_error / test_points.size() * 100.0) << "%\n";
    }
}

// ============================================================================
// LARGE SCENARIO (~2 MB) - High Accuracy Use Case
// ============================================================================

void test_memory_large() {
    print_section("Memory Efficiency (Large Budget ~2 MB)");

    // Variable-Increment CBF
    VICBFConfig vi_conf{8, 20, 16, 4};
    VariableIncrementBloomFilter vi_cbf(vi_conf);
    uint64_t vi_mem = vi_cbf.memory_usage();

    // Spectral BF (MI variant)
    SBFConfig sbf_conf{8, 20, 16, MINIMAL_INCREMENT};
    SpectralBloomFilter sbf(sbf_conf);
    uint64_t sbf_mem = sbf.memory_usage();

    // d-Left CBF
    DLeftCBFConfig dleft_conf{102400, 4, 4, 12, 16};
    DLeftCountingBloomFilter dleft(dleft_conf);
    uint64_t dleft_mem = dleft.memory_usage();

    // Count-Min Sketch
    CMSConfig cms_conf{0.00001, 0.001, false, 16};
    CountMinSketch cms(cms_conf);
    uint64_t cms_mem = cms.memory_usage();

    // CascadeCBF (16+16 bit for large scenario)
    CCBF_16_16 cbf;
    cbf.configure(8, {20, 18}, true);  // k=8, layer0=2^20, layer1=2^18, MI mode
    uint64_t cbf_mem = cbf.memory_usage();

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Variable-Increment CBF: " << config_utils::format_memory(vi_mem) << "\n";
    std::cout << "  Spectral BF (MI):       " << config_utils::format_memory(sbf_mem) << "\n";
    std::cout << "  d-Left CBF:             " << config_utils::format_memory(dleft_mem) << "\n";
    std::cout << "  Count-Min Sketch:       " << config_utils::format_memory(cms_mem) << "\n";
    std::cout << "  CascadeCBF (MI):        " << config_utils::format_memory(cbf_mem) << "\n";
}

void test_throughput_large() {
    print_section("Insert Throughput (1M uniform items)");

    auto dataset = generate_uniform_dataset(1000000, 42);

    // Variable-Increment CBF
    {
        VICBFConfig conf{8, 20, 16, 4};
        VariableIncrementBloomFilter cbf(conf);
        Timer timer;
        for (const auto &point : dataset) {
            cbf.put(point);
        }
        double elapsed = timer.elapsed_ms();
        std::cout << "  VI-CBF:         " << std::setw(8) << std::fixed << std::setprecision(2)
                  << (1000000.0 / elapsed * 1000.0) << " inserts/sec (" << elapsed << " ms)\n";
    }

    // Spectral BF
    {
        SBFConfig conf{8, 20, 16, MINIMAL_INCREMENT};
        SpectralBloomFilter sbf(conf);
        Timer timer;
        for (const auto &point : dataset) {
            sbf.put(point);
        }
        double elapsed = timer.elapsed_ms();
        std::cout << "  Spectral BF:    " << std::setw(8) << std::fixed << std::setprecision(2)
                  << (1000000.0 / elapsed * 1000.0) << " inserts/sec (" << elapsed << " ms)\n";
    }

    // d-Left CBF
    {
        DLeftCBFConfig conf{102400, 4, 4, 12, 16};
        DLeftCountingBloomFilter cbf(conf);
        Timer timer;
        for (const auto &point : dataset) {
            cbf.put(point);
        }
        double elapsed = timer.elapsed_ms();
        std::cout << "  d-Left CBF:     " << std::setw(8) << std::fixed << std::setprecision(2)
                  << (1000000.0 / elapsed * 1000.0) << " inserts/sec (" << elapsed << " ms)\n";
    }

    // Count-Min Sketch
    {
        CMSConfig conf{0.00001, 0.001, false, 16};
        CountMinSketch cms(conf);
        Timer timer;
        for (const auto &point : dataset) {
            cms.put(point);
        }
        double elapsed = timer.elapsed_ms();
        std::cout << "  Count-Min:      " << std::setw(8) << std::fixed << std::setprecision(2)
                  << (1000000.0 / elapsed * 1000.0) << " inserts/sec (" << elapsed << " ms)\n";
    }

    // CascadeCBF
    {
        CCBF_16_16 cbf;
        cbf.configure(8, {20, 18}, true);
        Timer timer;
        for (const auto &point : dataset) {
            cbf.put(point);
        }
        double elapsed = timer.elapsed_ms();
        std::cout << "  CascadeCBF:     " << std::setw(8) << std::fixed << std::setprecision(2)
                  << (1000000.0 / elapsed * 1000.0) << " inserts/sec (" << elapsed << " ms)\n";
    }
}

void test_accuracy_large() {
    print_section("Query Accuracy (100K unique, 1M inserts, Zipfian α=1.5)");

    auto dataset = generate_zipfian_dataset(100000, 1000000, 1.5, 42);
    auto truth = compute_ground_truth(dataset);

    // Test on top-10 most frequent items
    std::vector<std::pair<uint64_t, uint64_t>> sorted_truth(truth.begin(), truth.end());
    std::sort(sorted_truth.begin(), sorted_truth.end(),
              [](const auto &a, const auto &b) { return a.second > b.second; });

    std::vector<std::vector<uint64_t>> test_points;
    for (size_t i = 0; i < std::min<size_t>(10, sorted_truth.size()); ++i) {
        test_points.push_back({sorted_truth[i].first, sorted_truth[i].first * 2});
    }

    // Variable-Increment CBF
    {
        VICBFConfig conf{8, 20, 16, 4};
        VariableIncrementBloomFilter cbf(conf);
        for (const auto &point : dataset) {
            cbf.put(point);
        }

        double total_error = 0.0;
        for (size_t i = 0; i < test_points.size(); ++i) {
            uint64_t estimated = cbf.get_min(test_points[i]);
            uint64_t actual = sorted_truth[i].second;
            double error = std::abs((double)estimated - (double)actual) / (double)actual;
            total_error += error;
        }
        std::cout << "  VI-CBF:         Avg error = " << std::setw(8) << std::fixed
                  << std::setprecision(4) << (total_error / test_points.size() * 100.0) << "%\n";
    }

    // Spectral BF (MI variant)
    {
        SBFConfig conf{8, 20, 16, MINIMAL_INCREMENT};
        SpectralBloomFilter sbf(conf);
        for (const auto &point : dataset) {
            sbf.put(point);
        }

        double total_error = 0.0;
        for (size_t i = 0; i < test_points.size(); ++i) {
            uint64_t estimated = sbf.get_min(test_points[i]);
            uint64_t actual = sorted_truth[i].second;
            double error = std::abs((double)estimated - (double)actual) / (double)actual;
            total_error += error;
        }
        std::cout << "  Spectral (MI):  Avg error = " << std::setw(8) << std::fixed
                  << std::setprecision(4) << (total_error / test_points.size() * 100.0) << "%\n";
    }

    // d-Left CBF
    {
        DLeftCBFConfig conf{102400, 4, 4, 12, 16};
        DLeftCountingBloomFilter cbf(conf);
        for (const auto &point : dataset) {
            cbf.put(point);
        }

        double total_error = 0.0;
        for (size_t i = 0; i < test_points.size(); ++i) {
            uint64_t estimated = cbf.get_min(test_points[i]);
            uint64_t actual = sorted_truth[i].second;
            double error = std::abs((double)estimated - (double)actual) / (double)actual;
            total_error += error;
        }
        std::cout << "  d-Left CBF:     Avg error = " << std::setw(8) << std::fixed
                  << std::setprecision(4) << (total_error / test_points.size() * 100.0) << "%\n";
    }

    // Count-Min Sketch
    {
        CMSConfig conf{0.00001, 0.001, false, 16};
        CountMinSketch cms(conf);
        for (const auto &point : dataset) {
            cms.put(point);
        }

        double total_error = 0.0;
        for (size_t i = 0; i < test_points.size(); ++i) {
            uint64_t estimated = cms.get_min(test_points[i]);
            uint64_t actual = sorted_truth[i].second;
            double error = std::abs((double)estimated - (double)actual) / (double)actual;
            total_error += error;
        }
        std::cout << "  Count-Min:      Avg error = " << std::setw(8) << std::fixed
                  << std::setprecision(4) << (total_error / test_points.size() * 100.0) << "%\n";
    }

    // CascadeCBF
    {
        CCBF_16_16 cbf;
        cbf.configure(8, {20, 18}, true);
        for (const auto &point : dataset) {
            cbf.put(point);
        }

        double total_error = 0.0;
        for (size_t i = 0; i < test_points.size(); ++i) {
            uint64_t estimated = cbf.get_min(test_points[i]);
            uint64_t actual = sorted_truth[i].second;
            double error = std::abs((double)estimated - (double)actual) / (double)actual;
            total_error += error;
        }
        std::cout << "  CascadeCBF:    Avg error = " << std::setw(8) << std::fixed
                  << std::setprecision(4) << (total_error / test_points.size() * 100.0) << "%\n";
    }
}

// ============================================================================
// Feature Comparison Summary
// ============================================================================

void print_summary_table() {
    print_section("Feature Comparison");

    std::cout << "  Implementation      | Deletion | Conservative | Cache-Opt | Error Bounds\n";
    std::cout << "  --------------------|----------|--------------|-----------|-------------\n";
    std::cout << "  VI-CBF              |    No    |      No      |    No     |     No\n";
    std::cout << "  Spectral BF (MS)    |    No    |      No      |    No     |     No\n";
    std::cout << "  Spectral BF (MI)    |    No    |     Yes      |    No     |     No\n";
    std::cout << "  Spectral BF (RM)    |   Yes    |     Yes      |    No     |     No\n";
    std::cout << "  d-Left CBF          |   Yes    |      No      |   Yes     |     No\n";
    std::cout << "  Count-Min Sketch    |    No    |   Optional   |    No     |    Yes\n";
    std::cout << "  CascadeCBF (MI)     |    No    |     Yes      |    No     |     No\n";
}

int main() {
    print_header();

    // TINY SCENARIO (~3 KB) - IoT/Embedded Use Case
    print_scenario_header("SCENARIO 1: TINY BUDGET (~3 KB) - IoT/Embedded");
    test_memory_tiny();
    test_throughput_tiny();
    test_accuracy_tiny();

    // MEDIUM SCENARIO (~96 KB) - General Use Case
    print_scenario_header("SCENARIO 2: MEDIUM BUDGET (~96 KB) - General Use");
    test_memory_medium();
    test_throughput_medium();
    test_accuracy_medium();

    // LARGE SCENARIO (~2 MB) - High Accuracy Use Case
    print_scenario_header("SCENARIO 3: LARGE BUDGET (~2 MB) - High Accuracy");
    test_memory_large();
    test_throughput_large();
    test_accuracy_large();

    // Feature comparison table
    print_summary_table();

    std::cout << "\n========================================\n";
    std::cout << "Comparison Complete\n";
    std::cout << "========================================\n\n";

    return 0;
}
