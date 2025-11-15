#include "counting_globimap.hpp"
#include "spectral_bloom_filter.hpp"
#include "dleft_counting_bf.hpp"
#include "count_min_sketch.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <random>
#include <sys/stat.h>

#include "highfive/H5File.hpp"
#include <tqdm.hpp>
#include <tqdm/tqdm.h>

using namespace globimap;

const std::string data_path = "./datasets/hdf5/";
const std::string results_path = "./results/multicategory/";

struct BenchmarkResult {
    std::string impl_name;
    std::string dataset_name;
    uint64_t memory_bytes;
    double insert_time_sec;
    double query_mean_sec;

    // Per-category statistics
    struct CategoryStats {
        uint category_id;
        uint64_t insert_count;
        uint64_t query_count;
        double mean_estimated_count;
        double mean_error_pct;
    };
    std::vector<CategoryStats> category_stats;
};

template<typename Filter>
BenchmarkResult benchmark_multicategory_dataset(
    Filter& filter,
    const std::string& impl_name,
    const std::string& dataset_name,
    uint64_t memory_bytes,
    const std::vector<std::vector<double>>& coords
) {
    using std::chrono::duration;
    using std::chrono::high_resolution_clock;

    BenchmarkResult result;
    result.impl_name = impl_name;
    result.dataset_name = dataset_name;
    result.memory_bytes = memory_bytes;

    const size_t num_queries = 10000;

    std::cout << "  " << impl_name << ": Inserting " << coords.size() << " events...\n";

    // Insert all events (with categories)
    auto t1 = high_resolution_clock::now();
    for (const auto& coord : tq::tqdm(coords)) {
        // Convert to uint64_t for hashing
        uint64_t x = static_cast<uint64_t>(coord[0] * 1000000);  // Lat with 6 decimals
        uint64_t y = static_cast<uint64_t>(coord[1] * 1000000);  // Lon with 6 decimals
        uint64_t cat = static_cast<uint64_t>(coord[2]);          // Category 1-4
        filter.put({x, y, cat});
    }
    auto t2 = high_resolution_clock::now();
    duration<double> insert_time = t2 - t1;
    result.insert_time_sec = insert_time.count();

    // Count events per category
    std::map<uint, uint64_t> category_counts;
    for (const auto& coord : coords) {
        uint cat = static_cast<uint>(coord[2]);
        category_counts[cat]++;
    }

    std::cout << "  " << impl_name << ": Running query benchmark (" << num_queries << " queries per category)...\n";

    // Query benchmark: sample random events from each category
    std::random_device rd;
    std::mt19937 gen(42);  // Fixed seed for reproducibility

    // Build category-specific event lists for sampling
    std::map<uint, std::vector<std::vector<double>>> events_by_category;
    for (const auto& coord : coords) {
        uint cat = static_cast<uint>(coord[2]);
        events_by_category[cat].push_back(coord);
    }

    double total_query_time = 0.0;

    // Query each category
    for (uint cat = 1; cat <= 4; ++cat) {
        if (events_by_category[cat].empty()) continue;

        BenchmarkResult::CategoryStats cat_stats;
        cat_stats.category_id = cat;
        cat_stats.insert_count = category_counts[cat];
        cat_stats.query_count = std::min(num_queries, events_by_category[cat].size());

        // Sample events from this category
        std::vector<size_t> indices(events_by_category[cat].size());
        std::iota(indices.begin(), indices.end(), 0);
        std::shuffle(indices.begin(), indices.end(), gen);

        double total_estimated = 0.0;
        double total_error = 0.0;

        auto tq1 = high_resolution_clock::now();
        for (size_t i = 0; i < cat_stats.query_count; ++i) {
            const auto& coord = events_by_category[cat][indices[i]];
            uint64_t x = static_cast<uint64_t>(coord[0] * 1000000);
            uint64_t y = static_cast<uint64_t>(coord[1] * 1000000);
            uint64_t c = static_cast<uint64_t>(coord[2]);

            uint64_t estimated = filter.get_min({x, y, c});
            total_estimated += estimated;

            // Error: we expect count >= 1 for sampled events
            // For multi-category, we can't know the exact count, but it should be >= 1
            double error = (estimated == 0) ? 100.0 : 0.0;  // Flag as error if returns 0
            total_error += error;
        }
        auto tq2 = high_resolution_clock::now();
        duration<double> query_time = tq2 - tq1;
        total_query_time += query_time.count();

        cat_stats.mean_estimated_count = total_estimated / cat_stats.query_count;
        cat_stats.mean_error_pct = total_error / cat_stats.query_count;

        result.category_stats.push_back(cat_stats);
    }

    result.query_mean_sec = total_query_time / (4 * num_queries);  // Average per query

    return result;
}

void save_results(const std::vector<BenchmarkResult>& results, const std::string& dataset_name) {
    std::stringstream filename;
    filename << results_path << "compare_multicategory_" << dataset_name << ".json";

    std::ofstream out(filename.str());
    out << "{\n";
    out << "  \"experiment\": \"multicategory_dataset_benchmark\",\n";
    out << "  \"dataset\": \"" << dataset_name << "\",\n";
    out << "  \"implementations\": [\n";

    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        out << "    {\n";
        out << "      \"name\": \"" << r.impl_name << "\",\n";
        out << "      \"memory_bytes\": " << r.memory_bytes << ",\n";
        out << "      \"insert_time_sec\": " << std::fixed << std::setprecision(6) << r.insert_time_sec << ",\n";
        out << "      \"query_mean_sec\": " << std::scientific << std::setprecision(6) << r.query_mean_sec << ",\n";
        out << "      \"categories\": [\n";

        for (size_t j = 0; j < r.category_stats.size(); ++j) {
            const auto& c = r.category_stats[j];
            out << "        {\n";
            out << "          \"category_id\": " << c.category_id << ",\n";
            out << "          \"insert_count\": " << c.insert_count << ",\n";
            out << "          \"query_count\": " << c.query_count << ",\n";
            out << "          \"mean_estimated\": " << std::fixed << std::setprecision(2) << c.mean_estimated_count << ",\n";
            out << "          \"mean_error_pct\": " << std::fixed << std::setprecision(2) << c.mean_error_pct << "\n";
            out << "        }" << (j < r.category_stats.size() - 1 ? ",\n" : "\n");
        }

        out << "      ]\n";
        out << "    }" << (i < results.size() - 1 ? ",\n" : "\n");
    }

    out << "  ]\n";
    out << "}\n";
    out.close();

    std::cout << "\nResults saved to: " << filename.str() << std::endl;
}

void print_summary(const std::vector<BenchmarkResult>& results) {
    std::cout << "\n========================================\n";
    std::cout << "Summary\n";
    std::cout << "========================================\n\n";

    std::cout << std::left << std::setw(25) << "Implementation"
              << std::right << std::setw(12) << "Memory"
              << std::setw(15) << "Insert (s)"
              << std::setw(18) << "Query Mean (s)" << "\n";
    std::cout << std::string(70, '-') << "\n";

    for (const auto& r : results) {
        std::cout << std::left << std::setw(25) << r.impl_name
                  << std::right << std::setw(12) << (r.memory_bytes / 1024) << " KB"
                  << std::setw(15) << std::fixed << std::setprecision(3) << r.insert_time_sec
                  << std::setw(18) << std::scientific << std::setprecision(6) << r.query_mean_sec << "\n";

        // Print per-category stats
        for (const auto& c : r.category_stats) {
            std::cout << "  Cat " << c.category_id << ": "
                      << c.insert_count << " inserts, "
                      << "mean est=" << std::fixed << std::setprecision(1) << c.mean_estimated_count
                      << ", error=" << std::setprecision(2) << c.mean_error_pct << "%\n";
        }
        std::cout << "\n";
    }
}

int main() {
    uint k = 8;
    mkdir(results_path.c_str(), 0755);

    std::string dataset_file = "gdelt_events_multicategory.h5";
    std::string dataset_path = data_path + dataset_file;

    std::cout << "\n========================================\n";
    std::cout << "Multi-Category Dataset Benchmark\n";
    std::cout << "Dataset: " << dataset_file << "\n";
    std::cout << "========================================\n\n";

    // Load HDF5 dataset
    std::cout << "Loading " << dataset_path << "...\n";
    HighFive::File file(dataset_path, HighFive::File::ReadOnly);
    auto dataset = file.getDataSet("coords");
    auto shape = dataset.getDimensions();

    std::cout << "Dataset shape: [" << shape[0] << ", " << shape[1] << "]\n";
    if (shape[1] != 3) {
        std::cerr << "Error: Expected 3 columns [lat, lon, category], got " << shape[1] << "\n";
        return 1;
    }

    // Read all data
    std::vector<std::vector<double>> coords;
    dataset.read(coords);
    std::cout << "Loaded " << coords.size() << " events\n\n";

    std::vector<BenchmarkResult> results;

    // Spectral BF (MI)
    {
        std::cout << "Testing Spectral BF (MI)...\n";
        SBFConfig conf{k, 20, 16, MINIMAL_INCREMENT};
        SpectralBloomFilter sbf(conf);
        uint64_t mem = sbf.memory_usage();
        results.push_back(benchmark_multicategory_dataset(sbf, "Spectral BF (MI)", dataset_file, mem, coords));
        std::cout << "\n";
    }

    // d-Left CBF
    {
        std::cout << "Testing d-Left CBF...\n";
        DLeftCBFConfig conf{4864, 4, 4, 12, 16};
        DLeftCountingBloomFilter dleft(conf);
        uint64_t mem = dleft.memory_usage();
        results.push_back(benchmark_multicategory_dataset(dleft, "d-Left CBF", dataset_file, mem, coords));
        std::cout << "\n";
    }

    // Count-Min Sketch
    {
        std::cout << "Testing Count-Min Sketch...\n";
        CMSConfig conf{0.0003, 0.01, false, 16};
        CountMinSketch cms(conf);
        uint64_t mem = cms.memory_usage();
        results.push_back(benchmark_multicategory_dataset(cms, "Count-Min Sketch", dataset_file, mem, coords));
        std::cout << "\n";
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
        results.push_back(benchmark_multicategory_dataset(gbm, "CountingGloBiMap (MI)", dataset_file, mem, coords));
        std::cout << "\n";
    }

    print_summary(results);
    save_results(results, "gdelt_events_multicategory");

    return 0;
}
