/**
 * Polygon counting comparison experiment
 *
 * Compares multiple counting filter implementations on point-in-polygon queries:
 * - Spectral Bloom Filter (MI)
 * - Count-Min Sketch
 * - CascadeCBF (MI)
 *
 * Uses GDELT events and Natural Earth country polygons.
 */

#include "cascade_cbf.hpp"
#include "spectral_bloom_filter.hpp"
#include "count_min_sketch.hpp"
#include "dataset_config.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <sys/stat.h>

#include <highfive/H5File.hpp>
#include <tqdm.hpp>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/polygon.hpp>

#include "shapefile.hpp"

using namespace globimap;

namespace bg = boost::geometry;
typedef bg::model::point<double, 2, bg::cs::cartesian> point_t;
typedef bg::model::box<point_t> box_t;
typedef bg::model::polygon<point_t> polygon_t;

const std::string results_path = "./results/polygon/";
const uint RASTER_WIDTH = 8192;
const uint RASTER_HEIGHT = 8192;

// Convert lat/lon to raster coordinates
inline std::pair<uint64_t, uint64_t> latlon_to_raster(double lat, double lon) {
    double x = (lon + 180.0) / 360.0 * RASTER_WIDTH;
    double y = (lat + 90.0) / 180.0 * RASTER_HEIGHT;
    return {static_cast<uint64_t>(x), static_cast<uint64_t>(y)};
}

// Load polygons from shapefile
std::vector<polygon_t> load_polygons(const std::string& shapefile) {
    std::vector<polygon_t> polys;
    importSHP(shapefile, [&](SHPObject* shp, int start, int end) {
        if (shp->nSHPType == SHPT_POLYGON) {
            polygon_t poly;
            for (int i = start; i < end; ++i) {
                // X = longitude, Y = latitude in shapefiles
                bg::append(poly.outer(), point_t(shp->padfX[i], shp->padfY[i]));
            }
            polys.push_back(poly);
        }
    });
    return polys;
}

// Get bounding box of polygon in raster coordinates
std::tuple<uint64_t, uint64_t, uint64_t, uint64_t> get_raster_bbox(const polygon_t& poly) {
    box_t box;
    bg::envelope(poly, box);

    auto [min_x, min_y] = latlon_to_raster(box.min_corner().get<1>(), box.min_corner().get<0>());
    auto [max_x, max_y] = latlon_to_raster(box.max_corner().get<1>(), box.max_corner().get<0>());

    // Clamp to raster bounds
    min_x = std::max(min_x, (uint64_t)0);
    min_y = std::max(min_y, (uint64_t)0);
    max_x = std::min(max_x, (uint64_t)(RASTER_WIDTH - 1));
    max_y = std::min(max_y, (uint64_t)(RASTER_HEIGHT - 1));

    return {min_x, min_y, max_x, max_y};
}

// Check if raster cell center is within polygon
bool cell_in_polygon(uint64_t rx, uint64_t ry, const polygon_t& poly) {
    // Convert raster coords back to lat/lon (center of cell)
    double lon = (rx + 0.5) / RASTER_WIDTH * 360.0 - 180.0;
    double lat = (ry + 0.5) / RASTER_HEIGHT * 180.0 - 90.0;
    point_t pt(lon, lat);
    return bg::within(pt, poly);
}

struct PolygonResult {
    size_t polygon_idx;
    double area;           // In square degrees (approximate)
    uint64_t ground_truth; // Actual point count
    uint64_t estimated;    // Filter estimate
    double error_pct;      // Relative error
};

struct ImplResult {
    std::string impl_name;
    uint64_t memory_bytes;
    double insert_time_sec;
    double query_time_sec;
    double mean_error_pct;
    double max_error_pct;
    size_t num_polygons;
    std::vector<PolygonResult> polygon_results;
};

template<typename Filter>
ImplResult run_polygon_benchmark(
    Filter& filter,
    const std::string& impl_name,
    uint64_t memory_bytes,
    const std::vector<std::pair<uint64_t, uint64_t>>& raster_points,
    const std::vector<polygon_t>& polygons,
    const std::vector<uint64_t>& ground_truths
) {
    using std::chrono::duration;
    using std::chrono::high_resolution_clock;

    ImplResult result;
    result.impl_name = impl_name;
    result.memory_bytes = memory_bytes;
    result.num_polygons = polygons.size();

    std::cout << "  " << impl_name << ": Inserting " << raster_points.size() << " points...\n";

    // Insert all points
    auto t1 = high_resolution_clock::now();
    for (const auto& [x, y] : tq::tqdm(raster_points)) {
        filter.put({x, y});
    }
    auto t2 = high_resolution_clock::now();
    duration<double> insert_time = t2 - t1;
    result.insert_time_sec = insert_time.count();

    std::cout << "  " << impl_name << ": Querying " << polygons.size() << " polygons...\n";

    // Query each polygon
    double total_error = 0.0;
    double max_error = 0.0;
    auto tq1 = high_resolution_clock::now();

    for (size_t i = 0; i < polygons.size(); ++i) {
        const auto& poly = polygons[i];
        auto [min_x, min_y, max_x, max_y] = get_raster_bbox(poly);

        // Sum counts in polygon
        uint64_t estimated = 0;
        for (uint64_t x = min_x; x <= max_x; ++x) {
            for (uint64_t y = min_y; y <= max_y; ++y) {
                if (cell_in_polygon(x, y, poly)) {
                    estimated += filter.get_min({x, y});
                }
            }
        }

        PolygonResult pr;
        pr.polygon_idx = i;
        pr.ground_truth = ground_truths[i];
        pr.estimated = estimated;

        // Compute area (approximate)
        box_t box;
        bg::envelope(poly, box);
        pr.area = (box.max_corner().get<0>() - box.min_corner().get<0>()) *
                  (box.max_corner().get<1>() - box.min_corner().get<1>());

        // Compute error
        if (ground_truths[i] > 0) {
            pr.error_pct = std::abs((double)estimated - (double)ground_truths[i]) /
                           (double)ground_truths[i] * 100.0;
        } else {
            pr.error_pct = (estimated > 0) ? 100.0 : 0.0;
        }

        total_error += pr.error_pct;
        max_error = std::max(max_error, pr.error_pct);
        result.polygon_results.push_back(pr);
    }

    auto tq2 = high_resolution_clock::now();
    duration<double> query_time = tq2 - tq1;
    result.query_time_sec = query_time.count();
    result.mean_error_pct = total_error / polygons.size();
    result.max_error_pct = max_error;

    return result;
}

void save_results(const std::vector<ImplResult>& results, const std::string& dataset_name) {
    std::stringstream filename;
    filename << results_path << "polygon_compare_" << dataset_name << ".json";

    std::ofstream out(filename.str());
    out << "{\n";
    out << "  \"experiment\": \"polygon_counting_comparison\",\n";
    out << "  \"dataset\": \"" << dataset_name << "\",\n";
    out << "  \"raster_size\": [" << RASTER_WIDTH << ", " << RASTER_HEIGHT << "],\n";
    out << "  \"implementations\": [\n";

    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        out << "    {\n";
        out << "      \"name\": \"" << r.impl_name << "\",\n";
        out << "      \"memory_bytes\": " << r.memory_bytes << ",\n";
        out << "      \"insert_time_sec\": " << std::fixed << std::setprecision(3) << r.insert_time_sec << ",\n";
        out << "      \"query_time_sec\": " << std::fixed << std::setprecision(3) << r.query_time_sec << ",\n";
        out << "      \"mean_error_pct\": " << std::fixed << std::setprecision(2) << r.mean_error_pct << ",\n";
        out << "      \"max_error_pct\": " << std::fixed << std::setprecision(2) << r.max_error_pct << ",\n";
        out << "      \"num_polygons\": " << r.num_polygons << ",\n";
        out << "      \"top_polygons\": [\n";

        // Output top 10 polygons by ground truth
        std::vector<size_t> sorted_idx(r.polygon_results.size());
        std::iota(sorted_idx.begin(), sorted_idx.end(), 0);
        std::sort(sorted_idx.begin(), sorted_idx.end(), [&](size_t a, size_t b) {
            return r.polygon_results[a].ground_truth > r.polygon_results[b].ground_truth;
        });

        size_t top_n = std::min((size_t)10, r.polygon_results.size());
        for (size_t j = 0; j < top_n; ++j) {
            const auto& pr = r.polygon_results[sorted_idx[j]];
            out << "        {\"idx\": " << pr.polygon_idx
                << ", \"truth\": " << pr.ground_truth
                << ", \"est\": " << pr.estimated
                << ", \"error\": " << std::fixed << std::setprecision(1) << pr.error_pct << "}";
            out << (j < top_n - 1 ? ",\n" : "\n");
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
    std::cout << "Polygon Counting Summary\n";
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
                  << std::setw(12) << std::fixed << std::setprecision(2) << r.insert_time_sec
                  << std::setw(12) << std::fixed << std::setprecision(2) << r.query_time_sec
                  << std::setw(12) << std::fixed << std::setprecision(2) << r.mean_error_pct
                  << std::setw(12) << std::fixed << std::setprecision(2) << r.max_error_pct << "\n";
    }
}

int main() {
    uint k = 8;
    mkdir(results_path.c_str(), 0755);

    // Load GDELT dataset
    std::string dataset_path = dataset_config::HDF5_PATH + "/gdelt_events.h5";
    std::cout << "\n========================================\n";
    std::cout << "Polygon Counting Comparison\n";
    std::cout << "Dataset: GDELT events\n";
    std::cout << "Polygons: Natural Earth countries (50m)\n";
    std::cout << "Raster: " << RASTER_WIDTH << "x" << RASTER_HEIGHT << "\n";
    std::cout << "========================================\n\n";

    std::cout << "Loading " << dataset_path << "...\n";
    HighFive::File file(dataset_path, HighFive::File::ReadOnly);
    auto dataset = file.getDataSet("coords");

    std::vector<std::vector<double>> coords;
    dataset.read(coords);
    std::cout << "Loaded " << coords.size() << " points\n";

    // Convert to raster coordinates
    std::vector<std::pair<uint64_t, uint64_t>> raster_points;
    raster_points.reserve(coords.size());
    for (const auto& c : coords) {
        auto [x, y] = latlon_to_raster(c[0], c[1]);
        raster_points.push_back({x, y});
    }

    // Load polygons (use 50m for speed, fewer polygons)
    std::cout << "\nLoading Natural Earth 50m countries...\n";
    auto polygons = load_polygons(dataset_config::NE_COUNTRIES_50M);
    std::cout << "Loaded " << polygons.size() << " polygons\n";

    // Compute ground truth for each polygon
    std::cout << "\nComputing ground truth (points in each polygon)...\n";
    std::vector<uint64_t> ground_truths(polygons.size(), 0);

    for (size_t i = 0; i < coords.size(); ++i) {
        point_t pt(coords[i][1], coords[i][0]);  // lon, lat
        for (size_t j = 0; j < polygons.size(); ++j) {
            if (bg::within(pt, polygons[j])) {
                ground_truths[j]++;
                break;  // Point can only be in one country
            }
        }
        if (i % 100000 == 0) {
            std::cout << "  Processed " << i << "/" << coords.size() << " points\r" << std::flush;
        }
    }
    std::cout << "\n";

    // Count non-empty polygons
    size_t non_empty = 0;
    for (auto gt : ground_truths) {
        if (gt > 0) non_empty++;
    }
    std::cout << "Polygons with data: " << non_empty << "/" << polygons.size() << "\n\n";

    std::vector<ImplResult> results;

    // Spectral BF (MI)
    {
        std::cout << "Testing Spectral BF (MI)...\n";
        SBFConfig conf{k, 20, 16, MINIMAL_INCREMENT};
        SpectralBloomFilter sbf(conf);
        uint64_t mem = sbf.memory_usage();
        results.push_back(run_polygon_benchmark(sbf, "Spectral BF (MI)", mem,
                                                 raster_points, polygons, ground_truths));
        std::cout << "\n";
    }

    // Count-Min Sketch
    {
        std::cout << "Testing Count-Min Sketch...\n";
        CMSConfig conf{0.001, 0.01, true, 16};
        CountMinSketch cms(conf);
        uint64_t mem = cms.memory_usage();
        results.push_back(run_polygon_benchmark(cms, "Count-Min Sketch", mem,
                                                 raster_points, polygons, ground_truths));
        std::cout << "\n";
    }

    // CascadeCBF (MI)
    {
        std::cout << "Testing CascadeCBF (MI)...\n";
        CCBF_16_16 cbf;
        cbf.configure(k, {20, 16}, true);
        uint64_t mem = cbf.memory_usage();
        results.push_back(run_polygon_benchmark(cbf, "CascadeCBF (MI)", mem,
                                                 raster_points, polygons, ground_truths));
        std::cout << "\n";
    }

    print_summary(results);
    save_results(results, "gdelt_countries");

    return 0;
}
