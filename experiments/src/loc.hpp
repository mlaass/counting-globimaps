#define TUM1_ICAML_ORG 1

#ifndef TUM1_ICAML_ORG
const std::string base_path = "/media/moritz/Pro/datasets/atlas/";
const std::string vector_base_path = "/media/moritz/Pro/datasets/vector/";
const std::string experiments_path =
    "/home/moritz/workspace/bgdm/globimap/experiments/";
#else
const std::string base_path = "/home/moritz/tf/pointclouds_2d/data/";
const std::string experiments_path = "/home/moritz/tf/globimap/experiments/";
const std::string vector_base_path = "/home/moritz/tf/vector/";
#endif