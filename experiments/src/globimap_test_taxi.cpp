/**
 * NYC Taxi urban mobility experiment
 *
 * Demonstrates CascadeCBF performance on extreme urban hotspots.
 * NYC taxi data has extreme power-law distribution:
 * - Manhattan accounts for ~90% of all pickups
 * - Times Square, Penn Station, airports are major hotspots
 *
 * Key metrics:
 * - Insert throughput
 * - Query accuracy on hotspot locations
 * - Memory efficiency for urban-dense data
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
#include <map>
#include <algorithm>
#include <cmath>
#include <sys/stat.h>

#include <highfive/H5File.hpp>
#include <tqdm.hpp>

using namespace globimap;

const std::string results_path = "./results/taxi/";

// NYC raster parameters - higher resolution for urban detail
const uint RASTER_WIDTH = 4096;
const uint RASTER_HEIGHT = 4096;

// NYC bounding box
const double NYC_MIN_LAT = 40.4;
const double NYC_MAX_LAT = 41.0;
const double NYC_MIN_LON = -74.3;
const double NYC_MAX_LON = -73.7;

// Convert lat/lon to raster coordinates (NYC-specific)
inline std::pair<uint64_t, uint64_t> latlon_to_raster(double lat, double lon) {
    double x = (lon - NYC_MIN_LON) / (NYC_MAX_LON - NYC_MIN_LON) * RASTER_WIDTH;
    double y = (lat - NYC_MIN_LAT) / (NYC_MAX_LAT - NYC_MIN_LAT) * RASTER_HEIGHT;
    x = std::max(0.0, std::min(x, (double)(RASTER_WIDTH - 1)));
    y = std::max(0.0, std::min(y, (double)(RASTER_HEIGHT - 1)));
    return {static_cast<uint64_t>(x), static_cast<uint64_t>(y)};
}

struct HotspotInfo {
    std::string name;
    double lat;
    double lon;
    uint64_t ground_truth;
    uint64_t estimated;
    double error_pct;
};

struct ImplResult {
    std::string impl_name;
    uint64_t memory_bytes;
    double insert_time_sec;
    double query_time_sec;
    uint64_t total_points;
    uint64_t unique_cells;
    double mean_error_pct;
    double max_error_pct;
    std::vector<HotspotInfo> hotspots;
};

template<typename Filter>
ImplResult run_taxi_benchmark(
    Filter& filter,
    const std::string& impl_name,
    uint64_t memory_bytes,
    const std::vector<std::pair<uint64_t, uint64_t>>& raster_points,
    const std::map<std::pair<uint64_t, uint64_t>, uint64_t>& ground_truth
) {
    using std::chrono::duration;
    using std::chrono::high_resolution_clock;

    ImplResult result;
    result.impl_name = impl_name;
    result.memory_bytes = memory_bytes;
    result.total_points = raster_points.size();
    result.unique_cells = ground_truth.size();

    std::cout << "  " << impl_name << ": Inserting " << raster_points.size() << " points...\n";

    // Insert all points
    auto t1 = high_resolution_clock::now();
    for (const auto& [x, y] : tq::tqdm(raster_points)) {
        filter.put({x, y});
    }
    auto t2 = high_resolution_clock::now();
    duration<double> insert_time = t2 - t1;
    result.insert_time_sec = insert_time.count();

    std::cout << "  " << impl_name << ": Querying " << ground_truth.size() << " unique cells...\n";

    // Query accuracy on all cells
    double total_error = 0.0;
    double max_error = 0.0;
    size_t samples = 0;

    auto tq1 = high_resolution_clock::now();
    for (const auto& [loc, truth] : ground_truth) {
        uint64_t estimated = filter.get_min({loc.first, loc.second});
        if (truth > 0) {
            double error = std::abs((double)estimated - (double)truth) / (double)truth * 100.0;
            total_error += error;
            max_error = std::max(max_error, error);
            samples++;
        }
    }
    auto tq2 = high_resolution_clock::now();
    duration<double> query_time = tq2 - tq1;
    result.query_time_sec = query_time.count();
    result.mean_error_pct = (samples > 0) ? total_error / samples : 0.0;
    result.max_error_pct = max_error;

    // Query specific NYC hotspots
    std::vector<std::pair<std::string, std::pair<double, double>>> hotspot_locs = {
        {"Times Square", {40.758, -73.9855}},
        {"Penn Station", {40.7506, -73.9935}},
        {"Grand Central", {40.7527, -73.9772}},
        {"JFK Airport", {40.6413, -73.7781}},
        {"LaGuardia", {40.7769, -73.8740}},
        {"Wall Street", {40.7074, -74.0113}},
        {"Central Park S", {40.7644, -73.9732}},
        {"Empire State", {40.7484, -73.9857}},
    };

    for (const auto& [name, coords] : hotspot_locs) {
        auto [rx, ry] = latlon_to_raster(coords.first, coords.second);
        HotspotInfo hs;
        hs.name = name;
        hs.lat = coords.first;
        hs.lon = coords.second;

        // Find ground truth for this cell
        auto it = ground_truth.find({rx, ry});
        hs.ground_truth = (it != ground_truth.end()) ? it->second : 0;
        hs.estimated = filter.get_min({rx, ry});
        hs.error_pct = (hs.ground_truth > 0) ?
            std::abs((double)hs.estimated - (double)hs.ground_truth) / (double)hs.ground_truth * 100.0 : 0.0;

        result.hotspots.push_back(hs);
    }

    return result;
}

void save_results(const std::vector<ImplResult>& results, const std::string& dataset_name) {
    std::stringstream filename;
    filename << results_path << "taxi_benchmark_" << dataset_name << ".json";

    std::ofstream out(filename.str());
    out << "{\n";
    out << "  \"experiment\": \"nyc_taxi_urban_mobility\",\n";
    out << "  \"dataset\": \"" << dataset_name << "\",\n";
    out << "  \"raster_size\": [" << RASTER_WIDTH << ", " << RASTER_HEIGHT << "],\n";
    out << "  \"implementations\": [\n";

    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        out << "    {\n";
        out << "      \"name\": \"" << r.impl_name << "\",\n";
        out << "      \"memory_bytes\": " << r.memory_bytes << ",\n";
        out << "      \"total_points\": " << r.total_points << ",\n";
        out << "      \"unique_cells\": " << r.unique_cells << ",\n";
        out << "      \"insert_time_sec\": " << std::fixed << std::setprecision(3) << r.insert_time_sec << ",\n";
        out << "      \"query_time_sec\": " << std::fixed << std::setprecision(3) << r.query_time_sec << ",\n";
        out << "      \"mean_error_pct\": " << std::fixed << std::setprecision(2) << r.mean_error_pct << ",\n";
        out << "      \"max_error_pct\": " << std::fixed << std::setprecision(2) << r.max_error_pct << ",\n";
        out << "      \"hotspots\": [\n";

        for (size_t j = 0; j < r.hotspots.size(); ++j) {
            const auto& hs = r.hotspots[j];
            out << "        {\"name\": \"" << hs.name << "\", "
                << "\"truth\": " << hs.ground_truth << ", "
                << "\"est\": " << hs.estimated << ", "
                << "\"error\": " << std::fixed << std::setprecision(1) << hs.error_pct << "}";
            out << (j < r.hotspots.size() - 1 ? ",\n" : "\n");
        }
        out << "      ]\n";
        out << "    }" << (i < results.size() - 1 ? ",\n" : "\n");
    }

    out << "  ]\n";
    out << "}\n";
    out.close();

    std::cout << "\nResults saved to: " << filename.str() << std::endl;
}

void print_summary(const std::vector<ImplResult>& results) {
    std::cout << "\n========================================\n";
    std::cout << "NYC Taxi Urban Mobility Summary\n";
    std::cout << "========================================\n\n";

    std::cout << std::left << std::setw(20) << "Implementation"
              << std::right << std::setw(10) << "Memory"
              << std::setw(12) << "Insert(s)"
              << std::setw(12) << "Query(s)"
              << std::setw(12) << "Mean Err%"
              << std::setw(12) << "Max Err%" << "\n";
    std::cout << std::string(78, '-') << "\n";

    for (const auto& r : results) {
        std::cout << std::left << std::setw(20) << r.impl_name
                  << std::right << std::setw(8) << (r.memory_bytes / 1024) << " KB"
                  << std::setw(12) << std::fixed << std::setprecision(3) << r.insert_time_sec
                  << std::setw(12) << std::fixed << std::setprecision(3) << r.query_time_sec
                  << std::setw(12) << std::fixed << std::setprecision(2) << r.mean_error_pct
                  << std::setw(12) << std::fixed << std::setprecision(2) << r.max_error_pct << "\n";
    }

    // Print hotspot details for first implementation
    if (!results.empty() && !results[0].hotspots.empty()) {
        std::cout << "\nHotspot Details (CascadeCBF):\n";
        std::cout << std::string(60, '-') << "\n";
        for (const auto& hs : results.back().hotspots) {
            std::cout << std::left << std::setw(15) << hs.name
                      << std::right << std::setw(10) << hs.ground_truth << " trips"
                      << std::setw(10) << hs.estimated << " est"
                      << std::setw(10) << std::fixed << std::setprecision(1) << hs.error_pct << "% err\n";
        }
    }
}

int main() {
    uint k = 8;
    mkdir(results_path.c_str(), 0755);

    std::string dataset_path = "./datasets/hdf5/nyc_taxi.h5";

    std::cout << "\n========================================\n";
    std::cout << "NYC Taxi Urban Mobility Experiment\n";
    std::cout << "Raster: " << RASTER_WIDTH << "x" << RASTER_HEIGHT << "\n";
    std::cout << "========================================\n\n";

    // Load HDF5 dataset
    std::cout << "Loading " << dataset_path << "...\n";
    HighFive::File file(dataset_path, HighFive::File::ReadOnly);
    auto dataset = file.getDataSet("coords");

    std::vector<std::vector<double>> coords;
    dataset.read(coords);
    std::cout << "Loaded " << coords.size() << " pickup locations\n";

    // Convert to raster and build ground truth
    std::vector<std::pair<uint64_t, uint64_t>> raster_points;
    std::map<std::pair<uint64_t, uint64_t>, uint64_t> ground_truth;

    raster_points.reserve(coords.size());
    for (const auto& c : coords) {
        auto [x, y] = latlon_to_raster(c[0], c[1]);
        raster_points.push_back({x, y});
        ground_truth[{x, y}]++;
    }

    std::cout << "Unique raster cells: " << ground_truth.size() << "\n";

    // Find top hotspots
    std::vector<std::pair<uint64_t, std::pair<uint64_t, uint64_t>>> sorted_cells;
    for (const auto& [loc, count] : ground_truth) {
        sorted_cells.push_back({count, loc});
    }
    std::sort(sorted_cells.rbegin(), sorted_cells.rend());

    std::cout << "\nTop 10 hotspot cells:\n";
    for (size_t i = 0; i < std::min((size_t)10, sorted_cells.size()); ++i) {
        std::cout << "  Cell (" << sorted_cells[i].second.first << ","
                  << sorted_cells[i].second.second << "): "
                  << sorted_cells[i].first << " pickups\n";
    }
    std::cout << "\n";

    std::vector<ImplResult> results;

    // Spectral BF (MI)
    {
        std::cout << "Testing Spectral BF (MI)...\n";
        SBFConfig conf{k, 20, 16, MINIMAL_INCREMENT};
        SpectralBloomFilter sbf(conf);
        uint64_t mem = sbf.memory_usage();
        results.push_back(run_taxi_benchmark(sbf, "Spectral BF (MI)", mem,
                                              raster_points, ground_truth));
        std::cout << "\n";
    }

    // Count-Min Sketch (configured with ~2MB to match other implementations)
    {
        std::cout << "Testing Count-Min Sketch...\n";
        // epsilon = e/width, delta = e^(-depth)
        // For ~2MB with 16-bit counters: width ≈ 209715, depth = 5
        // epsilon = 2.718/209715 ≈ 0.000013
        CMSConfig conf{0.000013, 0.01, true, 16};
        CountMinSketch cms(conf);
        uint64_t mem = cms.memory_usage();
        results.push_back(run_taxi_benchmark(cms, "Count-Min Sketch", mem,
                                              raster_points, ground_truth));
        std::cout << "\n";
    }

    // CascadeCBF (MI)
    {
        std::cout << "Testing CascadeCBF (MI)...\n";
        CCBF_16_16 cbf;
        cbf.configure(k, {20, 16}, true);
        uint64_t mem = cbf.memory_usage();
        results.push_back(run_taxi_benchmark(cbf, "CascadeCBF (MI)", mem,
                                              raster_points, ground_truth));
        std::cout << "\n";
    }

    print_summary(results);
    save_results(results, "nyc_taxi");

    return 0;
}
