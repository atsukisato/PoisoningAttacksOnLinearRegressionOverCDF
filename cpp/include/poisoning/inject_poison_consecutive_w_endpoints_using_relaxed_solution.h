#pragma once
#include "common/types.h"
#include "common/binary_io.h"
#include "poisoning/calc_loss.h"
#include "poisoning/inject_poison_consecutive_w_endpoints_duplicate_allowed.h"
#include <string>

namespace poisoning {

// Inject poison values into data using consecutive approach with endpoints using relaxed solution and return the list of injected poison values
std::vector<std::uint64_t> get_poison_values_consecutive_w_endpoints_using_relaxed_solution(const std::vector<double>& data, 
                                                   size_t poison_num);

// Template version (automatically convert uint32/uint64)
template<typename T>
std::vector<T> get_poison_values_consecutive_w_endpoints_using_relaxed_solution(const std::vector<T>& data, 
                                           size_t poison_num);

// Template version (automatically convert uint32/uint64)
template<typename T>
std::vector<T> get_optimal_poison_values_consecutive_w_endpoints_using_relaxed_solution_brute_force(const std::vector<T>& data, 
                                                   size_t poison_num,
                                                   bool duplicate_allowed = false);

// Read input file, inject poison values using consecutive approach with endpoints using relaxed solution, and save to new file
struct PoisonResultConsecutiveWEndpointsUsingRelaxedSolution {
    std::string output_filename;
    double mse;
    double time_taken;
    std::vector<std::uint64_t> poison_values;
};

PoisonResultConsecutiveWEndpointsUsingRelaxedSolution inject_poison_consecutive_w_endpoints_using_relaxed_solution_to_file(const std::string& input_file, 
                                  const std::string& output_file,
                                  size_t poison_num);

} // namespace poisoning
