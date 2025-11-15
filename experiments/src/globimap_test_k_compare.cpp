#include "counting_globimap.hpp"
#include "spectral_bloom_filter.hpp"
#include "dleft_counting_bf.hpp"
#include "count_min_sketch.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <string>
#include <type_traits>
#include <vector>
#include <sys/stat.h>

using namespace globimap;

const std::string experiments_path = "./results/";

struct KSensitivityResult {
    std::string impl_name;
    uint k_value;
    uint64_t memory_bytes;
    double insert_time_sec;
    uint64_t num_inserts;
};

// Generate synthetic Zipfian-like data for testing
std::vector<std::vector<uint64_t>> generate_test_data(uint num_points, uint width, uint height) {
    std::vector<std::vector<uint64_t>> data;
    data.reserve(num_points);

    // Simple Zipfian-like distribution
    for (uint i = 0; i < num_points; ++i) {
        // Generate skewed data - most points cluster in certain regions
        double r = (double)rand() / RAND_MAX;
        r = pow(r, 2.0);  // Square to create skew
        uint64_t x = (uint64_t)(r * width);
        uint64_t y = (uint64_t)(r * height);
        data.push_back({x, y});
    }

    return data;
}

template<typename Filter>
KSensitivityResult benchmark_k(Filter& filter, const std::string& impl_name, uint k,
                               const std::vector<std::vector<uint64_t>>& data, uint64_t memory_bytes) {
    using std::chrono::duration;
    using std::chrono::high_resolution_clock;

    KSensitivityResult result;
    result.impl_name = impl_name;
    result.k_value = k;
    result.memory_bytes = memory_bytes;
    result.num_inserts = data.size();

    auto t1 = high_resolution_clock::now();

    for (const auto& point : data) {
        filter.put(point);
    }

    auto t2 = high_resolution_clock::now();
    duration<double> insert_time = t2 - t1;
    result.insert_time_sec = insert_time.count();

    return result;
}

void save_results(const std::vector<KSensitivityResult>& results) {
    std::stringstream filename;
    filename << experiments_path << "compare_k_sensitivity.json";

    std::ofstream out(filename.str());
    out << "{\n";
    out << "  \"experiment\": \"k_sensitivity_comparison\",\n";
    out << "  \"results\": [\n";

    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        out << "    {\n";
        out << "      \"implementation\": \"" << r.impl_name << "\",\n";
        out << "      \"k\": " << r.k_value << ",\n";
        out << "      \"memory_bytes\": " << r.memory_bytes << ",\n";
        out << "      \"insert_time_sec\": " << std::fixed << std::setprecision(6)
            << r.insert_time_sec << ",\n";
        out << "      \"inserts_per_sec\": " << std::fixed << std::setprecision(0)
            << (r.num_inserts / r.insert_time_sec) << "\n";
        out << "    }" << (i < results.size() - 1 ? ",\n" : "\n");
    }

    out << "  ]\n";
    out << "}\n";
    out.close();

    std::cout << "\nResults saved to: " << filename.str() << std::endl;
}

int main() {
    std::vector<uint> k_values = {1, 2, 4, 8, 16, 32};
    uint num_points = 100000;
    uint width = 8192, height = 8192;

    // Create results directory if it doesn't exist
    mkdir(experiments_path.c_str(), 0755);

    std::cout << "\n========================================\n";
    std::cout << "K Parameter Sensitivity Comparison\n";
    std::cout << "Testing k values: ";
    for (auto k : k_values) std::cout << k << " ";
    std::cout << "\nData points: " << num_points << "\n";
    std::cout << "========================================\n\n";

    // Generate test data once
    std::cout << "Generating test data..." << std::endl;
    auto test_data = generate_test_data(num_points, width, height);

    std::vector<KSensitivityResult> results;

    for (auto k : k_values) {
        std::cout << "\n--- Testing k=" << k << " ---\n";

        // Spectral BF (MI)
        {
            std::cout << "  Spectral BF (MI)..." << std::endl;
            SBFConfig conf{k, 16, 16, MINIMAL_INCREMENT};
            SpectralBloomFilter sbf(conf);
            uint64_t mem = sbf.memory_usage();
            results.push_back(benchmark_k(sbf, "Spectral BF (MI)", k, test_data, mem));
        }

        // d-Left CBF (use k as d parameter)
        {
            std::cout << "  d-Left CBF (d=" << k << ")..." << std::endl;
            DLeftCBFConfig conf{4864, 4, static_cast<uint>(std::min(k, 8u)), 12, 16};  // d limited to 8
            DLeftCountingBloomFilter dleft(conf);
            uint64_t mem = dleft.memory_usage();
            results.push_back(benchmark_k(dleft, "d-Left CBF", k, test_data, mem));
        }

        // Count-Min Sketch
        {
            std::cout << "  Count-Min Sketch..." << std::endl;
            // Note: Count-Min Sketch calculates depth automatically from epsilon/delta
            CMSConfig conf{0.0003, 0.01, false, 16};
            CountMinSketch cms(conf);
            uint64_t mem = cms.memory_usage();
            results.push_back(benchmark_k(cms, "Count-Min Sketch", k, test_data, mem));
        }

        // CountingGloBiMap (MI)
        {
            std::cout << "  CountingGloBiMap (MI)..." << std::endl;
            FilterConfig conf;
            conf.hash_k = k;
            conf.layers = {{8, 16}, {16, 14}};
            conf.minimal_increment = true;
            CountingGloBiMap<> gbm(conf);
            uint64_t mem = gbm.byte_size();
            results.push_back(benchmark_k(gbm, "CountingGloBiMap (MI)", k, test_data, mem));
        }
    }

    // Print summary table
    std::cout << "\n========================================\n";
    std::cout << "Summary\n";
    std::cout << "========================================\n\n";
    std::cout << std::left << std::setw(25) << "Implementation"
              << std::right << std::setw(6) << "k"
              << std::setw(12) << "Memory"
              << std::setw(18) << "Insert Time (s)"
              << std::setw(18) << "Inserts/sec" << "\n";
    std::cout << std::string(79, '-') << "\n";

    for (const auto& r : results) {
        std::cout << std::left << std::setw(25) << r.impl_name
                  << std::right << std::setw(6) << r.k_value
                  << std::setw(12) << (r.memory_bytes / 1024) << " KB"
                  << std::setw(18) << std::fixed << std::setprecision(6) << r.insert_time_sec
                  << std::setw(18) << std::fixed << std::setprecision(0) << (r.num_inserts / r.insert_time_sec)
                  << "\n";
    }
    std::cout << "\n";

    save_results(results);

    return 0;
}
