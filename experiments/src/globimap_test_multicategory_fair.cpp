/**
 * @file globimap_test_multicategory_fair.cpp
 * @brief Fair memory comparison for multi-category counting
 *
 * Compares all implementations at the SAME memory budget (1 MB default).
 * Uses exact_size/modulo mode for arbitrary sizes.
 *
 * Note: Insert times are NOT comparable due to modulo vs bitmask difference.
 * This experiment focuses on accuracy at equal memory.
 */

#include "cascade_cbf.hpp"
#include "spectral_bloom_filter.hpp"
#include "count_min_sketch.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <random>
#include <map>
#include <sys/stat.h>

#include "highfive/H5File.hpp"
#include <tqdm.hpp>
#include <tqdm/tqdm.h>

using namespace globimap;

const std::string data_path = "./datasets/hdf5/";
const std::string results_path = "./results/multicategory_fair/";

struct BenchmarkResult {
    std::string impl_name;
    std::string dataset_name;
    uint64_t memory_bytes;
    uint64_t target_memory_bytes;
    double insert_time_sec;
    double resolution_scale;
    uint64_t max_ground_truth_count;  // Max count in any single cell
    uint64_t unique_cells;            // Number of unique location-category cells

    // Per-category statistics
    struct CategoryStats {
        uint category_id;
        uint64_t insert_count;
        uint64_t query_count;
        double mean_estimated_count;
        double mean_error_pct;
        double max_error_pct;
    };
    std::vector<CategoryStats> category_stats;

    // Overall statistics
    double overall_mean_error_pct;
    double overall_max_error_pct;
};

/**
 * @brief Calculate exact size for Spectral BF to hit target memory
 */
uint64_t calc_sbf_size_for_memory(uint64_t target_bytes, uint counter_bits) {
    return target_bytes / (counter_bits / 8);
}

/**
 * @brief Calculate CMS dimensions for target memory
 * Uses depth=5 (standard) and calculates width to fit memory
 */
std::pair<uint, uint> calc_cms_dims_for_memory(uint64_t target_bytes, uint counter_bits, uint depth = 5) {
    uint64_t bytes_per_counter = counter_bits / 8;
    uint64_t total_counters = target_bytes / bytes_per_counter;
    uint width = total_counters / depth;
    return {width, depth};
}

/**
 * @brief Calculate CascadeCBF layer sizes for target memory
 * Uses 90% for layer0 (16-bit), 10% for layer1 (16-bit)
 */
std::pair<uint64_t, uint64_t> calc_ccbf_sizes_for_memory(uint64_t target_bytes) {
    // Layer0: 16-bit (2 bytes), Layer1: 16-bit (2 bytes) for CCBF_16_16
    // Allocate 90% to layer0, 10% to layer1
    uint64_t layer0_bytes = static_cast<uint64_t>(target_bytes * 0.90);
    uint64_t layer1_bytes = target_bytes - layer0_bytes;

    uint64_t layer0_size = layer0_bytes / 2;  // 16-bit = 2 bytes
    uint64_t layer1_size = layer1_bytes / 2;  // 16-bit = 2 bytes (CCBF_16_16)

    return {layer0_size, layer1_size};
}

template<typename Filter>
BenchmarkResult benchmark_implementation(
    Filter& filter,
    const std::string& impl_name,
    const std::string& dataset_name,
    uint64_t actual_memory,
    uint64_t target_memory,
    const std::vector<std::vector<double>>& coords,
    const std::map<uint, std::vector<size_t>>& category_indices,
    double resolution_scale = 1000000.0
) {
    using std::chrono::duration;
    using std::chrono::high_resolution_clock;

    BenchmarkResult result;
    result.impl_name = impl_name;
    result.dataset_name = dataset_name;
    result.memory_bytes = actual_memory;
    result.target_memory_bytes = target_memory;
    result.resolution_scale = resolution_scale;

    const size_t num_queries = 10000;

    std::cout << "  " << impl_name << " (" << actual_memory / 1024 << " KB): Inserting " << coords.size() << " events...\n";

    // Count per location-category pair for ground truth
    std::map<std::tuple<uint64_t, uint64_t, uint64_t>, uint64_t> ground_truth;

    auto t1 = high_resolution_clock::now();
    for (const auto& coord : tq::tqdm(coords)) {
        uint64_t x = static_cast<uint64_t>(coord[0] * resolution_scale);
        uint64_t y = static_cast<uint64_t>(coord[1] * resolution_scale);
        uint64_t cat = static_cast<uint64_t>(coord[2]);
        filter.put({x, y, cat});
        ground_truth[{x, y, cat}]++;
    }
    auto t2 = high_resolution_clock::now();
    duration<double> insert_time = t2 - t1;
    result.insert_time_sec = insert_time.count();

    // Calculate max count and unique cells
    result.unique_cells = ground_truth.size();
    result.max_ground_truth_count = 0;
    for (const auto& [key, count] : ground_truth) {
        result.max_ground_truth_count = std::max(result.max_ground_truth_count, count);
    }
    std::cout << "  " << impl_name << ": " << result.unique_cells << " unique cells, max count=" << result.max_ground_truth_count << "\n";

    std::cout << "  " << impl_name << ": Querying " << num_queries << " samples per category...\n";

    std::random_device rd;
    std::mt19937 gen(42);

    double total_error_sum = 0.0;
    double total_max_error = 0.0;
    size_t total_queries = 0;

    // Query each category
    for (uint cat = 1; cat <= 4; ++cat) {
        auto it = category_indices.find(cat);
        if (it == category_indices.end() || it->second.empty()) continue;

        const auto& indices = it->second;
        BenchmarkResult::CategoryStats cat_stats;
        cat_stats.category_id = cat;
        cat_stats.insert_count = indices.size();
        cat_stats.query_count = std::min(num_queries, indices.size());

        // Shuffle indices for random sampling
        std::vector<size_t> shuffled_indices = indices;
        std::shuffle(shuffled_indices.begin(), shuffled_indices.end(), gen);

        double error_sum = 0.0;
        double max_error = 0.0;
        double estimated_sum = 0.0;

        for (size_t i = 0; i < cat_stats.query_count; ++i) {
            const auto& coord = coords[shuffled_indices[i]];
            uint64_t x = static_cast<uint64_t>(coord[0] * resolution_scale);
            uint64_t y = static_cast<uint64_t>(coord[1] * resolution_scale);
            uint64_t c = static_cast<uint64_t>(coord[2]);

            uint64_t estimated = filter.get_min({x, y, c});
            uint64_t actual = ground_truth[{x, y, c}];

            estimated_sum += estimated;

            // Calculate relative error
            double error = 0.0;
            if (actual > 0) {
                error = 100.0 * std::abs(static_cast<double>(estimated) - actual) / actual;
            } else if (estimated > 0) {
                error = 100.0;  // False positive
            }

            error_sum += error;
            max_error = std::max(max_error, error);
        }

        cat_stats.mean_estimated_count = estimated_sum / cat_stats.query_count;
        cat_stats.mean_error_pct = error_sum / cat_stats.query_count;
        cat_stats.max_error_pct = max_error;

        result.category_stats.push_back(cat_stats);

        total_error_sum += error_sum;
        total_max_error = std::max(total_max_error, max_error);
        total_queries += cat_stats.query_count;
    }

    result.overall_mean_error_pct = total_error_sum / total_queries;
    result.overall_max_error_pct = total_max_error;

    return result;
}

void save_results(const std::vector<BenchmarkResult>& results, const std::string& dataset_name, uint64_t target_memory_kb, double resolution_scale) {
    std::stringstream filename;
    filename << results_path << "fair_compare_" << target_memory_kb << "kb_" << dataset_name << ".json";

    std::ofstream out(filename.str());
    out << "{\n";
    out << "  \"experiment\": \"fair_memory_comparison\",\n";
    out << "  \"dataset\": \"" << dataset_name << "\",\n";
    out << "  \"target_memory_kb\": " << target_memory_kb << ",\n";
    out << "  \"resolution_scale\": " << std::fixed << std::setprecision(0) << resolution_scale << ",\n";
    out << "  \"implementations\": [\n";

    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        out << "    {\n";
        out << "      \"name\": \"" << r.impl_name << "\",\n";
        out << "      \"memory_bytes\": " << r.memory_bytes << ",\n";
        out << "      \"target_memory_bytes\": " << r.target_memory_bytes << ",\n";
        out << "      \"insert_time_sec\": " << std::fixed << std::setprecision(6) << r.insert_time_sec << ",\n";
        out << "      \"unique_cells\": " << r.unique_cells << ",\n";
        out << "      \"max_ground_truth_count\": " << r.max_ground_truth_count << ",\n";
        out << "      \"overall_mean_error_pct\": " << std::fixed << std::setprecision(4) << r.overall_mean_error_pct << ",\n";
        out << "      \"overall_max_error_pct\": " << std::fixed << std::setprecision(4) << r.overall_max_error_pct << ",\n";
        out << "      \"categories\": [\n";

        for (size_t j = 0; j < r.category_stats.size(); ++j) {
            const auto& c = r.category_stats[j];
            out << "        {\n";
            out << "          \"category_id\": " << c.category_id << ",\n";
            out << "          \"insert_count\": " << c.insert_count << ",\n";
            out << "          \"query_count\": " << c.query_count << ",\n";
            out << "          \"mean_estimated\": " << std::fixed << std::setprecision(2) << c.mean_estimated_count << ",\n";
            out << "          \"mean_error_pct\": " << std::fixed << std::setprecision(4) << c.mean_error_pct << ",\n";
            out << "          \"max_error_pct\": " << std::fixed << std::setprecision(4) << c.max_error_pct << "\n";
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

void print_summary(const std::vector<BenchmarkResult>& results, uint64_t target_memory_kb, double resolution_scale) {
    std::cout << "\n========================================\n";
    std::cout << "Fair Comparison at " << target_memory_kb << " KB Memory Budget\n";
    std::cout << "Resolution scale: " << std::fixed << std::setprecision(0) << resolution_scale << "\n";
    if (!results.empty()) {
        std::cout << "Unique cells: " << results[0].unique_cells << ", Max count: " << results[0].max_ground_truth_count << "\n";
    }
    std::cout << "========================================\n\n";

    std::cout << std::left << std::setw(22) << "Implementation"
              << std::right << std::setw(12) << "Memory"
              << std::setw(14) << "Mean Err%"
              << std::setw(14) << "Max Err%" << "\n";
    std::cout << std::string(62, '-') << "\n";

    for (const auto& r : results) {
        std::cout << std::left << std::setw(22) << r.impl_name
                  << std::right << std::setw(10) << (r.memory_bytes / 1024) << " KB"
                  << std::setw(14) << std::fixed << std::setprecision(2) << r.overall_mean_error_pct
                  << std::setw(14) << std::fixed << std::setprecision(2) << r.overall_max_error_pct << "\n";
    }

    std::cout << "\nPer-category breakdown:\n";
    std::cout << std::string(62, '-') << "\n";

    for (const auto& r : results) {
        std::cout << r.impl_name << ":\n";
        for (const auto& c : r.category_stats) {
            std::cout << "  Cat " << c.category_id << ": "
                      << c.insert_count << " inserts, "
                      << "mean err=" << std::fixed << std::setprecision(2) << c.mean_error_pct << "%, "
                      << "max err=" << std::setprecision(2) << c.max_error_pct << "%\n";
        }
        std::cout << "\n";
    }
}

int main(int argc, char* argv[]) {
    // Default: 1 MB memory budget, 1000000 resolution (0.1m for lat/lon)
    uint64_t target_memory_kb = 1024;
    double resolution_scale = 1000000.0;  // Default: ~0.1m resolution

    if (argc > 1) {
        target_memory_kb = std::stoull(argv[1]);
    }
    if (argc > 2) {
        resolution_scale = std::stod(argv[2]);
    }

    uint64_t target_memory_bytes = target_memory_kb * 1024;
    uint k = 8;

    mkdir(results_path.c_str(), 0755);

    std::string dataset_file = "gdelt_events_multicategory.h5";
    std::string dataset_path = data_path + dataset_file;

    std::cout << "\n========================================\n";
    std::cout << "Fair Memory Comparison Benchmark\n";
    std::cout << "Target memory: " << target_memory_kb << " KB\n";
    std::cout << "Resolution scale: " << resolution_scale << "\n";
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

    std::vector<std::vector<double>> coords;
    dataset.read(coords);
    std::cout << "Loaded " << coords.size() << " events\n";

    // Build category indices
    std::map<uint, std::vector<size_t>> category_indices;
    for (size_t i = 0; i < coords.size(); ++i) {
        uint cat = static_cast<uint>(coords[i][2]);
        category_indices[cat].push_back(i);
    }

    std::cout << "\nCategory distribution:\n";
    for (const auto& [cat, indices] : category_indices) {
        std::cout << "  Category " << cat << ": " << indices.size() << " events\n";
    }
    std::cout << "\n";

    std::vector<BenchmarkResult> results;

    // Spectral BF (MI) with exact size
    {
        std::cout << "Testing Spectral BF (MI)...\n";
        uint64_t exact_size = calc_sbf_size_for_memory(target_memory_bytes, 32);
        SBFConfig conf{k, 0, 32, MINIMAL_INCREMENT, exact_size};  // logsize=0 (unused), exact_size set
        SpectralBloomFilter sbf(conf);
        uint64_t mem = sbf.memory_usage();
        results.push_back(benchmark_implementation(sbf, "Spectral BF (MI)", dataset_file, mem, target_memory_bytes, coords, category_indices, resolution_scale));
        std::cout << "\n";
    }

    // Count-Min Sketch with exact dimensions
    {
        std::cout << "Testing Count-Min Sketch...\n";
        auto [width, depth] = calc_cms_dims_for_memory(target_memory_bytes, 32, 5);
        CMSConfig conf{0.01, 0.01, true, 32, width, depth};  // epsilon/delta unused, exact dims set
        CountMinSketch cms(conf);
        uint64_t mem = cms.memory_usage();
        results.push_back(benchmark_implementation(cms, "Count-Min Sketch", dataset_file, mem, target_memory_bytes, coords, category_indices, resolution_scale));
        std::cout << "\n";
    }

    // CascadeCBF (MI) with exact sizes
    {
        std::cout << "Testing CascadeCBF (MI)...\n";
        auto [layer0_size, layer1_size] = calc_ccbf_sizes_for_memory(target_memory_bytes);
        CCBF_16_16 cbf;
        cbf.configure_exact(k, {layer0_size, layer1_size}, true);  // MI mode
        uint64_t mem = cbf.memory_usage();
        results.push_back(benchmark_implementation(cbf, "CascadeCBF (MI)", dataset_file, mem, target_memory_bytes, coords, category_indices, resolution_scale));
        std::cout << "\n";
    }

    print_summary(results, target_memory_kb, resolution_scale);
    save_results(results, "gdelt_multicategory", target_memory_kb, resolution_scale);

    return 0;
}
