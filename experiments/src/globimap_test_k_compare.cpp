#include "cascade_cbf.hpp"
#include "spectral_bloom_filter.hpp"
#include "count_min_sketch.hpp"

#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <map>
#include <random>
#include <string>
#include <type_traits>
#include <vector>
#include <sys/stat.h>

using namespace globimap;

const std::string experiments_path = "./results/k_sensitivity/";

struct KSensitivityResult {
    std::string impl_name;
    uint k_value;
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
KSensitivityResult benchmark_k(Filter& filter, const std::string& impl_name, uint k,
                               const std::vector<std::vector<uint64_t>>& data,
                               const std::map<uint64_t, uint64_t>& ground_truth,
                               uint64_t memory_bytes) {
    using std::chrono::duration;
    using std::chrono::high_resolution_clock;

    KSensitivityResult result;
    result.impl_name = impl_name;
    result.k_value = k;
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

void save_results(const std::vector<KSensitivityResult>& results) {
    std::stringstream filename;
    filename << experiments_path << "compare_k_sensitivity.json";

    std::ofstream out(filename.str());
    if (!out.is_open()) {
        std::cerr << "Error: Could not open " << filename.str() << " for writing\n";
        return;
    }

    out << "{\n";
    out << "  \"experiment\": \"k_sensitivity_comparison\",\n";
    out << "  \"results\": [\n";

    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        out << "    {\n";
        out << "      \"implementation\": \"" << r.impl_name << "\",\n";
        out << "      \"k\": " << r.k_value << ",\n";
        out << "      \"memory_bytes\": " << r.memory_bytes << ",\n";
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

    std::cout << "\nResults saved to: " << filename.str() << std::endl;
}

int main() {
    // Continuous k values from 1 to 32
    std::vector<uint> k_values;
    for (uint k = 1; k <= 32; ++k) k_values.push_back(k);

    size_t num_unique = 10000;
    size_t total_items = 100000;
    double zipf_alpha = 1.5;

    // Create results directory if it doesn't exist
    mkdir("./results", 0755);
    mkdir(experiments_path.c_str(), 0755);

    std::cout << "\n========================================\n";
    std::cout << "K Parameter Sensitivity Comparison\n";
    std::cout << "Testing k values: 1-" << k_values.back() << " (continuous)\n";
    std::cout << "Unique items: " << num_unique << "\n";
    std::cout << "Total inserts: " << total_items << "\n";
    std::cout << "Zipfian alpha: " << zipf_alpha << "\n";
    std::cout << "========================================\n\n";

    // Generate Zipfian data with ground truth
    std::cout << "Generating Zipfian test data..." << std::endl;
    auto [test_data, ground_truth] = generate_zipfian_data(num_unique, total_items, zipf_alpha, 42);

    std::vector<KSensitivityResult> results;

    for (auto k : k_values) {
        std::cout << "\n--- Testing k=" << k << " ---\n";

        // Spectral BF (MI)
        {
            std::cout << "  Spectral BF (MI)..." << std::flush;
            SBFConfig conf{k, 16, 16, MINIMAL_INCREMENT};
            SpectralBloomFilter sbf(conf);
            uint64_t mem = sbf.memory_usage();
            results.push_back(benchmark_k(sbf, "Spectral BF (MI)", k, test_data, ground_truth, mem));
            std::cout << " err=" << std::fixed << std::setprecision(2) << results.back().mean_error_pct << "%\n";
        }

        // Count-Min Sketch (using k as depth via delta parameter)
        {
            std::cout << "  Count-Min Sketch..." << std::flush;
            // depth = ceil(ln(1/delta)), so delta = e^(-k) to get depth=k
            double delta = std::exp(-static_cast<double>(k));
            // Use reasonable epsilon for fair comparison
            double epsilon = 0.001;  // 0.1% error bound target
            CMSConfig conf{epsilon, std::max(delta, 1e-15), true, 16};  // Conservative update
            CountMinSketch cms(conf);
            uint64_t mem = cms.memory_usage();
            results.push_back(benchmark_k(cms, "Count-Min Sketch", k, test_data, ground_truth, mem));
            std::cout << " err=" << std::fixed << std::setprecision(2) << results.back().mean_error_pct << "%\n";
        }

        // CascadeCBF (MI)
        {
            std::cout << "  CascadeCBF (MI)..." << std::flush;
            CCBF_12_20 cbf;
            cbf.configure(k, {16, 14}, true);
            uint64_t mem = cbf.memory_usage();
            results.push_back(benchmark_k(cbf, "CascadeCBF (MI)", k, test_data, ground_truth, mem));
            std::cout << " err=" << std::fixed << std::setprecision(2) << results.back().mean_error_pct << "%\n";
        }
    }

    // Print summary table
    std::cout << "\n========================================\n";
    std::cout << "Summary (sorted by k)\n";
    std::cout << "========================================\n\n";
    std::cout << std::left << std::setw(20) << "Implementation"
              << std::right << std::setw(4) << "k"
              << std::setw(10) << "Memory"
              << std::setw(14) << "Inserts/sec"
              << std::setw(12) << "Mean Err%"
              << std::setw(12) << "Max Err%" << "\n";
    std::cout << std::string(72, '-') << "\n";

    for (const auto& r : results) {
        std::cout << std::left << std::setw(20) << r.impl_name
                  << std::right << std::setw(4) << r.k_value
                  << std::setw(8) << (r.memory_bytes / 1024) << " KB"
                  << std::setw(14) << std::fixed << std::setprecision(0) << (r.num_inserts / r.insert_time_sec)
                  << std::setw(12) << std::fixed << std::setprecision(4) << r.mean_error_pct
                  << std::setw(12) << std::fixed << std::setprecision(4) << r.max_error_pct
                  << "\n";
    }
    std::cout << "\n";

    save_results(results);

    return 0;
}
