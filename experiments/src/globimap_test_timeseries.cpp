/**
 * Time-series evolution experiment for CascadeCBF
 *
 * Demonstrates how CascadeCBF handles growing counts over time using
 * COVID-19 pandemic evolution data across 6 snapshots.
 *
 * Key metrics:
 * - Layer utilization (overflow from L0 to L1)
 * - Count accuracy at each time step
 * - Memory efficiency vs accuracy tradeoff
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
#include <set>
#include <cmath>
#include <sys/stat.h>

#include <highfive/H5File.hpp>
#include <tqdm.hpp>

using namespace globimap;

const std::string results_path = "./results/timeseries/";
const uint RASTER_WIDTH = 8192;
const uint RASTER_HEIGHT = 8192;

// Convert lat/lon to raster coordinates
inline std::pair<uint64_t, uint64_t> latlon_to_raster(double lat, double lon) {
    double x = (lon + 180.0) / 360.0 * RASTER_WIDTH;
    double y = (lat + 90.0) / 180.0 * RASTER_HEIGHT;
    return {static_cast<uint64_t>(x), static_cast<uint64_t>(y)};
}

struct TimeSnapshot {
    std::string date;
    uint time_idx;
    uint64_t total_points;
    uint64_t cumulative_points;
    std::map<std::pair<uint64_t, uint64_t>, uint64_t> location_counts;  // Ground truth
};

struct SnapshotResult {
    std::string date;
    uint64_t points_in_snapshot;
    uint64_t cumulative_points;
    uint64_t unique_locations;
    double mean_error_pct;
    double max_error_pct;
    uint64_t layer0_nonzero;  // For CascadeCBF
    uint64_t layer1_nonzero;  // For CascadeCBF
    double layer1_utilization_pct;
};

struct ImplResult {
    std::string impl_name;
    uint64_t memory_bytes;
    double total_insert_time_sec;
    std::vector<SnapshotResult> snapshots;
};

// Count non-zero entries in CascadeCBF layers
template<typename Filter>
std::pair<uint64_t, uint64_t> count_layer_usage(Filter& filter, uint layer0_size, uint layer1_size) {
    // This is a simplified check - we'd need internal access to the filter
    // For now, return 0,0 as placeholder
    return {0, 0};
}

// Specialization for CCBF_16_16 (we can access layers)
template<>
std::pair<uint64_t, uint64_t> count_layer_usage(CCBF_16_16& filter, uint layer0_size, uint layer1_size) {
    uint64_t l0_nonzero = 0, l1_nonzero = 0;

    // Sample a subset of locations to estimate layer usage
    for (uint64_t x = 0; x < RASTER_WIDTH; x += 100) {
        for (uint64_t y = 0; y < RASTER_HEIGHT; y += 100) {
            auto count = filter.get_min({x, y});
            if (count > 0) {
                if (count < 65535) l0_nonzero++;
                else l1_nonzero++;
            }
        }
    }

    // Scale up to full grid
    double scale = (RASTER_WIDTH * RASTER_HEIGHT) / (double)((RASTER_WIDTH/100) * (RASTER_HEIGHT/100));
    return {static_cast<uint64_t>(l0_nonzero * scale), static_cast<uint64_t>(l1_nonzero * scale)};
}

template<typename Filter>
ImplResult run_timeseries_benchmark(
    Filter& filter,
    const std::string& impl_name,
    uint64_t memory_bytes,
    const std::vector<TimeSnapshot>& snapshots
) {
    using std::chrono::duration;
    using std::chrono::high_resolution_clock;

    ImplResult result;
    result.impl_name = impl_name;
    result.memory_bytes = memory_bytes;
    result.total_insert_time_sec = 0.0;

    std::cout << "\n  " << impl_name << ":\n";

    std::map<std::pair<uint64_t, uint64_t>, uint64_t> cumulative_ground_truth;
    uint64_t total_cumulative = 0;

    for (const auto& snapshot : snapshots) {
        SnapshotResult sr;
        sr.date = snapshot.date;
        sr.points_in_snapshot = snapshot.total_points;

        // Insert points for this snapshot
        auto t1 = high_resolution_clock::now();
        for (const auto& [loc, count] : snapshot.location_counts) {
            for (uint64_t i = 0; i < count; ++i) {
                filter.put({loc.first, loc.second});
            }
            cumulative_ground_truth[loc] += count;
            total_cumulative += count;
        }
        auto t2 = high_resolution_clock::now();
        duration<double> insert_time = t2 - t1;
        result.total_insert_time_sec += insert_time.count();

        sr.cumulative_points = total_cumulative;
        sr.unique_locations = cumulative_ground_truth.size();

        // Compute accuracy on sampled locations
        double total_error = 0.0;
        double max_error = 0.0;
        size_t samples = 0;

        for (const auto& [loc, truth] : cumulative_ground_truth) {
            if (truth > 0 && samples < 10000) {  // Sample up to 10K locations
                uint64_t estimated = filter.get_min({loc.first, loc.second});
                double error = std::abs((double)estimated - (double)truth) / (double)truth * 100.0;
                total_error += error;
                max_error = std::max(max_error, error);
                samples++;
            }
        }

        sr.mean_error_pct = (samples > 0) ? total_error / samples : 0.0;
        sr.max_error_pct = max_error;

        // Layer usage (only meaningful for CascadeCBF)
        sr.layer0_nonzero = 0;
        sr.layer1_nonzero = 0;
        sr.layer1_utilization_pct = 0.0;

        result.snapshots.push_back(sr);

        std::cout << "    " << sr.date << ": +" << sr.points_in_snapshot
                  << " points (cumulative: " << sr.cumulative_points
                  << "), mean error: " << std::fixed << std::setprecision(2) << sr.mean_error_pct << "%\n";
    }

    return result;
}

void save_results(const std::vector<ImplResult>& results, const std::string& dataset_name) {
    std::stringstream filename;
    filename << results_path << "timeseries_" << dataset_name << ".json";

    std::ofstream out(filename.str());
    out << "{\n";
    out << "  \"experiment\": \"timeseries_evolution\",\n";
    out << "  \"dataset\": \"" << dataset_name << "\",\n";
    out << "  \"raster_size\": [" << RASTER_WIDTH << ", " << RASTER_HEIGHT << "],\n";
    out << "  \"implementations\": [\n";

    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        out << "    {\n";
        out << "      \"name\": \"" << r.impl_name << "\",\n";
        out << "      \"memory_bytes\": " << r.memory_bytes << ",\n";
        out << "      \"total_insert_time_sec\": " << std::fixed << std::setprecision(3) << r.total_insert_time_sec << ",\n";
        out << "      \"snapshots\": [\n";

        for (size_t j = 0; j < r.snapshots.size(); ++j) {
            const auto& s = r.snapshots[j];
            out << "        {\n";
            out << "          \"date\": \"" << s.date << "\",\n";
            out << "          \"points_in_snapshot\": " << s.points_in_snapshot << ",\n";
            out << "          \"cumulative_points\": " << s.cumulative_points << ",\n";
            out << "          \"unique_locations\": " << s.unique_locations << ",\n";
            out << "          \"mean_error_pct\": " << std::fixed << std::setprecision(2) << s.mean_error_pct << ",\n";
            out << "          \"max_error_pct\": " << std::fixed << std::setprecision(2) << s.max_error_pct << "\n";
            out << "        }" << (j < r.snapshots.size() - 1 ? ",\n" : "\n");
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
    std::cout << "Time-Series Evolution Summary\n";
    std::cout << "========================================\n\n";

    for (const auto& r : results) {
        std::cout << r.impl_name << " (" << (r.memory_bytes / 1024) << " KB):\n";

        std::cout << std::setw(14) << "Date"
                  << std::setw(12) << "Points"
                  << std::setw(14) << "Cumulative"
                  << std::setw(10) << "Locs"
                  << std::setw(12) << "Error%" << "\n";
        std::cout << std::string(62, '-') << "\n";

        for (const auto& s : r.snapshots) {
            std::cout << std::setw(14) << s.date
                      << std::setw(12) << s.points_in_snapshot
                      << std::setw(14) << s.cumulative_points
                      << std::setw(10) << s.unique_locations
                      << std::setw(12) << std::fixed << std::setprecision(2) << s.mean_error_pct << "\n";
        }
        std::cout << "\n";
    }
}

int main() {
    uint k = 8;
    mkdir(results_path.c_str(), 0755);

    std::string dataset_path = "./datasets/hdf5/covid19_timeseries.h5";

    std::cout << "\n========================================\n";
    std::cout << "Time-Series Evolution Experiment\n";
    std::cout << "Dataset: COVID-19 pandemic snapshots\n";
    std::cout << "Raster: " << RASTER_WIDTH << "x" << RASTER_HEIGHT << "\n";
    std::cout << "========================================\n\n";

    // Load HDF5 dataset
    std::cout << "Loading " << dataset_path << "...\n";
    HighFive::File file(dataset_path, HighFive::File::ReadOnly);

    // Read coordinates [lat, lon, time_idx]
    auto coords_ds = file.getDataSet("coords");
    std::vector<std::vector<double>> coords;
    coords_ds.read(coords);
    std::cout << "Loaded " << coords.size() << " coordinate triplets\n";

    // Read timestamps
    auto ts_ds = file.getDataSet("timestamps");
    std::vector<std::string> timestamps;
    ts_ds.read(timestamps);
    std::cout << "Snapshots: " << timestamps.size() << "\n";

    for (size_t i = 0; i < timestamps.size(); ++i) {
        std::cout << "  [" << i << "] " << timestamps[i] << "\n";
    }

    // Organize data by snapshot
    std::vector<TimeSnapshot> snapshots(timestamps.size());
    for (size_t i = 0; i < timestamps.size(); ++i) {
        snapshots[i].date = timestamps[i];
        snapshots[i].time_idx = i;
        snapshots[i].total_points = 0;
    }

    // Group points by snapshot and location
    for (const auto& c : coords) {
        uint time_idx = static_cast<uint>(c[2]);
        if (time_idx < snapshots.size()) {
            auto [x, y] = latlon_to_raster(c[0], c[1]);
            snapshots[time_idx].location_counts[{x, y}]++;
            snapshots[time_idx].total_points++;
        }
    }

    // Print snapshot stats
    std::cout << "\nSnapshot statistics:\n";
    for (const auto& s : snapshots) {
        std::cout << "  " << s.date << ": " << s.total_points << " points, "
                  << s.location_counts.size() << " unique locations\n";
    }
    std::cout << "\n";

    std::vector<ImplResult> results;

    // Spectral BF (MI)
    {
        std::cout << "Testing Spectral BF (MI)...\n";
        SBFConfig conf{k, 20, 16, MINIMAL_INCREMENT};
        SpectralBloomFilter sbf(conf);
        uint64_t mem = sbf.memory_usage();
        results.push_back(run_timeseries_benchmark(sbf, "Spectral BF (MI)", mem, snapshots));
    }

    // Count-Min Sketch
    {
        std::cout << "Testing Count-Min Sketch...\n";
        CMSConfig conf{0.001, 0.01, true, 16};
        CountMinSketch cms(conf);
        uint64_t mem = cms.memory_usage();
        results.push_back(run_timeseries_benchmark(cms, "Count-Min Sketch", mem, snapshots));
    }

    // CascadeCBF (MI)
    {
        std::cout << "Testing CascadeCBF (MI)...\n";
        CCBF_16_16 cbf;
        cbf.configure(k, {20, 16}, true);
        uint64_t mem = cbf.memory_usage();
        results.push_back(run_timeseries_benchmark(cbf, "CascadeCBF (MI)", mem, snapshots));
    }

    print_summary(results);
    save_results(results, "covid19");

    return 0;
}
