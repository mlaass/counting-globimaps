/**
 * @file encode_dataset.cpp
 * @brief CLI tool to encode datasets into binary CBF files
 *
 * Usage:
 *   ./encode_dataset input.h5 output.cbf --type cms --epsilon 0.01 --delta 0.01
 */

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <cmath>
#include <ctime>

// HDF5 support
#include <highfive/H5File.hpp>
#include <highfive/H5DataSet.hpp>
#include <highfive/H5DataSpace.hpp>

// Filter implementations
#include "../include/count_min_sketch.hpp"
#include "../include/spectral_bloom_filter.hpp"
#include "../include/counting_globimap.hpp"

using namespace globimap;

// ============================================================================
// Configuration
// ============================================================================

struct EncoderConfig {
    std::string input_file;
    std::string output_file;
    std::string filter_type = "cms";  // cms, spectral, globimap

    // Count-Min Sketch params
    double epsilon = 0.01;
    double delta = 0.01;
    bool conservative = true;
    uint counter_bits = 16;

    // Spectral BF params
    uint hash_k = 8;
    uint logsize = 20;
    SBFVariant variant = MINIMAL_INCREMENT;

    // Coordinate transformation
    uint width = 3600;   // Grid width (0.1 degree resolution = 3600 cells)
    uint height = 1800;  // Grid height (0.1 degree resolution = 1800 cells)

    bool verbose = false;
};

// ============================================================================
// Helper Functions
// ============================================================================

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " INPUT.h5 OUTPUT.cbf [OPTIONS]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --type TYPE         Filter type: cms, spectral, globimap (default: cms)\n";
    std::cout << "  --epsilon FLOAT     CMS epsilon parameter (default: 0.01)\n";
    std::cout << "  --delta FLOAT       CMS delta parameter (default: 0.01)\n";
    std::cout << "  --conservative      Enable conservative update (default: true)\n";
    std::cout << "  --counter-bits N    Counter bit width: 8, 16, 32, 64 (default: 16)\n";
    std::cout << "  --hash-k N          Number of hash functions (default: 8)\n";
    std::cout << "  --logsize N         Log2 of filter size (default: 20)\n";
    std::cout << "  --width N           Grid width in cells (default: 3600)\n";
    std::cout << "  --height N          Grid height in cells (default: 1800)\n";
    std::cout << "  --verbose           Print progress information\n";
    std::cout << "  --help              Show this help message\n";
    std::cout << "\n";
    std::cout << "Examples:\n";
    std::cout << "  # Encode GDELT dataset with Count-Min Sketch\n";
    std::cout << "  " << program_name << " gdelt.h5 gdelt.cbf --type cms\n\n";
    std::cout << "  # Encode with custom parameters\n";
    std::cout << "  " << program_name << " data.h5 out.cbf --type spectral --hash-k 12 --logsize 22\n";
}

EncoderConfig parse_args(int argc, char* argv[]) {
    EncoderConfig config;

    if (argc < 3) {
        print_usage(argv[0]);
        exit(1);
    }

    config.input_file = argv[1];
    config.output_file = argv[2];

    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help") {
            print_usage(argv[0]);
            exit(0);
        } else if (arg == "--type" && i + 1 < argc) {
            config.filter_type = argv[++i];
        } else if (arg == "--epsilon" && i + 1 < argc) {
            config.epsilon = std::stod(argv[++i]);
        } else if (arg == "--delta" && i + 1 < argc) {
            config.delta = std::stod(argv[++i]);
        } else if (arg == "--conservative") {
            config.conservative = true;
        } else if (arg == "--counter-bits" && i + 1 < argc) {
            config.counter_bits = std::stoi(argv[++i]);
        } else if (arg == "--hash-k" && i + 1 < argc) {
            config.hash_k = std::stoi(argv[++i]);
        } else if (arg == "--logsize" && i + 1 < argc) {
            config.logsize = std::stoi(argv[++i]);
        } else if (arg == "--width" && i + 1 < argc) {
            config.width = std::stoi(argv[++i]);
        } else if (arg == "--height" && i + 1 < argc) {
            config.height = std::stoi(argv[++i]);
        } else if (arg == "--verbose") {
            config.verbose = true;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            exit(1);
        }
    }

    return config;
}

// ============================================================================
// Dataset Loading
// ============================================================================

/**
 * @brief Load dataset from HDF5 file
 *
 * Expected format: dataset "data" with shape (N, 2) or (N, 3)
 * Columns: [longitude, latitude] or [longitude, latitude, category]
 */
std::vector<std::vector<uint64_t>> load_dataset(
    const std::string& filename,
    const EncoderConfig& config)
{
    if (config.verbose) {
        std::cout << "Loading dataset: " << filename << "\n";
    }

    HighFive::File file(filename, HighFive::File::ReadOnly);

    // Try common dataset names
    std::string dataset_name = "data";
    if (!file.exist(dataset_name)) {
        dataset_name = "coordinates";
    }
    if (!file.exist(dataset_name)) {
        dataset_name = "coords";
    }
    if (!file.exist(dataset_name)) {
        dataset_name = "points";
    }

    HighFive::DataSet dataset = file.getDataSet(dataset_name);
    auto dims = dataset.getDimensions();

    size_t num_points = dims[0];
    size_t num_dims = dims[1];  // 2 for (lon, lat) or 3 for (lon, lat, cat)

    if (config.verbose) {
        std::cout << "  Points: " << num_points << "\n";
        std::cout << "  Dimensions: " << num_dims << "\n";
    }

    // Read data
    std::vector<std::vector<double>> raw_data;
    dataset.read(raw_data);

    // Transform to grid coordinates
    std::vector<std::vector<uint64_t>> points;
    points.reserve(num_points);

    for (const auto& row : raw_data) {
        double longitude = row[0];
        double latitude = row[1];

        // Normalize to grid: lon [-180, 180] -> [0, width], lat [-90, 90] -> [0, height]
        double x = config.width * ((longitude + 180.0) / 360.0);
        double y = config.height * ((latitude + 90.0) / 180.0);

        // Clamp to valid grid range [0, width-1] and [0, height-1]
        // This handles edge cases like lon=180.0 or lat=90.0
        uint64_t grid_x = std::min(
            static_cast<uint64_t>(config.width - 1),
            static_cast<uint64_t>(std::floor(x))
        );
        uint64_t grid_y = std::min(
            static_cast<uint64_t>(config.height - 1),
            static_cast<uint64_t>(std::floor(y))
        );

        std::vector<uint64_t> point;
        point.push_back(grid_x);
        point.push_back(grid_y);

        // Add category if present
        if (num_dims >= 3) {
            point.push_back(static_cast<uint64_t>(row[2]));
        }

        points.push_back(point);
    }

    return points;
}

// ============================================================================
// JSON Metadata Generation
// ============================================================================

void write_json_metadata(const std::string& cbf_file,
                         const EncoderConfig& config,
                         const globimap::CountMinSketch& cms,
                         size_t total_points)
{
    std::string json_file = cbf_file + ".json";
    std::ofstream json_out(json_file);

    if (!json_out) {
        throw std::runtime_error("Failed to open JSON metadata file: " + json_file);
    }

    // Get current timestamp
    auto now = std::time(nullptr);
    char timestamp[100];
    std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&now));

    // Calculate resolution in degrees
    double resolution_degrees = 360.0 / config.width;

    // Build JSON manually (avoiding external dependencies)
    json_out << "{\n";
    json_out << "  \"version\": \"1.0\",\n";
    json_out << "  \"grid\": {\n";
    json_out << "    \"width\": " << config.width << ",\n";
    json_out << "    \"height\": " << config.height << ",\n";
    json_out << "    \"resolution_degrees\": " << resolution_degrees << ",\n";
    json_out << "    \"bounds\": {\n";
    json_out << "      \"min_lat\": -90.0,\n";
    json_out << "      \"max_lat\": 90.0,\n";
    json_out << "      \"min_lng\": -180.0,\n";
    json_out << "      \"max_lng\": 180.0\n";
    json_out << "    }\n";
    json_out << "  },\n";
    json_out << "  \"filter\": {\n";
    json_out << "    \"type\": \"" << config.filter_type << "\",\n";
    json_out << "    \"epsilon\": " << cms.epsilon_actual() << ",\n";
    json_out << "    \"delta\": " << cms.delta_actual() << ",\n";
    json_out << "    \"conservative\": " << (cms.conservative() ? "true" : "false") << ",\n";
    json_out << "    \"counter_bits\": " << cms.counter_bits() << ",\n";
    json_out << "    \"width\": " << cms.width() << ",\n";
    json_out << "    \"depth\": " << cms.depth() << "\n";
    json_out << "  },\n";
    json_out << "  \"dataset\": {\n";
    json_out << "    \"total_points\": " << total_points << ",\n";
    json_out << "    \"encoded_date\": \"" << timestamp << "\"\n";
    json_out << "  }\n";
    json_out << "}\n";

    json_out.close();

    if (config.verbose) {
        std::cout << "\n✓ Metadata written to: " << json_file << "\n";
    }
}

// ============================================================================
// Encoding Functions
// ============================================================================

void encode_cms(const std::vector<std::vector<uint64_t>>& points,
                const std::string& output_file,
                const EncoderConfig& config)
{
    if (config.verbose) {
        std::cout << "Creating Count-Min Sketch...\n";
        std::cout << "  ε=" << config.epsilon << ", δ=" << config.delta << "\n";
        std::cout << "  Conservative: " << (config.conservative ? "Yes" : "No") << "\n";
        std::cout << "  Counter bits: " << config.counter_bits << "\n";
    }

    // Create filter
    CMSConfig cms_config{config.epsilon, config.delta, config.conservative, config.counter_bits};
    CountMinSketch cms(cms_config);

    if (config.verbose) {
        std::cout << "  Dimensions: " << cms.depth() << " × " << cms.width() << "\n";
        std::cout << "  Memory: " << cms.memory_usage() << " bytes\n";
        std::cout << "\nInserting points...\n";
    }

    // Insert all points
    for (size_t i = 0; i < points.size(); ++i) {
        cms.put(points[i]);

        if (config.verbose && (i + 1) % 100000 == 0) {
            std::cout << "  Inserted: " << (i + 1) << " / " << points.size() << "\n";
        }
    }

    if (config.verbose) {
        std::cout << "Serializing to binary...\n";
    }

    // Serialize
    std::vector<uint8_t> binary_data = cms.to_bytes();

    if (config.verbose) {
        std::cout << "  Binary size: " << binary_data.size() << " bytes\n";
        std::cout << "\nWriting to file: " << output_file << "\n";
    }

    // Write to file
    std::ofstream out(output_file, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Failed to open output file: " + output_file);
    }

    out.write(reinterpret_cast<const char*>(binary_data.data()), binary_data.size());
    out.close();

    if (config.verbose) {
        std::cout << "\n✓ Encoding complete!\n";
        std::cout << "  Output: " << output_file << " (" << binary_data.size() << " bytes)\n";
    }

    // Write JSON metadata sidecar
    write_json_metadata(output_file, config, cms, points.size());
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    try {
        EncoderConfig config = parse_args(argc, argv);

        // Load dataset
        auto points = load_dataset(config.input_file, config);

        // Encode based on filter type
        if (config.filter_type == "cms") {
            encode_cms(points, config.output_file, config);
        } else {
            throw std::runtime_error("Unsupported filter type: " + config.filter_type);
        }

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
