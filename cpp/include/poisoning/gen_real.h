#pragma once
#include "common/types.h"
#include <random>
#include <string>
#include <vector>

namespace poisoning {

// Real dataset information
struct RealDatasetInfo {
    std::string name;       // Dataset name (e.g., "books_200M")
    size_t total_size = 200000000;      // Total number of elements (e.g., 200_000_000)
    std::string dtype_str = "uint64";  // Data type string ("uint32" or "uint64")
};

// Real data sampling configuration
struct RealSamplingConfig {
    std::vector<RealDatasetInfo> datasets;
    std::vector<size_t> ns;         // Sample size
    std::vector<std::uint64_t> seeds;    // Random seed
};

// Real data sampler class
class RealDataSampler {
private:
    std::mt19937_64 rng_;
    
public:
    RealDataSampler(std::uint64_t seed = 0) : rng_(seed) {}
    
    // Set seed
    void set_seed(std::uint64_t seed) { rng_.seed(seed); }
    
    // Template function: Sample from dataset
    template<typename T>
    std::vector<T> sample_from_dataset(const std::vector<T>& original_data, 
                                      size_t n, 
                                      std::uint64_t seed);
};

RealSamplingConfig get_real_sampling_config(bool all_mode = false);

// Sample real data and save to file
void sample_and_save_real_datasets(const RealSamplingConfig& config);

// Sample real data and save to file (with output directory specified)
void sample_and_save_real_datasets(const RealSamplingConfig& config, const std::string& output_dir);

} // namespace poisoning 