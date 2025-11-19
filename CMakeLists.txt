cmake_minimum_required(VERSION 3.12)
set(CMAKE_CXX_STANDARD 20)
project(counting-globimaps)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "-fopenmp -march=native -std=c++17 -fPIC -I/usr/include/hdf5/serial -L/usr/lib/x86_64-linux-gnu/hdf5/serial -lhdf5_cpp")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-Ofast")

# Include directories
include_directories(
    include
    lib/HighFive/include/highfive
    lib/HighFive/include
    lib/tqdm.cpp/include
    lib/tqdm-cpp
    lib/boost-headers-only
    lib/shapelib
)

# Configure HighFive
set(HIGHFIVE_USE_BOOST OFF)
set(HIGHFIVE_EXAMPLES OFF)
set(HIGHFIVE_BUILD_DOCS OFF)
set(HIGHFIVE_PARALLEL_HDF5 OFF)

# Help CMake find HDF5
set(HDF5_PREFER_PARALLEL OFF)
set(HDF5_ROOT /usr)
set(HDF5_INCLUDE_DIRS /usr/include/hdf5/serial)
set(HDF5_C_INCLUDE_DIRS /usr/include/hdf5/serial)
set(HDF5_LIBRARY_DIRS /usr/lib/x86_64-linux-gnu/hdf5/serial)

add_subdirectory(lib/HighFive/)
add_subdirectory(lib/shapelib/)

# Python module (optional - only if building with setup.py or if pybind11 is available)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/lib/pybind11/CMakeLists.txt")
    add_subdirectory(lib/pybind11)

    # Python module source
    set(PYBIND_SOURCES
        counting_globimap/pybind.cpp
    )

    # Generate Python module
    pybind11_add_module(counting_globimap ${PYBIND_SOURCES})

    # Link OpenMP for parallel processing
    if(OpenMP_CXX_FOUND)
        target_link_libraries(counting_globimap PRIVATE OpenMP::OpenMP_CXX)
    endif()
endif()

# Experiment executables
set(SOURCES_TEST_DATASET experiments/src/globimap_test_dataset.cpp)
set(SOURCES_TEST_DATASET_NEW experiments/src/globimap_test_dataset_new.cpp)
set(SOURCES_TEST_POLYGON experiments/src/globimap_test_polygon.cpp)
set(SOURCES_TEST_COS experiments/src/globimap_test_cos.cpp)
set(SOURCES_TEST_DATASETS_FOR_K experiments/src/globimap_test_datasets_for_k.cpp)

add_executable(test_datasets ${SOURCES_TEST_DATASET})
add_executable(test_datasets_new ${SOURCES_TEST_DATASET_NEW})
add_executable(test_datasets_for_k ${SOURCES_TEST_DATASETS_FOR_K})
add_executable(test_polygon ${SOURCES_TEST_POLYGON})
add_executable(test_polygons_mask experiments/src/globimap_test_polygons_mask.cpp)
add_executable(print_poly_stats experiments/src/globimap_print_poly_stats.cpp)
add_executable(globimap_rasterize_polys experiments/src/globimap_rasterize_polys.cpp)
add_executable(test_dataset_full_time experiments/src/globimap_test_dataset_full_time.cpp)
add_executable(test_cos ${SOURCES_TEST_COS})
add_executable(compare_all_implementations experiments/src/compare_all_implementations.cpp)

# Comparison experiments
add_executable(globimap_test_dataset_compare experiments/src/globimap_test_dataset_compare.cpp)
add_executable(globimap_test_cos_compare experiments/src/globimap_test_cos_compare.cpp)
add_executable(globimap_test_k_compare experiments/src/globimap_test_k_compare.cpp)

# Unit tests
add_executable(test_variable_increment_bf tests/test_variable_increment_bf.cpp)
add_executable(test_spectral_bloom_filter tests/test_spectral_bloom_filter.cpp)
add_executable(test_dleft_counting_bf tests/test_dleft_counting_bf.cpp)
add_executable(test_count_min_sketch tests/test_count_min_sketch.cpp)
add_executable(test_enhanced_globimap tests/test_enhanced_globimap.cpp)

find_package(OpenMP)
if(OpenMP_CXX_FOUND)

target_link_libraries(test_datasets
PUBLIC HighFive
PUBLIC OpenMP::OpenMP_CXX)

target_link_libraries(test_datasets_new
PUBLIC HighFive
PUBLIC OpenMP::OpenMP_CXX)

target_link_libraries(test_dataset_full_time
PUBLIC HighFive
PUBLIC OpenMP::OpenMP_CXX)

target_link_libraries(test_datasets_for_k
PUBLIC HighFive
PUBLIC OpenMP::OpenMP_CXX)

target_link_libraries(test_polygon
PUBLIC HighFive
PUBLIC shp
PUBLIC OpenMP::OpenMP_CXX
)

target_link_libraries(print_poly_stats
PUBLIC HighFive
PUBLIC shp
PUBLIC OpenMP::OpenMP_CXX
stdc++fs
)

target_link_libraries(test_polygons_mask
PUBLIC HighFive
PUBLIC shp
PUBLIC OpenMP::OpenMP_CXX
stdc++fs
)

target_link_libraries(globimap_rasterize_polys
PUBLIC HighFive
PUBLIC shp
PUBLIC OpenMP::OpenMP_CXX
)

target_link_libraries(test_cos
PUBLIC HighFive
PUBLIC OpenMP::OpenMP_CXX
)

target_link_libraries(compare_all_implementations
PUBLIC OpenMP::OpenMP_CXX
)

target_link_libraries(globimap_test_dataset_compare
PUBLIC HighFive
PUBLIC OpenMP::OpenMP_CXX
)

target_link_libraries(globimap_test_cos_compare
PUBLIC OpenMP::OpenMP_CXX
)

target_link_libraries(globimap_test_k_compare
PUBLIC OpenMP::OpenMP_CXX
)

target_link_libraries(test_variable_increment_bf
PUBLIC OpenMP::OpenMP_CXX
)

target_link_libraries(test_spectral_bloom_filter
PUBLIC OpenMP::OpenMP_CXX
)

target_link_libraries(test_dleft_counting_bf
PUBLIC OpenMP::OpenMP_CXX
)

target_link_libraries(test_count_min_sketch
PUBLIC OpenMP::OpenMP_CXX
)

target_link_libraries(test_enhanced_globimap
PUBLIC OpenMP::OpenMP_CXX
)

# Multi-category validation test
add_executable(test_multicategory tests/test_multicategory.cpp)
target_link_libraries(test_multicategory
PUBLIC OpenMP::OpenMP_CXX
)

# Multi-category performance experiment
add_executable(globimap_test_multicategory experiments/src/globimap_test_multicategory.cpp)
target_link_libraries(globimap_test_multicategory
PUBLIC OpenMP::OpenMP_CXX
)

# Multi-category dataset benchmark
add_executable(globimap_test_multicategory_dataset experiments/src/globimap_test_multicategory_dataset.cpp)
target_link_libraries(globimap_test_multicategory_dataset
PUBLIC HighFive
PUBLIC OpenMP::OpenMP_CXX
)

# Dataset encoder for WASM frontend
add_executable(encode_dataset encoder/encode_dataset.cpp)
target_link_libraries(encode_dataset
PUBLIC HighFive
PUBLIC OpenMP::OpenMP_CXX
)

endif()
