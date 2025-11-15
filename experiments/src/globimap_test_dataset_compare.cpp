#include "counting_globimap.hpp"
#include "spectral_bloom_filter.hpp"
#include "dleft_counting_bf.hpp"
#include "count_min_sketch.hpp"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <math.h>
#include <string>
#include <type_traits>
#include <sys/stat.h>
#include <highfive/H5File.hpp>
#include <tqdm.hpp>
#include <tqdm/tqdm.h>

using namespace globimap;

// Use environment variable or datasets/hdf5 directory for data
const std::string base_path = []() {
    const char* env_path = std::getenv("GLOBIMAP_DATA_PATH");
    return env_path ? std::string(env_path) + "/" : std::string("./datasets/hdf5/");
}();

const std::string experiments_path = "./results/";

std::vector<std::string> datasets{"gdelt_events.h5", "covid19_cases.h5"};

struct BenchmarkResult {
    std::string impl_name;
    uint64_t memory_bytes;
    double insert_time_sec;
    double query_time_mean_sec;
    double query_time_min_sec;
    double query_time_max_sec;
    std::vector<double> query_times;
};

template<typename Filter>
BenchmarkResult benchmark_filter(Filter& filter, const std::string& impl_name,
                                 const std::string& dataset_file,
                                 uint width, uint height, uint64_t memory_bytes) {
    using std::chrono::duration;
    using std::chrono::high_resolution_clock;
    using namespace HighFive;

    BenchmarkResult result;
    result.impl_name = impl_name;
    result.memory_bytes = memory_bytes;

    // Load and insert dataset
    std::cout << "  " << impl_name << ": Loading " << dataset_file << "..." << std::endl;
    auto filename = base_path + dataset_file;
    auto file = File(filename, File::ReadWrite);
    DataSet dataset = file.getDataSet("coords");

    auto batch_size = 4096;
    std::vector<std::vector<double>> batch_data;
    auto shape = dataset.getDimensions();
    size_t total_points = shape[0];
    int batches = std::ceil((double)total_points / batch_size);

    std::cout << "  " << impl_name << ": Inserting " << total_points << " points..." << std::endl;
    auto t1 = high_resolution_clock::now();

    for (auto i : tq::trange(batches)) {
        size_t start = i * batch_size;
        size_t count = std::min((size_t)batch_size, total_points - start);
        dataset.select({start, 0}, {count, 2}).read(batch_data);

        for (auto& p : batch_data) {
            double x = (double)width * (((double)p[0] + 180.0) / 360.0);
            double y = (double)height * (((double)p[1] + 90.0) / 180.0);
            filter.put({(uint64_t)x, (uint64_t)y});
        }
    }

    auto t2 = high_resolution_clock::now();
    duration<double> insert_time = t2 - t1;
    result.insert_time_sec = insert_time.count();

    // Query benchmark - 10K queries across 10 runs
    std::cout << "  " << impl_name << ": Running query benchmark..." << std::endl;
    uint num_queries = 10000;
    double query_time_mean = 0;
    double query_time_min = MAXFLOAT;
    double query_time_max = 0;

    for (auto run = 0; run < 10; run++) {
        auto t3 = high_resolution_clock::now();
        for (auto i = 0; i < num_queries; ++i) {
            uint64_t x = rand() % width;
            uint64_t y = rand() % height;
            filter.get_min({x, y});
        }
        auto t4 = high_resolution_clock::now();

        duration<double> query_time = t4 - t3;
        double qt = query_time.count();
        query_time_mean += qt;
        result.query_times.push_back(qt);
        query_time_min = std::min(qt, query_time_min);
        query_time_max = std::max(qt, query_time_max);
    }

    query_time_mean /= 10.0;
    result.query_time_mean_sec = query_time_mean;
    result.query_time_min_sec = query_time_min;
    result.query_time_max_sec = query_time_max;

    return result;
}

void save_results(const std::vector<BenchmarkResult>& results,
                 const std::string& dataset_name,
                 uint width, uint height) {
    std::stringstream filename;
    filename << experiments_path << "compare_dataset_" << dataset_name
             << "_w" << width << "h" << height << ".json";

    std::ofstream out(filename.str());
    out << "{\n";
    out << "  \"experiment\": \"dataset_comparison\",\n";
    out << "  \"dataset\": \"" << dataset_name << "\",\n";
    out << "  \"width\": " << width << ",\n";
    out << "  \"height\": " << height << ",\n";
    out << "  \"implementations\": [\n";

    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        out << "    {\n";
        out << "      \"name\": \"" << r.impl_name << "\",\n";
        out << "      \"memory_bytes\": " << r.memory_bytes << ",\n";
        out << "      \"insert_time_sec\": " << std::fixed << std::setprecision(3)
            << r.insert_time_sec << ",\n";
        out << "      \"query_time_mean_sec\": " << std::scientific << std::setprecision(6)
            << r.query_time_mean_sec << ",\n";
        out << "      \"query_time_min_sec\": " << std::scientific << std::setprecision(6)
            << r.query_time_min_sec << ",\n";
        out << "      \"query_time_max_sec\": " << std::scientific << std::setprecision(6)
            << r.query_time_max_sec << ",\n";
        out << "      \"query_times_sec\": [";
        for (size_t j = 0; j < r.query_times.size(); ++j) {
            out << std::scientific << std::setprecision(6) << r.query_times[j];
            if (j < r.query_times.size() - 1) out << ", ";
        }
        out << "]\n";
        out << "    }" << (i < results.size() - 1 ? ",\n" : "\n");
    }

    out << "  ]\n";
    out << "}\n";
    out.close();

    std::cout << "\nResults saved to: " << filename.str() << std::endl;
}

bool file_exists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

int main() {
    uint width = 8192, height = 8192;
    uint k = 8;

    // Create results directory if it doesn't exist
    mkdir(experiments_path.c_str(), 0755);

    std::cout << "Data path: " << base_path << "\n";
    std::cout << "Results path: " << experiments_path << "\n\n";

    // Check which datasets are available
    std::vector<std::string> available_datasets;
    for (const auto& dataset_file : datasets) {
        std::string full_path = base_path + dataset_file;
        if (file_exists(full_path)) {
            available_datasets.push_back(dataset_file);
        } else {
            std::cout << "Warning: Dataset not found, skipping: " << full_path << "\n";
        }
    }

    if (available_datasets.empty()) {
        std::cerr << "\nError: No datasets found!\n";
        std::cerr << "Set GLOBIMAP_DATA_PATH environment variable to point to dataset directory,\n";
        std::cerr << "or place datasets in ./data/\n";
        std::cerr << "Expected datasets: twitter_coords.h5, asia-latest.h5\n";
        return 1;
    }

    for (const auto& dataset_file : available_datasets) {
        std::cout << "\n========================================\n";
        std::cout << "Testing dataset: " << dataset_file << "\n";
        std::cout << "========================================\n\n";

        std::vector<BenchmarkResult> results;

        // Spectral BF (MI) - 128 KB
        {
            std::cout << "Testing Spectral BF (MI)...\n";
            SBFConfig conf{k, 16, 16, MINIMAL_INCREMENT};
            SpectralBloomFilter sbf(conf);
            uint64_t mem = sbf.memory_usage();
            results.push_back(benchmark_filter(sbf, "Spectral BF (MI)", dataset_file, width, height, mem));
        }

        // d-Left CBF - 95 KB
        {
            std::cout << "\nTesting d-Left CBF...\n";
            DLeftCBFConfig conf{4864, 4, 4, 12, 16};
            DLeftCountingBloomFilter dleft(conf);
            uint64_t mem = dleft.memory_usage();
            results.push_back(benchmark_filter(dleft, "d-Left CBF", dataset_file, width, height, mem));
        }

        // Count-Min Sketch - 88.5 KB
        {
            std::cout << "\nTesting Count-Min Sketch...\n";
            CMSConfig conf{0.0003, 0.01, false, 16};
            CountMinSketch cms(conf);
            uint64_t mem = cms.memory_usage();
            results.push_back(benchmark_filter(cms, "Count-Min Sketch", dataset_file, width, height, mem));
        }

        // CountingGloBiMap (MI) - 96 KB
        {
            std::cout << "\nTesting CountingGloBiMap (MI)...\n";
            FilterConfig conf;
            conf.hash_k = k;
            conf.layers = {{8, 16}, {16, 14}};
            conf.minimal_increment = true;
            CountingGloBiMap<> gbm(conf);
            uint64_t mem = gbm.byte_size();
            results.push_back(benchmark_filter(gbm, "CountingGloBiMap (MI)", dataset_file, width, height, mem));
        }

        // Print summary
        std::cout << "\n========================================\n";
        std::cout << "Summary for " << dataset_file << "\n";
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
                      << std::setw(18) << std::scientific << std::setprecision(6) << r.query_time_mean_sec
                      << "\n";
        }
        std::cout << "\n";

        // Save results to JSON
        std::string dataset_name = dataset_file.substr(0, dataset_file.find(".h5"));
        save_results(results, dataset_name, width, height);
    }

    return 0;
}
