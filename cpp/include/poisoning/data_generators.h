#pragma once
#include "common/types.h"
#include <random>

namespace poisoning {

class DataGenerator {
private:
    std::mt19937_64 rng_;
    
public:
    DataGenerator(std::uint64_t seed = 0) : rng_(seed) {}
    
    template<typename T>
    std::vector<T> generate_uniform(size_t n, T R);
    
    template<typename T>
    std::vector<T> generate_normal(size_t n, T R);
    
    template<typename T>
    std::vector<T> generate_exponential(size_t n, T R);
    
    // Common post-processing (normalization, casting, unique, sort)
    template<typename T>
    std::vector<T> normalize_and_process(const std::vector<double>& raw_data, T R);
    
    // Set seed
    void set_seed(std::uint64_t seed) { rng_.seed(seed); }
};

struct GenerationConfig {
    std::vector<std::string> distributions;
    std::vector<size_t> ns;
    std::vector<std::uint64_t> seeds;
    std::vector<std::uint64_t> Rs;
    std::vector<std::string> dtypes;
};

GenerationConfig get_generation_config(bool all_mode = false);

// Data generation and file output
void generate_and_save_datasets(const GenerationConfig& config);

// Data generation and file output (with output directory specified)
void generate_and_save_datasets(const GenerationConfig& config, const std::string& output_dir);

} // namespace poisoning 