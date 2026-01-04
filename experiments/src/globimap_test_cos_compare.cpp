#include "cascade_cbf.hpp"
#include "spectral_bloom_filter.hpp"
#include "dleft_counting_bf.hpp"
#include "count_min_sketch.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <math.h>
#include <string>
#include <type_traits>
#include <sys/stat.h>

using namespace globimap;

const std::string experiments_path = "./results/cosine/";

struct CosineResult {
    std::string impl_name;
    uint64_t memory_bytes;
    double insert_time_sec;
    uint64_t num_inserts;
};

template<typename Filter>
CosineResult benchmark_cosine(Filter& filter, const std::string& impl_name,
                             uint width, uint height, uint limit, uint64_t memory_bytes) {
    using std::chrono::duration;
    using std::chrono::high_resolution_clock;

    CosineResult result;
    result.impl_name = impl_name;
    result.memory_bytes = memory_bytes;
    result.num_inserts = limit;

    std::cout << "  " << impl_name << ": Inserting " << limit << " cosine-distributed points..." << std::endl;

    auto t1 = high_resolution_clock::now();

    for (auto i = 0; i < limit; i++) {
        uint64_t x = rand() % width;
        double y = (double)x / (double)width;
        y = (double)height * (1.0 + 0.5 * cos(y * M_PI * 2.0));
        std::vector<uint64_t> v{x, (uint64_t)y};
        filter.put(v);
    }

    auto t2 = high_resolution_clock::now();
    duration<double> insert_time = t2 - t1;
    result.insert_time_sec = insert_time.count();

    return result;
}

void save_results(const std::vector<CosineResult>& results,
                 uint width, uint height, uint limit) {
    std::stringstream filename;
    filename << experiments_path << "compare_cosine_w" << width
             << "h" << height << "l" << limit << ".json";

    std::ofstream out(filename.str());
    out << "{\n";
    out << "  \"experiment\": \"cosine_comparison\",\n";
    out << "  \"width\": " << width << ",\n";
    out << "  \"height\": " << height << ",\n";
    out << "  \"num_inserts\": " << limit << ",\n";
    out << "  \"implementations\": [\n";

    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        out << "    {\n";
        out << "      \"name\": \"" << r.impl_name << "\",\n";
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
    uint width = 8192, height = 8192, limit = 65536;
    uint k = 8;

    // Create results directory if it doesn't exist
    mkdir(experiments_path.c_str(), 0755);

    std::cout << "\n========================================\n";
    std::cout << "Cosine Distribution Benchmark\n";
    std::cout << "Width: " << width << ", Height: " << height << "\n";
    std::cout << "Inserts: " << limit << "\n";
    std::cout << "========================================\n\n";

    std::vector<CosineResult> results;

    // Spectral BF (MI) - 128 KB
    {
        std::cout << "Testing Spectral BF (MI)...\n";
        SBFConfig conf{k, 16, 16, MINIMAL_INCREMENT};
        SpectralBloomFilter sbf(conf);
        uint64_t mem = sbf.memory_usage();
        results.push_back(benchmark_cosine(sbf, "Spectral BF (MI)", width, height, limit, mem));
    }

    // d-Left CBF - 95 KB
    {
        std::cout << "\nTesting d-Left CBF...\n";
        DLeftCBFConfig conf{4864, 4, 4, 12, 16};
        DLeftCountingBloomFilter dleft(conf);
        uint64_t mem = dleft.memory_usage();
        results.push_back(benchmark_cosine(dleft, "d-Left CBF", width, height, limit, mem));
    }

    // Count-Min Sketch - 88.5 KB
    {
        std::cout << "\nTesting Count-Min Sketch...\n";
        CMSConfig conf{0.0003, 0.01, false, 16};
        CountMinSketch cms(conf);
        uint64_t mem = cms.memory_usage();
        results.push_back(benchmark_cosine(cms, "Count-Min Sketch", width, height, limit, mem));
    }

    // CascadeCBF (MI) - ~160 KB
    {
        std::cout << "\nTesting CascadeCBF (MI)...\n";
        CCBF_16_16 cbf;
        cbf.configure(k, {16, 14}, true);  // k=8, layer0=2^16, layer1=2^14
        uint64_t mem = cbf.memory_usage();
        results.push_back(benchmark_cosine(cbf, "CascadeCBF (MI)", width, height, limit, mem));
    }

    // Print summary
    std::cout << "\n========================================\n";
    std::cout << "Summary\n";
    std::cout << "========================================\n\n";
    std::cout << std::left << std::setw(25) << "Implementation"
              << std::right << std::setw(12) << "Memory"
              << std::setw(18) << "Insert Time (s)"
              << std::setw(18) << "Inserts/sec" << "\n";
    std::cout << std::string(73, '-') << "\n";

    for (const auto& r : results) {
        std::cout << std::left << std::setw(25) << r.impl_name
                  << std::right << std::setw(12) << (r.memory_bytes / 1024) << " KB"
                  << std::setw(18) << std::fixed << std::setprecision(6) << r.insert_time_sec
                  << std::setw(18) << std::fixed << std::setprecision(0) << (r.num_inserts / r.insert_time_sec)
                  << "\n";
    }
    std::cout << "\n";

    save_results(results, width, height, limit);

    return 0;
}
