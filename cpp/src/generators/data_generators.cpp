#include "poisoning/data_generators.h"
#include "common/binary_io.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>
#include <filesystem>

namespace poisoning {

template<typename T>
std::vector<T> DataGenerator::normalize_and_process(const std::vector<double>& raw_data, T R) {
    if (raw_data.empty()) {
        return std::vector<T>();
    }
    
    auto min_it = std::min_element(raw_data.begin(), raw_data.end());
    auto max_it = std::max_element(raw_data.begin(), raw_data.end());
    double min_val = *min_it;
    double max_val = *max_it;
    
    if (max_val == min_val) {
        // Handle degenerate case
        return std::vector<T>(1, static_cast<T>(0));
    }
    
    std::vector<double> normalized(raw_data.size());
    for (size_t i = 0; i < raw_data.size(); ++i) {
        normalized[i] = static_cast<double>(R) * (raw_data[i] - min_val) / (max_val - min_val);
    }
    
    std::vector<T> floored;
    floored.reserve(normalized.size());
    for (double val : normalized) {
        floored.push_back(static_cast<T>(std::floor(val)));
    }
    
    std::set<T> unique_set(floored.begin(), floored.end());
    std::vector<T> result(unique_set.begin(), unique_set.end());
    
    return result;
}

template<typename T>
std::vector<T> DataGenerator::generate_uniform(size_t n, T R) {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    std::vector<double> raw_data(n);
    
    for (size_t i = 0; i < n; ++i) {
        raw_data[i] = dist(rng_);
    }
    
    return normalize_and_process(raw_data, R);
}

template<typename T>
std::vector<T> DataGenerator::generate_normal(size_t n, T R) {
    std::normal_distribution<double> dist(0.0, 1.0);
    std::vector<double> raw_data(n);
    
    for (size_t i = 0; i < n; ++i) {
        raw_data[i] = dist(rng_);
    }
    
    return normalize_and_process(raw_data, R);
}

template<typename T>
std::vector<T> DataGenerator::generate_exponential(size_t n, T R) {
    std::exponential_distribution<double> dist(1.0);
    std::vector<double> raw_data(n);
    
    for (size_t i = 0; i < n; ++i) {
        raw_data[i] = dist(rng_);
    }
    
    return normalize_and_process(raw_data, R);
}

GenerationConfig get_generation_config(bool all_mode) {
    GenerationConfig config;
    
    if (all_mode) {
        config.distributions = {"uniform", "normal", "exponential"};
        config.ns = {100, 200, 500, 1000, 2000, 5000, 10000};
        config.Rs = {1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000, 500000, 1000000};
        config.dtypes = {"uint64"};
        
        // Seed 0~99
        for (std::uint64_t i = 0; i < 100; ++i) {
            config.seeds.push_back(i);
        }
    } else {
        config.distributions = {"uniform", "normal"};
        config.ns = {100, 1000, 5000};
        config.Rs = {1000, 10000, 100000};
        config.dtypes = {"uint64"};
        
        // Seed 0~2
        for (std::uint64_t i = 0; i < 3; ++i) {
            config.seeds.push_back(i);
        }
    }
    
    return config;
}

void generate_and_save_datasets(const GenerationConfig& config) {
    generate_and_save_datasets(config, "../data/generated/");
}

void generate_and_save_datasets(const GenerationConfig& config, const std::string& output_dir) {
    DataGenerator generator;
    
    for (const auto& dist : config.distributions) {
        for (size_t n : config.ns) {
            for (const auto& dtype_str : config.dtypes) {
                for (std::uint64_t R : config.Rs) {
                    if (n >= R) {
                        continue;
                    }
                    for (std::uint64_t seed : config.seeds) {
                        generator.set_seed(seed);
                        
                        if (dtype_str == "uint32") {
                            std::string filename = output_dir + dist + "/n" + std::to_string(n) +
                                                 "/R" + std::to_string(R) + "/seed" + std::to_string(seed) +
                                                 "/legitimate_uint32";
                            if (std::filesystem::exists(filename)) {
                                std::cerr << "[Skipped] Output file already exists: " << filename << std::endl;
                                continue;
                            }
                            std::filesystem::create_directories(std::filesystem::path(filename).parent_path());
                            std::cerr << "Outputting to " << filename << std::endl;

                            std::vector<std::uint32_t> data;
                            
                            if (dist == "uniform") {
                                data = generator.generate_uniform<std::uint32_t>(n, static_cast<std::uint32_t>(R));
                            } else if (dist == "normal") {
                                data = generator.generate_normal<std::uint32_t>(n, static_cast<std::uint32_t>(R));
                            } else if (dist == "exponential") {
                                data = generator.generate_exponential<std::uint32_t>(n, static_cast<std::uint32_t>(R));
                            }
                            
                            common::write_to_binary(data, filename);
                        } else { // uint64
                            std::string filename = output_dir + dist + "/n" + std::to_string(n) +
                                                 "/R" + std::to_string(R) + "/seed" + std::to_string(seed) +
                                                 "/legitimate_uint64";
                            if (std::filesystem::exists(filename)) {
                                std::cerr << "[Skipped] Output file already exists: " << filename << std::endl;
                                continue;
                            }
                            std::filesystem::create_directories(std::filesystem::path(filename).parent_path());
                            std::cerr << "Outputting to " << filename << std::endl;

                            std::vector<std::uint64_t> data;
                            
                            if (dist == "uniform") {
                                data = generator.generate_uniform<std::uint64_t>(n, R);
                            } else if (dist == "normal") {
                                data = generator.generate_normal<std::uint64_t>(n, R);
                            } else if (dist == "exponential") {
                                data = generator.generate_exponential<std::uint64_t>(n, R);
                            }
                            
                            common::write_to_binary(data, filename);
                        }
                    }
                    // std::cout << dist << "_n" << n << "_R" << R << "_seed[0-9]+_" << dtype_str << std::endl;
                }
            }
        }
    }
}

// Explicit template instantiation
template std::vector<std::uint32_t> DataGenerator::generate_uniform<std::uint32_t>(size_t, std::uint32_t);
template std::vector<std::uint64_t> DataGenerator::generate_uniform<std::uint64_t>(size_t, std::uint64_t);
template std::vector<std::uint32_t> DataGenerator::generate_normal<std::uint32_t>(size_t, std::uint32_t);
template std::vector<std::uint64_t> DataGenerator::generate_normal<std::uint64_t>(size_t, std::uint64_t);
template std::vector<std::uint32_t> DataGenerator::generate_exponential<std::uint32_t>(size_t, std::uint32_t);
template std::vector<std::uint64_t> DataGenerator::generate_exponential<std::uint64_t>(size_t, std::uint64_t);
template std::vector<std::uint32_t> DataGenerator::normalize_and_process<std::uint32_t>(const std::vector<double>&, std::uint32_t);
template std::vector<std::uint64_t> DataGenerator::normalize_and_process<std::uint64_t>(const std::vector<double>&, std::uint64_t);

} // namespace poisoning 