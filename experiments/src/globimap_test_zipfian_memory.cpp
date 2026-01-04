#include "cascade_cbf.hpp"
#include "spectral_bloom_filter.hpp"
#include "count_min_sketch.hpp"
#include "dleft_counting_bf.hpp"
#include "variable_increment_bf.hpp"

#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <map>
#include <random>
#include <string>
#include <vector>
#include <sys/stat.h>

using namespace globimap;

const std::string experiments_path = "./results/zipfian_memory/";

struct ZipfianMemoryResult {
    std::string impl_name;
    double alpha;
    uint64_t memory_bytes;
    double insert_time_sec;
    double query_time_sec;
    double mean_error_pct;
    double max_error_pct;
    uint64_t num_inserts;
    uint64_t num_queries;
};

// Generate Zipfian dataset with ground truth
std::pair<std::vector<std::vector<uint64_t>>, std::map<uint64_t, uint64_t>>
generate_zipfian_data(size_t num_unique, size_t total_items, double alpha, uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::vector<std::vector<uint64_t>> dataset;
    dataset.reserve(total_items);

    // Compute Zipfian probabilities
    std::vector<double> probs(num_unique);
    double sum = 0.0;
    for (size_t i = 0; i < num_unique; ++i) {
        probs[i] = 1.0 / std::pow(i + 1, alpha);
        sum += probs[i];
    }
    for (auto& p : probs) p /= sum;

    // Cumulative distribution
    std::vector<double> cumulative(num_unique);
    cumulative[0] = probs[0];
    for (size_t i = 1; i < num_unique; ++i) {
        cumulative[i] = cumulative[i - 1] + probs[i];
    }

    // Generate items
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    std::map<uint64_t, uint64_t> ground_truth;

    for (size_t i = 0; i < total_items; ++i) {
        double r = dist(rng);
        size_t idx = 0;
        for (size_t j = 0; j < num_unique; ++j) {
            if (r <= cumulative[j]) { idx = j; break; }
        }
        dataset.push_back({idx, idx * 2});
        ground_truth[idx]++;
    }

    return {dataset, ground_truth};
}

template<typename Filter>
ZipfianMemoryResult benchmark_memory(Filter& filter, const std::string& impl_name,
                                      double alpha,
                                      const std::vector<std::vector<uint64_t>>& data,
                                      const std::map<uint64_t, uint64_t>& ground_truth,
                                      uint64_t memory_bytes) {
    using std::chrono::duration;
    using std::chrono::high_resolution_clock;

    ZipfianMemoryResult result;
    result.impl_name = impl_name;
    result.alpha = alpha;
    result.memory_bytes = memory_bytes;
    result.num_inserts = data.size();
    result.num_queries = ground_truth.size();

    // Insert phase
    auto t1 = high_resolution_clock::now();
    for (const auto& point : data) {
        filter.put(point);
    }
    auto t2 = high_resolution_clock::now();
    result.insert_time_sec = duration<double>(t2 - t1).count();

    // Query phase - measure accuracy
    double total_error = 0.0;
    double max_error = 0.0;
    auto t3 = high_resolution_clock::now();

    for (const auto& [idx, actual_count] : ground_truth) {
        std::vector<uint64_t> query_point = {idx, idx * 2};
        uint64_t estimated = filter.get_min(query_point);
        double error_pct = 100.0 * std::abs((double)estimated - actual_count) / actual_count;
        total_error += error_pct;
        max_error = std::max(max_error, error_pct);
    }

    auto t4 = high_resolution_clock::now();
    result.query_time_sec = duration<double>(t4 - t3).count();
    result.mean_error_pct = total_error / ground_truth.size();
    result.max_error_pct = max_error;

    return result;
}

void save_results(const std::vector<ZipfianMemoryResult>& results,
                  size_t num_unique, size_t total_items,
                  const std::vector<double>& alphas) {
    std::string filename = experiments_path + "compare_zipfian_memory.json";

    std::ofstream out(filename);
    if (!out.is_open()) {
        std::cerr << "Error: Could not open " << filename << " for writing\n";
        return;
    }

    out << "{\n";
    out << "  \"experiment\": \"zipfian_memory_efficiency\",\n";
    out << "  \"parameters\": {\n";
    out << "    \"k\": 8,\n";
    out << "    \"num_unique\": " << num_unique << ",\n";
    out << "    \"total_inserts\": " << total_items << ",\n";
    out << "    \"alphas\": [";
    for (size_t i = 0; i < alphas.size(); ++i) {
        out << alphas[i] << (i < alphas.size() - 1 ? ", " : "");
    }
    out << "]\n";
    out << "  },\n";
    out << "  \"results\": [\n";

    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        out << "    {\n";
        out << "      \"implementation\": \"" << r.impl_name << "\",\n";
        out << "      \"alpha\": " << std::fixed << std::setprecision(1) << r.alpha << ",\n";
        out << "      \"memory_bytes\": " << r.memory_bytes << ",\n";
        out << "      \"memory_kb\": " << std::fixed << std::setprecision(2) << (r.memory_bytes / 1024.0) << ",\n";
        out << "      \"insert_time_sec\": " << std::fixed << std::setprecision(6) << r.insert_time_sec << ",\n";
        out << "      \"query_time_sec\": " << std::fixed << std::setprecision(6) << r.query_time_sec << ",\n";
        out << "      \"inserts_per_sec\": " << std::fixed << std::setprecision(0) << (r.num_inserts / r.insert_time_sec) << ",\n";
        out << "      \"mean_error_pct\": " << std::fixed << std::setprecision(4) << r.mean_error_pct << ",\n";
        out << "      \"max_error_pct\": " << std::fixed << std::setprecision(4) << r.max_error_pct << "\n";
        out << "    }" << (i < results.size() - 1 ? ",\n" : "\n");
    }

    out << "  ]\n";
    out << "}\n";
    out.close();

    std::cout << "\nResults saved to: " << filename << std::endl;
}

int main() {
    const uint k = 8;  // Fixed k (optimal from k-sensitivity experiments)
    const size_t num_unique = 10000;
    const size_t total_items = 100000;

    // Multiple alpha values to test different skew levels
    std::vector<double> alphas = {1.0, 1.5, 2.0};

    // Memory levels via logsize: 10-17 (1K to 128K counters)
    std::vector<uint> logsizes = {10, 11, 12, 13, 14, 15, 16, 17};

    // Create results directory if it doesn't exist
    mkdir("./results", 0755);
    mkdir(experiments_path.c_str(), 0755);

    std::cout << "\n========================================\n";
    std::cout << "Zipfian Memory Efficiency Comparison\n";
    std::cout << "Fixed k = " << k << "\n";
    std::cout << "Unique items: " << num_unique << "\n";
    std::cout << "Total inserts: " << total_items << "\n";
    std::cout << "Alpha values: ";
    for (auto a : alphas) std::cout << a << " ";
    std::cout << "\nMemory levels (logsize): ";
    for (auto ls : logsizes) std::cout << ls << " ";
    std::cout << "\n========================================\n\n";

    std::vector<ZipfianMemoryResult> all_results;

    for (double alpha : alphas) {
        std::cout << "\n=== Alpha = " << alpha << " ===\n";

        // Generate Zipfian data for this alpha
        std::cout << "Generating Zipfian test data (alpha=" << alpha << ")..." << std::endl;
        auto [test_data, ground_truth] = generate_zipfian_data(num_unique, total_items, alpha, 42);

        for (uint logsize : logsizes) {
            std::cout << "\n--- Memory level: logsize=" << logsize
                      << " (~" << ((1ULL << logsize) * 2 / 1024) << " KB for 16-bit) ---\n";

            // CascadeCBF - Single layer (fair memory comparison)
            {
                std::cout << "  CascadeCBF-16 (1L)..." << std::flush;
                CCBF_16 cbf;
                cbf.configure(k, {logsize}, false, false);  // non-MI, hashing trick
                uint64_t mem = cbf.memory_usage();
                auto result = benchmark_memory(cbf, "CascadeCBF-16 (1L)", alpha, test_data, ground_truth, mem);
                all_results.push_back(result);
                std::cout << " mem=" << (mem / 1024) << "KB, err="
                          << std::fixed << std::setprecision(2) << result.mean_error_pct << "%\n";
            }

            // CascadeCBF - Single layer with MI
            {
                std::cout << "  CascadeCBF-16 MI..." << std::flush;
                CCBF_16 cbf;
                cbf.configure(k, {logsize}, true, false);  // MI, hashing trick
                uint64_t mem = cbf.memory_usage();
                auto result = benchmark_memory(cbf, "CascadeCBF-16 MI", alpha, test_data, ground_truth, mem);
                all_results.push_back(result);
                std::cout << " mem=" << (mem / 1024) << "KB, err="
                          << std::fixed << std::setprecision(2) << result.mean_error_pct << "%\n";
            }

            // CascadeCBF - Single layer with independent hashes
            {
                std::cout << "  CascadeCBF-16 IH..." << std::flush;
                CCBF_16 cbf;
                cbf.configure(k, {logsize}, false, true);  // non-MI, independent hashes
                uint64_t mem = cbf.memory_usage();
                auto result = benchmark_memory(cbf, "CascadeCBF-16 IH", alpha, test_data, ground_truth, mem);
                all_results.push_back(result);
                std::cout << " mem=" << (mem / 1024) << "KB, err="
                          << std::fixed << std::setprecision(2) << result.mean_error_pct << "%\n";
            }

            // CascadeCBF - Single layer with MI + independent hashes (best accuracy)
            {
                std::cout << "  CascadeCBF-16 MI+IH..." << std::flush;
                CCBF_16 cbf;
                cbf.configure(k, {logsize}, true, true);  // MI + independent hashes
                uint64_t mem = cbf.memory_usage();
                auto result = benchmark_memory(cbf, "CascadeCBF-16 MI+IH", alpha, test_data, ground_truth, mem);
                all_results.push_back(result);
                std::cout << " mem=" << (mem / 1024) << "KB, err="
                          << std::fixed << std::setprecision(2) << result.mean_error_pct << "%\n";
            }

            // CascadeCBF - Two-layer (for large count handling)
            {
                std::cout << "  CascadeCBF-16-16 MI+IH..." << std::flush;
                CCBF_16_16 cbf;
                uint layer1_logsize = (logsize > 2) ? logsize - 2 : logsize;
                cbf.configure(k, {logsize, layer1_logsize}, true, true);  // MI + independent hashes
                uint64_t mem = cbf.memory_usage();
                auto result = benchmark_memory(cbf, "CascadeCBF-16-16 MI+IH", alpha, test_data, ground_truth, mem);
                all_results.push_back(result);
                std::cout << " mem=" << (mem / 1024) << "KB, err="
                          << std::fixed << std::setprecision(2) << result.mean_error_pct << "%\n";
            }

            // Spectral BF (MI)
            {
                std::cout << "  Spectral BF (MI)..." << std::flush;
                SBFConfig conf{k, logsize, 16, MINIMAL_INCREMENT};
                SpectralBloomFilter sbf(conf);
                uint64_t mem = sbf.memory_usage();
                auto result = benchmark_memory(sbf, "Spectral BF (MI)", alpha, test_data, ground_truth, mem);
                all_results.push_back(result);
                std::cout << " mem=" << (mem / 1024) << "KB, err="
                          << std::fixed << std::setprecision(2) << result.mean_error_pct << "%\n";
            }

            // Count-Min Sketch
            {
                std::cout << "  Count-Min Sketch..." << std::flush;
                // Calculate width from logsize to achieve similar memory
                // CMS memory = depth * width * 2 bytes (16-bit counters)
                // For depth=5, width = 2^logsize / 5 * 2 to match Spectral BF memory
                uint depth = 5;
                uint width = (1U << logsize) / depth;
                if (width < 64) width = 64;  // Minimum width

                // Use epsilon/delta that gives us this width/depth
                // width = ceil(e/epsilon), depth = ceil(ln(1/delta))
                double epsilon = 2.718281828 / width;
                double delta = std::exp(-static_cast<double>(depth));

                CMSConfig conf{epsilon, std::max(delta, 1e-15), true, 16};
                CountMinSketch cms(conf);
                uint64_t mem = cms.memory_usage();
                auto result = benchmark_memory(cms, "Count-Min Sketch", alpha, test_data, ground_truth, mem);
                all_results.push_back(result);
                std::cout << " mem=" << (mem / 1024) << "KB, err="
                          << std::fixed << std::setprecision(2) << result.mean_error_pct << "%\n";
            }

            // d-Left CBF
            {
                std::cout << "  d-Left CBF..." << std::flush;
                // Calculate buckets from logsize
                // Memory = buckets * slots * 5 bytes
                // For similar memory to Spectral BF: buckets = 2^logsize * 2 / (4 * 5)
                uint d = 4;
                uint slots = 4;
                uint buckets_total = (1U << logsize) * 2 / (slots * 5);
                // Round to multiple of d
                buckets_total = ((buckets_total / d) + 1) * d;
                if (buckets_total < d * 16) buckets_total = d * 16;  // Minimum

                DLeftCBFConfig conf{buckets_total, d, slots, 8, 8};
                DLeftCountingBloomFilter dleft(conf);
                uint64_t mem = dleft.memory_usage();
                auto result = benchmark_memory(dleft, "d-Left CBF", alpha, test_data, ground_truth, mem);
                all_results.push_back(result);
                std::cout << " mem=" << (mem / 1024) << "KB, err="
                          << std::fixed << std::setprecision(2) << result.mean_error_pct << "%\n";
            }

            // Variable-Increment CBF (NOTE: expected ~4x overcounting for frequency)
            {
                std::cout << "  VI-CBF..." << std::flush;
                VICBFConfig conf{k, logsize, 16, 4};  // L=4
                VariableIncrementBloomFilter vicbf(conf);
                uint64_t mem = vicbf.memory_usage();
                auto result = benchmark_memory(vicbf, "VI-CBF", alpha, test_data, ground_truth, mem);
                all_results.push_back(result);
                std::cout << " mem=" << (mem / 1024) << "KB, err="
                          << std::fixed << std::setprecision(2) << result.mean_error_pct << "%\n";
            }
        }
    }

    // Print summary table
    std::cout << "\n==================================================\n";
    std::cout << "Summary (sorted by alpha, then memory)\n";
    std::cout << "==================================================\n\n";
    std::cout << std::left << std::setw(24) << "Implementation"
              << std::right << std::setw(6) << "Alpha"
              << std::setw(10) << "Memory"
              << std::setw(14) << "Inserts/sec"
              << std::setw(12) << "Mean Err%"
              << std::setw(12) << "Max Err%" << "\n";
    std::cout << std::string(78, '-') << "\n";

    for (const auto& r : all_results) {
        std::cout << std::left << std::setw(24) << r.impl_name
                  << std::right << std::setw(6) << std::fixed << std::setprecision(1) << r.alpha
                  << std::setw(8) << (r.memory_bytes / 1024) << " KB"
                  << std::setw(14) << std::fixed << std::setprecision(0) << (r.num_inserts / r.insert_time_sec)
                  << std::setw(12) << std::fixed << std::setprecision(2) << r.mean_error_pct
                  << std::setw(12) << std::fixed << std::setprecision(2) << r.max_error_pct
                  << "\n";
    }
    std::cout << "\n";

    save_results(all_results, num_unique, total_items, alphas);

    return 0;
}
