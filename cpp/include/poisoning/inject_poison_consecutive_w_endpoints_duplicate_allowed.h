#pragma once
#include "common/types.h"
#include "common/binary_io.h"
#include "poisoning/calc_loss.h"
#include <string>

namespace poisoning {

// Inject poison values into data using consecutive approach with endpoints and return the list of injected poison values
std::vector<std::uint64_t> get_poison_values_consecutive_w_endpoints_duplicate_allowed(const std::vector<double>& data, 
                                                   size_t poison_num);

// Template version (automatically convert uint32/uint64)
template<typename T>
std::vector<T> get_poison_values_consecutive_w_endpoints_duplicate_allowed(const std::vector<T>& data, 
                                           size_t poison_num);

// Read input file, inject poison values using consecutive approach with endpoints, and save to new file
struct PoisonResultConsecutiveWEndpointsDuplicateAllowed {
    std::string output_filename;
    double mse;
    double time_taken;
    std::vector<std::uint64_t> poison_values;
};

PoisonResultConsecutiveWEndpointsDuplicateAllowed inject_poison_consecutive_w_endpoints_duplicate_allowed_to_file(const std::string& input_file, 
                                  const std::string& output_file,
                                  size_t poison_num);

} // namespace poisoning 