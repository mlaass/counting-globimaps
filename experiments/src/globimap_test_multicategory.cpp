#include "counting_globimap.hpp"
#include "spectral_bloom_filter.hpp"
#include "dleft_counting_bf.hpp"
#include "count_min_sketch.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <string>
#include <sys/stat.h>

using namespace globimap;

const std::string experiments_path = "./results/multicategory/";

struct CategoryTestResult {
    std::string impl_name;
    uint category;
    uint64_t expected_count;
    uint64_t measured_count;
    double error_pct;
    double isolation_score;  // % of other categories that show 0 count
};

struct OverallResult {
    std::string impl_name;
    uint64_t memory_bytes;
    double mean_error_pct;
    double max_error_pct;
    double isolation_rate;
    double insert_time_sec;
    std::vector<CategoryTestResult> category_results;
};

template<typename Filter>
OverallResult benchmark_multicategory(Filter& filter, const std::string& impl_name, uint64_t memory_bytes) {
    using std::chrono::duration;
    using std::chrono::high_resolution_clock;

    OverallResult result;
    result.impl_name = impl_name;
    result.memory_bytes = memory_bytes;

    // Test scenario: Same location (1000, 2000) with 4 different categories
    // Category 1: 100 inserts, Category 2: 500 inserts,
    // Category 3: 50 inserts, Category 4: 1000 inserts
    struct CategoryTest {
        uint cat_id;
        uint64_t count;
    };

    std::vector<CategoryTest> tests = {
        {1, 100},
        {2, 500},
        {3, 50},
        {4, 1000}
    };

    // Insert events
    auto t1 = high_resolution_clock::now();
    for (const auto& test : tests) {
        for (uint64_t i = 0; i < test.count; ++i) {
            filter.put({1000, 2000, test.cat_id});
        }
    }
    auto t2 = high_resolution_clock::now();
    duration<double> insert_time = t2 - t1;
    result.insert_time_sec = insert_time.count();

    // Query each category and measure accuracy
    double total_error = 0.0;
    double max_error = 0.0;
    uint total_isolation_failures = 0;

    for (const auto& test : tests) {
        CategoryTestResult cat_result;
        cat_result.impl_name = impl_name;
        cat_result.category = test.cat_id;
        cat_result.expected_count = test.count;
        cat_result.measured_count = filter.get_min({1000, 2000, test.cat_id});

        // Calculate error
        double error = std::abs((double)cat_result.measured_count - (double)test.count) / (double)test.count * 100.0;
        cat_result.error_pct = error;
        total_error += error;
        max_error = std::max(max_error, error);

        // Test isolation: check that other categories return 0 or very small counts
        uint other_cats_clean = 0;
        for (uint other_cat = 5; other_cat <= 8; ++other_cat) {
            uint64_t other_count = filter.get_min({1000, 2000, other_cat});
            if (other_count == 0) other_cats_clean++;
        }
        cat_result.isolation_score = (double)other_cats_clean / 4.0 * 100.0;
        if (other_cats_clean < 4) total_isolation_failures++;

        result.category_results.push_back(cat_result);
    }

    result.mean_error_pct = total_error / tests.size();
    result.max_error_pct = max_error;
    result.isolation_rate = (double)(tests.size() * 4 - total_isolation_failures) / (tests.size() * 4) * 100.0;

    return result;
}

void save_results(const std::vector<OverallResult>& results) {
    std::stringstream filename;
    filename << experiments_path << "compare_multicategory.json";

    std::ofstream out(filename.str());
    out << "{\n";
    out << "  \"experiment\": \"multicategory_isolation\",\n";
    out << "  \"description\": \"Test category isolation at same location\",\n";
    out << "  \"implementations\": [\n";

    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        out << "    {\n";
        out << "      \"name\": \"" << r.impl_name << "\",\n";
        out << "      \"memory_bytes\": " << r.memory_bytes << ",\n";
        out << "      \"insert_time_sec\": " << std::fixed << std::setprecision(6) << r.insert_time_sec << ",\n";
        out << "      \"mean_error_pct\": " << std::fixed << std::setprecision(2) << r.mean_error_pct << ",\n";
        out << "      \"max_error_pct\": " << std::fixed << std::setprecision(2) << r.max_error_pct << ",\n";
        out << "      \"isolation_rate_pct\": " << std::fixed << std::setprecision(2) << r.isolation_rate << ",\n";
        out << "      \"categories\": [\n";

        for (size_t j = 0; j < r.category_results.size(); ++j) {
            const auto& c = r.category_results[j];
            out << "        {\n";
            out << "          \"category_id\": " << c.category << ",\n";
            out << "          \"expected\": " << c.expected_count << ",\n";
            out << "          \"measured\": " << c.measured_count << ",\n";
            out << "          \"error_pct\": " << std::fixed << std::setprecision(2) << c.error_pct << ",\n";
            out << "          \"isolation_score_pct\": " << std::fixed << std::setprecision(2) << c.isolation_score << "\n";
            out << "        }" << (j < r.category_results.size() - 1 ? ",\n" : "\n");
        }

        out << "      ]\n";
        out << "    }" << (i < results.size() - 1 ? ",\n" : "\n");
    }

    out << "  ]\n";
    out << "}\n";
    out.close();

    std::cout << "\nResults saved to: " << filename.str() << std::endl;
}

int main() {
    uint k = 8;

    mkdir(experiments_path.c_str(), 0755);

    std::cout << "\n========================================\n";
    std::cout << "Multi-Category Isolation Test\n";
    std::cout << "========================================\n\n";

    std::vector<OverallResult> results;

    // Spectral BF (MI)
    {
        std::cout << "Testing Spectral BF (MI)...\n";
        SBFConfig conf{k, 20, 16, MINIMAL_INCREMENT};
        SpectralBloomFilter sbf(conf);
        uint64_t mem = sbf.memory_usage();
        results.push_back(benchmark_multicategory(sbf, "Spectral BF (MI)", mem));
    }

    // d-Left CBF
    {
        std::cout << "Testing d-Left CBF...\n";
        DLeftCBFConfig conf{4864, 4, 4, 12, 16};
        DLeftCountingBloomFilter dleft(conf);
        uint64_t mem = dleft.memory_usage();
        results.push_back(benchmark_multicategory(dleft, "d-Left CBF", mem));
    }

    // Count-Min Sketch
    {
        std::cout << "Testing Count-Min Sketch...\n";
        CMSConfig conf{0.0003, 0.01, false, 16};
        CountMinSketch cms(conf);
        uint64_t mem = cms.memory_usage();
        results.push_back(benchmark_multicategory(cms, "Count-Min Sketch", mem));
    }

    // CountingGloBiMap (MI)
    {
        std::cout << "Testing CountingGloBiMap (MI)...\n";
        FilterConfig conf;
        conf.hash_k = k;
        conf.layers = {{8, 20}, {16, 18}};
        conf.minimal_increment = true;
        CountingGloBiMap<> gbm(conf);
        uint64_t mem = gbm.byte_size();
        results.push_back(benchmark_multicategory(gbm, "CountingGloBiMap (MI)", mem));
    }

    // Print summary
    std::cout << "\n========================================\n";
    std::cout << "Summary\n";
    std::cout << "========================================\n\n";
    std::cout << std::left << std::setw(25) << "Implementation"
              << std::right << std::setw(12) << "Memory"
              << std::setw(15) << "Mean Err %"
              << std::setw(15) << "Max Err %"
              << std::setw(18) << "Isolation %" << "\n";
    std::cout << std::string(85, '-') << "\n";

    for (const auto& r : results) {
        std::cout << std::left << std::setw(25) << r.impl_name
                  << std::right << std::setw(12) << (r.memory_bytes / 1024) << " KB"
                  << std::setw(15) << std::fixed << std::setprecision(2) << r.mean_error_pct
                  << std::setw(15) << std::fixed << std::setprecision(2) << r.max_error_pct
                  << std::setw(18) << std::fixed << std::setprecision(2) << r.isolation_rate
                  << "\n";

        // Print per-category details
        for (const auto& c : r.category_results) {
            std::cout << "  Cat " << c.category << ": expected=" << std::setw(4) << c.expected_count
                      << ", measured=" << std::setw(4) << c.measured_count
                      << ", error=" << std::setw(6) << std::fixed << std::setprecision(2) << c.error_pct << "%"
                      << ", isolation=" << std::setw(6) << std::setprecision(1) << c.isolation_score << "%\n";
        }
        std::cout << "\n";
    }

    save_results(results);

    return 0;
}
