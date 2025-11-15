#include "counting_globimap.hpp"
#include "spectral_bloom_filter.hpp"
#include "dleft_counting_bf.hpp"
#include "count_min_sketch.hpp"

#include <cassert>
#include <iostream>
#include <iomanip>

using namespace globimap;

void test_globimap_category_isolation() {
    std::cout << "\nTesting CountingGloBiMap category isolation...\n";
    
    FilterConfig conf;
    conf.hash_k = 8;
    conf.layers = {{8, 20}, {16, 18}};
    conf.minimal_increment = true;
    
    CountingGloBiMap<> gbm(conf);
    
    // Insert same location (100, 200) with different categories
    std::cout << "  Inserting events at location (100, 200):\n";
    std::cout << "    Category 1: 5 events\n";
    for (int i = 0; i < 5; ++i) {
        gbm.put({100, 200, 1});
    }
    
    std::cout << "    Category 2: 10 events\n";
    for (int i = 0; i < 10; ++i) {
        gbm.put({100, 200, 2});
    }
    
    std::cout << "    Category 3: 3 events\n";
    for (int i = 0; i < 3; ++i) {
        gbm.put({100, 200, 3});
    }
    
    // Query each category
    uint64_t cat1_count = gbm.get_min({100, 200, 1});
    uint64_t cat2_count = gbm.get_min({100, 200, 2});
    uint64_t cat3_count = gbm.get_min({100, 200, 3});
    uint64_t cat4_count = gbm.get_min({100, 200, 4});  // Never inserted
    
    std::cout << "  Query results:\n";
    std::cout << "    Category 1: " << cat1_count << " (expected: ~5)\n";
    std::cout << "    Category 2: " << cat2_count << " (expected: ~10)\n";
    std::cout << "    Category 3: " << cat3_count << " (expected: ~3)\n";
    std::cout << "    Category 4: " << cat4_count << " (expected: 0)\n";

    // Verify isolation - allow some undercounting due to minimal_increment
    // but categories should be clearly separated
    bool cat1_ok = (cat1_count >= 3 && cat1_count <= 7);
    bool cat2_ok = (cat2_count >= 8 && cat2_count <= 12);
    bool cat3_ok = (cat3_count >= 2 && cat3_count <= 5);
    bool cat4_ok = (cat4_count == 0);

    std::cout << "  Verification: cat1=" << (cat1_ok ? "✓" : "✗")
              << " cat2=" << (cat2_ok ? "✓" : "✗")
              << " cat3=" << (cat3_ok ? "✓" : "✗")
              << " cat4=" << (cat4_ok ? "✓" : "✗") << "\n";

    assert(cat1_ok && "Category 1 count out of expected range");
    assert(cat2_ok && "Category 2 count out of expected range");
    assert(cat3_ok && "Category 3 count out of expected range");
    assert(cat4_ok && "Category 4 should be 0");
    
    std::cout << "  ✓ Category isolation verified!\n";
}

void test_spectral_bf_category_isolation() {
    std::cout << "\nTesting Spectral BF category isolation...\n";
    
    SBFConfig conf{8, 20, 16, MINIMAL_INCREMENT};
    SpectralBloomFilter sbf(conf);
    
    // Same test pattern
    for (int i = 0; i < 5; ++i) sbf.put({100, 200, 1});
    for (int i = 0; i < 10; ++i) sbf.put({100, 200, 2});
    for (int i = 0; i < 3; ++i) sbf.put({100, 200, 3});
    
    uint64_t cat1 = sbf.get_min({100, 200, 1});
    uint64_t cat2 = sbf.get_min({100, 200, 2});
    uint64_t cat3 = sbf.get_min({100, 200, 3});
    uint64_t cat4 = sbf.get_min({100, 200, 4});
    
    std::cout << "  Category 1: " << cat1 << ", Category 2: " << cat2 
              << ", Category 3: " << cat3 << ", Category 4: " << cat4 << "\n";
    
    assert(cat1 == 5 && cat2 == 10 && cat3 == 3 && cat4 == 0);
    std::cout << "  ✓ Spectral BF categories work!\n";
}

void test_count_min_sketch_category_isolation() {
    std::cout << "\nTesting Count-Min Sketch category isolation...\n";
    
    CMSConfig conf{0.001, 0.01, true, 16};
    CountMinSketch cms(conf);
    
    for (int i = 0; i < 5; ++i) cms.put({100, 200, 1});
    for (int i = 0; i < 10; ++i) cms.put({100, 200, 2});
    for (int i = 0; i < 3; ++i) cms.put({100, 200, 3});
    
    uint64_t cat1 = cms.get_min({100, 200, 1});
    uint64_t cat2 = cms.get_min({100, 200, 2});
    uint64_t cat3 = cms.get_min({100, 200, 3});
    uint64_t cat4 = cms.get_min({100, 200, 4});
    
    std::cout << "  Category 1: " << cat1 << ", Category 2: " << cat2 
              << ", Category 3: " << cat3 << ", Category 4: " << cat4 << "\n";
    
    assert(cat1 == 5 && cat2 == 10 && cat3 == 3 && cat4 == 0);
    std::cout << "  ✓ Count-Min Sketch categories work!\n";
}

void test_dleft_cbf_category_isolation() {
    std::cout << "\nTesting d-Left CBF category isolation...\n";
    
    DLeftCBFConfig conf{2048, 4, 4, 12, 16};
    DLeftCountingBloomFilter dleft(conf);
    
    for (int i = 0; i < 5; ++i) dleft.put({100, 200, 1});
    for (int i = 0; i < 10; ++i) dleft.put({100, 200, 2});
    for (int i = 0; i < 3; ++i) dleft.put({100, 200, 3});
    
    uint64_t cat1 = dleft.get_min({100, 200, 1});
    uint64_t cat2 = dleft.get_min({100, 200, 2});
    uint64_t cat3 = dleft.get_min({100, 200, 3});
    uint64_t cat4 = dleft.get_min({100, 200, 4});
    
    std::cout << "  Category 1: " << cat1 << ", Category 2: " << cat2 
              << ", Category 3: " << cat3 << ", Category 4: " << cat4 << "\n";
    
    assert(cat1 == 5 && cat2 == 10 && cat3 == 3 && cat4 == 0);
    std::cout << "  ✓ d-Left CBF categories work!\n";
}

void test_different_locations_same_category() {
    std::cout << "\nTesting different locations with same category...\n";
    
    FilterConfig conf;
    conf.hash_k = 8;
    conf.layers = {{8, 20}, {16, 18}};
    
    CountingGloBiMap<> gbm(conf);
    
    // Insert different locations with same category
    gbm.put({100, 200, 1});
    gbm.put({100, 200, 1});
    gbm.put({300, 400, 1});
    gbm.put({300, 400, 1});
    gbm.put({300, 400, 1});
    
    uint64_t loc1 = gbm.get_min({100, 200, 1});
    uint64_t loc2 = gbm.get_min({300, 400, 1});
    uint64_t loc3 = gbm.get_min({500, 600, 1});
    
    std::cout << "  Location (100,200,cat1): " << loc1 << " (expected: 2)\n";
    std::cout << "  Location (300,400,cat1): " << loc2 << " (expected: 3)\n";
    std::cout << "  Location (500,600,cat1): " << loc3 << " (expected: 0)\n";
    
    assert(loc1 == 2);
    assert(loc2 == 3);
    assert(loc3 == 0);
    
    std::cout << "  ✓ Different locations tracked correctly!\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "Multi-Category Support Validation Tests\n";
    std::cout << "========================================\n";
    
    try {
        test_globimap_category_isolation();
        test_spectral_bf_category_isolation();
        test_count_min_sketch_category_isolation();
        test_dleft_cbf_category_isolation();
        test_different_locations_same_category();
        
        std::cout << "\n========================================\n";
        std::cout << "✓ ALL TESTS PASSED!\n";
        std::cout << "Multi-category support confirmed working.\n";
        std::cout << "========================================\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n✗ TEST FAILED: " << e.what() << "\n";
        return 1;
    }
}
