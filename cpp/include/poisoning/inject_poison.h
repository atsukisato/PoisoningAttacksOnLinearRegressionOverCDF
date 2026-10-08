#pragma once
#include "common/types.h"
#include "common/binary_io.h"
#include "poisoning/calc_loss.h"
#include <string>

namespace poisoning {

// Inject poison values into data and return the list of injected poison values
std::vector<std::uint64_t> get_poison_values_delta_calc(const std::vector<double>& data, 
                                                   size_t poison_num);

// Template version (automatically convert uint32/uint64)
template<typename T>
std::vector<T> get_poison_values_delta_calc(const std::vector<T>& data, 
                                           size_t poison_num);

// Inject poison values into data with duplicate allowed and return the list of injected poison values
std::vector<std::uint64_t> get_poison_values_delta_calc_duplicate_allowed(const std::vector<double>& data, 
                                                   size_t poison_num);

// Template version (automatically convert uint32/uint64) with duplicate allowed
template<typename T>
std::vector<T> get_poison_values_delta_calc_duplicate_allowed(const std::vector<T>& data, 
                                           size_t poison_num);

// Template version (automatically convert uint32/uint64)
template<typename T>
std::vector<T> get_optimal_poison_values_brute_force(const std::vector<T>& data, 
                                                   size_t poison_num,
                                                   bool duplicate_allowed = false);

// Read input file, inject poison values, and save to new file
struct PoisonResult {
    std::string output_filename;
    double mse;
    double time_taken;
    std::vector<std::uint64_t> poison_values;
};

PoisonResult inject_poison_to_file(const std::string& input_file, 
                                  const std::string& output_file,
                                  size_t poison_num);

// Read input file, inject poison values with duplicate allowed, and save to new file
PoisonResult inject_poison_duplicate_allowed_to_file(const std::string& input_file, 
                                  const std::string& output_file,
                                  size_t poison_num);

} // namespace poisoning 