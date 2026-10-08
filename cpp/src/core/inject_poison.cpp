#include "poisoning/inject_poison.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <numeric>
#include <regex>
#include <stdexcept>

namespace poisoning {

std::vector<std::uint64_t> get_poison_values_delta_calc(const std::vector<double>& data, 
                                                   size_t poison_num) {
    if (data.empty()) {
        throw std::runtime_error("Empty data");
    }
    
    // Check if the data is sorted (for debugging)
    // assert(std::is_sorted(data.begin(), data.end()));
    
    std::vector<std::uint64_t> poison_values;
    poison_values.reserve(poison_num);
    
    auto poisoned_data = data; // Copy
    
    for (size_t iter = 0; iter < poison_num; ++iter) {
        size_t n = poisoned_data.size();
        
        double sum_x = std::accumulate(poisoned_data.begin(), poisoned_data.end(), double(0));
        
        double sum_x2 = 0;
        for (const auto& x : poisoned_data) {
            sum_x2 += x * x;
        }
        
        double s_xy = 0;
        for (size_t i = 0; i < n; ++i) {
            s_xy += poisoned_data[i] * static_cast<double>(i + 1);
        }
        
        double cur_loss = (static_cast<double>(n * n - 1) / static_cast<double>(12)) - 
                           std::pow(s_xy / n - sum_x / n * (n + 1) / static_cast<double>(2), 2) / 
                           (sum_x2 / n - std::pow(sum_x / n, 2));
        
        // Preprocessing for O(1) interval sum
        std::vector<double> prefix_sum = poisoned_data;
        for (size_t i = 1; i < n; ++i) {
            prefix_sum[i] += prefix_sum[i - 1];
        }
        double total_sum_x = prefix_sum[n - 1];
        
        double best_loss = 0;
        double best_x = 0;
        size_t best_pos = 0;
        bool found_improvement = false;
        
        for (size_t i = 0; i < n; ++i) {
            // left cand: poisoned_data[i] - 1
            if (i > 0 && poisoned_data[i - 1] < poisoned_data[i] - 1 - 1e-10) {
                double x_new = poisoned_data[i] - 1;
                size_t pos = i;
                double loss = loss_after_insert(x_new, pos, n, sum_x, sum_x2, s_xy,
                                                 total_sum_x, prefix_sum);
                if (loss > best_loss) {
                    best_loss = loss;
                    best_x = x_new;
                    best_pos = pos;
                    found_improvement = true;
                }
            }
            
            // right cand: poisoned_data[i] + 1
            if (i < n - 1 && poisoned_data[i + 1] > poisoned_data[i] + 1 + 1e-10) {
                double x_new = poisoned_data[i] + 1;
                size_t pos = i + 1;
                double loss = loss_after_insert(x_new, pos, n, sum_x, sum_x2, s_xy,
                                                 total_sum_x, prefix_sum);
                if (loss > best_loss) {
                    best_loss = loss;
                    best_x = x_new;
                    best_pos = pos;
                    found_improvement = true;
                }
            }
        }
        
        if (!found_improvement || best_loss <= cur_loss) {
            break;
        }
        
        poison_values.push_back(static_cast<std::uint64_t>(std::round(best_x)));
        
        poisoned_data.insert(poisoned_data.begin() + best_pos, best_x);
        
        // Check if the data is sorted (for debugging)
        // assert(std::is_sorted(poisoned_data.begin(), poisoned_data.end()));
    }
    
    return poison_values;
}

std::vector<std::uint64_t> get_poison_values_delta_calc_duplicate_allowed(const std::vector<double>& data, 
                                                   size_t poison_num) {
    if (data.empty()) {
        throw std::runtime_error("Empty data");
    }
    
    // Check if the data is sorted (for debugging)
    // assert(std::is_sorted(data.begin(), data.end()));
    
    std::vector<std::uint64_t> poison_values;
    poison_values.reserve(poison_num);
    
    auto poisoned_data = data; // Copy
    
    for (size_t iter = 0; iter < poison_num; ++iter) {
        size_t n = poisoned_data.size();
        
        double sum_x = std::accumulate(poisoned_data.begin(), poisoned_data.end(), double(0));
        
        double sum_x2 = 0;
        for (const auto& x : poisoned_data) {
            sum_x2 += x * x;
        }
        
        double s_xy = 0;
        for (size_t i = 0; i < n; ++i) {
            s_xy += poisoned_data[i] * static_cast<double>(i + 1);
        }
        
        double cur_loss = (static_cast<double>(n * n - 1) / static_cast<double>(12)) - 
                           std::pow(s_xy / n - sum_x / n * (n + 1) / static_cast<double>(2), 2) / 
                           (sum_x2 / n - std::pow(sum_x / n, 2));
        
        // Preprocessing for O(1) interval sum
        std::vector<double> prefix_sum = poisoned_data;
        for (size_t i = 1; i < n; ++i) {
            prefix_sum[i] += prefix_sum[i - 1];
        }
        double total_sum_x = prefix_sum[n - 1];
        
        double best_loss = 0;
        double best_x = 0;
        size_t best_pos = 0;
        bool found_improvement = false;
        
        for (size_t i = 0; i < n; ++i) {
            // left cand: poisoned_data[i] - 1
            if (i > 0 && poisoned_data[i - 1] < poisoned_data[i] - 1 - 1e-10) {
                double x_new = poisoned_data[i] - 1;
                size_t pos = i;
                double loss = loss_after_insert(x_new, pos, n, sum_x, sum_x2, s_xy,
                                                 total_sum_x, prefix_sum);
                if (loss > best_loss) {
                    best_loss = loss;
                    best_x = x_new;
                    best_pos = pos;
                    found_improvement = true;
                }
            }
            
            // right cand: poisoned_data[i] + 1
            if (i < n - 1 && poisoned_data[i + 1] > poisoned_data[i] + 1 + 1e-10) {
                double x_new = poisoned_data[i] + 1;
                size_t pos = i + 1;
                double loss = loss_after_insert(x_new, pos, n, sum_x, sum_x2, s_xy,
                                                 total_sum_x, prefix_sum);
                if (loss > best_loss) {
                    best_loss = loss;
                    best_x = x_new;
                    best_pos = pos;
                    found_improvement = true;
                }
            }
            
            // duplicate cand: poisoned_data[i] (duplicate allowed)
            double x_new = poisoned_data[i];
            size_t pos = i + 1; // Insert after the current element
            double loss = loss_after_insert(x_new, pos, n, sum_x, sum_x2, s_xy,
                                             total_sum_x, prefix_sum);
            if (loss > best_loss) {
                best_loss = loss;
                best_x = x_new;
                best_pos = pos;
                found_improvement = true;
            }
        }
        
        if (!found_improvement || best_loss <= cur_loss) {
            break;
        }
        
        poison_values.push_back(static_cast<std::uint64_t>(std::round(best_x)));
        
        poisoned_data.insert(poisoned_data.begin() + best_pos, best_x);
        
        // Check if the data is sorted (for debugging)
        // assert(std::is_sorted(poisoned_data.begin(), poisoned_data.end()));
    }
    
    return poison_values;
}

template<typename T>
std::vector<T> get_poison_values_delta_calc(const std::vector<T>& data, 
                                           size_t poison_num) {
    // Convert to double
    T offset = data.front();
    std::vector<double> data_decimal(data.size());
    for (size_t i = 0; i < data.size(); ++i) {
        data_decimal[i] = static_cast<double>(data[i] - offset);
    }
    
    auto poison_values_uint64 = get_poison_values_delta_calc(data_decimal, poison_num);
    
    // Convert back to T
    std::vector<T> poison_values(poison_values_uint64.size());
    for (size_t i = 0; i < poison_values_uint64.size(); ++i) {
        poison_values[i] = static_cast<T>(poison_values_uint64[i] + offset);
    }
    
    return poison_values;
}

template<typename T>
std::vector<T> get_poison_values_delta_calc_duplicate_allowed(const std::vector<T>& data, 
                                           size_t poison_num) {
    // Convert to double
    T offset = data.front();
    std::vector<double> data_decimal(data.size());
    for (size_t i = 0; i < data.size(); ++i) {
        data_decimal[i] = static_cast<double>(data[i] - offset);
    }
    
    auto poison_values_uint64 = get_poison_values_delta_calc_duplicate_allowed(data_decimal, poison_num);
    
    // Convert back to T
    std::vector<T> poison_values(poison_values_uint64.size());
    for (size_t i = 0; i < poison_values_uint64.size(); ++i) {
        poison_values[i] = static_cast<T>(poison_values_uint64[i] + offset);
    }
    
    return poison_values;
}

PoisonResult inject_poison_to_file(const std::string& input_file, 
                                  const std::string& output_file,
                                  size_t poison_num) {
    if (!std::filesystem::exists(input_file)) {
        throw std::runtime_error("Input file not found: " + input_file);
    }
    
    std::string filename = std::filesystem::path(input_file).filename();
    
    common::DataType dtype;
    std::string dtype_str;
    if (filename.length() >= 7 && filename.substr(filename.length() - 7) == "_uint32") {
        dtype = common::DataType::UINT32;
        dtype_str = "uint32";
    } else if (filename.length() >= 7 && filename.substr(filename.length() - 7) == "_uint64") {
        dtype = common::DataType::UINT64;
        dtype_str = "uint64";
    } else {
        throw std::runtime_error("Unknown data type for file: " + filename);
    }
    
    std::vector<std::uint64_t> poison_values;
    double mse;
    
    if (dtype == common::DataType::UINT32) {
        // Read uint32 data
        auto data = common::read_from_binary<std::uint32_t>(input_file);
        
        // Generate poison values
        auto start_time = std::chrono::high_resolution_clock::now();
        auto poison_values_uint32 = get_poison_values_delta_calc(data, poison_num);
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        double time_taken = static_cast<double>(duration.count()) / 1000000.0;
        
        // Add poison values
        std::vector<std::uint32_t> poisoned_data;
        poisoned_data.reserve(data.size() + poison_values_uint32.size());
        poisoned_data.insert(poisoned_data.end(), data.begin(), data.end());
        poisoned_data.insert(poisoned_data.end(), poison_values_uint32.begin(), poison_values_uint32.end());
        
        // Sort
        std::sort(poisoned_data.begin(), poisoned_data.end());
        
        // Calculate MSE
        mse = calc_loss(poisoned_data);
        
        // Save
        common::write_to_binary(poisoned_data, output_file);
        
        // Convert to uint64 and return
        poison_values.resize(poison_values_uint32.size());
        for (size_t i = 0; i < poison_values_uint32.size(); ++i) {
            poison_values[i] = static_cast<std::uint64_t>(poison_values_uint32[i]);
        }
        
        return {std::filesystem::path(output_file).filename().string(), static_cast<double>(mse), time_taken, poison_values};
        
    } else { // uint64
        // Read uint64 data
        auto data = common::read_from_binary<std::uint64_t>(input_file);
        
        // Generate poison values
        auto start_time = std::chrono::high_resolution_clock::now();
        poison_values = get_poison_values_delta_calc(data, poison_num);
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        double time_taken = static_cast<double>(duration.count()) / 1000000.0;
        
        // Add poison values
        std::vector<std::uint64_t> poisoned_data;
        poisoned_data.reserve(data.size() + poison_values.size());
        poisoned_data.insert(poisoned_data.end(), data.begin(), data.end());
        poisoned_data.insert(poisoned_data.end(), poison_values.begin(), poison_values.end());
        
        // Sort
        std::sort(poisoned_data.begin(), poisoned_data.end());
        
        // Calculate MSE
        mse = calc_loss(poisoned_data);
        
        // Save
        common::write_to_binary(poisoned_data, output_file);
        
        return {std::filesystem::path(output_file).filename().string(), static_cast<double>(mse), time_taken, poison_values};
    }
}

PoisonResult inject_poison_duplicate_allowed_to_file(const std::string& input_file, 
                                  const std::string& output_file,
                                  size_t poison_num) {
    if (!std::filesystem::exists(input_file)) {
        throw std::runtime_error("Input file not found: " + input_file);
    }
    
    std::string filename = std::filesystem::path(input_file).filename();
    
    common::DataType dtype;
    std::string dtype_str;
    if (filename.length() >= 7 && filename.substr(filename.length() - 7) == "_uint32") {
        dtype = common::DataType::UINT32;
        dtype_str = "uint32";
    } else if (filename.length() >= 7 && filename.substr(filename.length() - 7) == "_uint64") {
        dtype = common::DataType::UINT64;
        dtype_str = "uint64";
    } else {
        throw std::runtime_error("Unknown data type for file: " + filename);
    }
    
    std::vector<std::uint64_t> poison_values;
    double mse;
    
    if (dtype == common::DataType::UINT32) {
        // Read uint32 data
        auto data = common::read_from_binary<std::uint32_t>(input_file);
        
        // Generate poison values with duplicate allowed
        auto start_time = std::chrono::high_resolution_clock::now();
        auto poison_values_uint32 = get_poison_values_delta_calc_duplicate_allowed(data, poison_num);
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        double time_taken = static_cast<double>(duration.count()) / 1000000.0;
        
        // Add poison values
        std::vector<std::uint32_t> poisoned_data;
        poisoned_data.reserve(data.size() + poison_values_uint32.size());
        poisoned_data.insert(poisoned_data.end(), data.begin(), data.end());
        poisoned_data.insert(poisoned_data.end(), poison_values_uint32.begin(), poison_values_uint32.end());
        
        // Sort
        std::sort(poisoned_data.begin(), poisoned_data.end());
        
        // Calculate MSE
        mse = calc_loss(poisoned_data);
        
        // Save
        common::write_to_binary(poisoned_data, output_file);
        
        // Convert to uint64 and return
        poison_values.resize(poison_values_uint32.size());
        for (size_t i = 0; i < poison_values_uint32.size(); ++i) {
            poison_values[i] = static_cast<std::uint64_t>(poison_values_uint32[i]);
        }
        
        return {std::filesystem::path(output_file).filename().string(), static_cast<double>(mse), time_taken, poison_values};
        
    } else { // uint64
        // Read uint64 data
        auto data = common::read_from_binary<std::uint64_t>(input_file);
        
        // Generate poison values with duplicate allowed
        auto start_time = std::chrono::high_resolution_clock::now();
        poison_values = get_poison_values_delta_calc_duplicate_allowed(data, poison_num);
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        double time_taken = static_cast<double>(duration.count()) / 1000000.0;
        
        // Add poison values
        std::vector<std::uint64_t> poisoned_data;
        poisoned_data.reserve(data.size() + poison_values.size());
        poisoned_data.insert(poisoned_data.end(), data.begin(), data.end());
        poisoned_data.insert(poisoned_data.end(), poison_values.begin(), poison_values.end());
        
        // Sort
        std::sort(poisoned_data.begin(), poisoned_data.end());
        
        // Calculate MSE
        mse = calc_loss(poisoned_data);
        
        // Save
        common::write_to_binary(poisoned_data, output_file);
        
        return {std::filesystem::path(output_file).filename().string(), static_cast<double>(mse), time_taken, poison_values};
    }
}

template<typename T>
std::vector<T> get_optimal_poison_values_brute_force(const std::vector<T>& data, 
                                                   size_t poison_num,
                                                   bool duplicate_allowed) {
    if (data.empty()) {
        throw std::runtime_error("Empty data");
    }
    if (poison_num == 0) {
        return {};
    }
    if (data.size() < 2) {
        throw std::runtime_error("Need at least two points to place poisons between them");
    }
    if (!is_sorted(data.begin(), data.end())) {
        throw std::runtime_error("Data must be sorted");
    }

    const size_t n = data.size();
    std::vector<T> xs(n);
    for (size_t i = 0; i < n; ++i) {
        xs[i] = static_cast<T>(data[i]);
    }

    if (!duplicate_allowed) {
        // 2) Capacity of each gap (integer points)
        std::vector<size_t> cap(n - 1);
        size_t total_cap = 0;
        for (size_t i = 0; i < n - 1; ++i) {
            // xs[i]+1, ..., xs[i+1]-1 can be placed
            cap[i] = static_cast<size_t>(xs[i + 1] - xs[i] - 1);
            total_cap += cap[i];
        }

        // 3) Enumerate all (a_i, b_i) by DFS
        // a_i: number to place after xs[i], b_i: number to place before xs[i+1]
        std::vector<size_t> a(n - 1, 0), b(n - 1, 0);

        double best_loss = -std::numeric_limits<double>::infinity();
        std::vector<T> best_poisons;

        std::function<void(size_t, size_t)> dfs = [&](size_t idx, size_t remaining) {
            if (idx == n - 1) {
                // Assignments are determined, so build new_xs and poisons
                std::vector<T> new_xs;
                new_xs.reserve(n + poison_num);
                std::vector<T> poisons;
                poisons.reserve(poison_num);

                for (size_t i = 0; i < n; ++i) {
                    if (i > 0) {
                        // b[i-1] numbers (fill left side with consecutive integers) before xs[i]
                        const size_t bi = b[i - 1];
                        for (size_t j = 0; j < bi; ++j) {
                            T val = xs[i] - static_cast<T>(bi - j);
                            new_xs.push_back(val);
                            poisons.push_back(val);
                        }
                    }

                    // Original point
                    new_xs.push_back(xs[i]);

                    if (i < n - 1) {
                        // a[i] numbers (fill right side with consecutive integers) after xs[i]
                        const size_t ai = a[i];
                        for (size_t j = 1; j <= ai; ++j) {
                            T val = xs[i] + static_cast<T>(j);
                            new_xs.push_back(val);
                            poisons.push_back(val);
                        }
                    }
                }

                // 4) Loss evaluation (use given calc_loss)
                double loss = calc_loss(new_xs);
                if (loss > best_loss) {
                    best_loss = loss;
                    best_poisons = std::move(poisons);
                }
                return;
            }

            const size_t c = cap[idx];
            const size_t a_max = std::min(c, remaining);
            for (size_t ai = 0; ai <= a_max; ++ai) {
                const size_t rem_after_a = remaining - ai;
                const size_t b_max = std::min(c - ai, rem_after_a);
                for (size_t bi = 0; bi <= b_max; ++bi) {
                    a[idx] = ai;
                    b[idx] = bi;

                    if (idx > 0 && b[idx - 1] > 0 && a[idx] > 0) {
                        continue;
                    }

                    dfs(idx + 1, rem_after_a - bi);
                }
            }
        };

        dfs(0, poison_num);

        // Just in case, sort (should be already sorted)
        std::sort(best_poisons.begin(), best_poisons.end());
        return best_poisons;
    } else {
        // 2) Enumerate all (a_i) by DFS
        // a_i: number to place poisons at xs[i]
        std::vector<size_t> a(n, 0);

        double best_loss = -std::numeric_limits<double>::infinity();
        std::vector<T> best_poisons;

        std::function<void(size_t, size_t)> dfs = [&](size_t idx, size_t remaining) {
            if (idx == n) {
                // Assignments are determined, so build new_xs and poisons
                std::vector<T> new_xs;
                new_xs.reserve(n + poison_num);
                std::vector<T> poisons;
                poisons.reserve(poison_num);

                for (size_t i = 0; i < n; ++i) {
                    // Original point
                    new_xs.push_back(xs[i]);

                    // place a[i] poisons at xs[i]
                    const size_t ai = a[i];
                    for (size_t j = 0; j < ai; ++j) {
                        new_xs.push_back(xs[i]);
                        poisons.push_back(xs[i]);
                    }
                }

                // 4) Loss evaluation (use given calc_loss)
                double loss = calc_loss(new_xs);
                if (loss > best_loss) {
                    best_loss = loss;
                    best_poisons = std::move(poisons);
                }
                return;
            }

            const size_t a_max = remaining;
            for (size_t ai = 0; ai <= a_max; ++ai) {
                a[idx] = ai;
                dfs(idx + 1, remaining - ai);
            }
        };

        dfs(0, poison_num);

        // Just in case, sort (should be already sorted)
        std::sort(best_poisons.begin(), best_poisons.end());
        return best_poisons;
    }
}


// Explicit template instantiations
template std::vector<std::uint32_t> get_poison_values_delta_calc<std::uint32_t>(const std::vector<std::uint32_t>&, size_t);
template std::vector<std::uint64_t> get_poison_values_delta_calc<std::uint64_t>(const std::vector<std::uint64_t>&, size_t);
template std::vector<std::uint32_t> get_poison_values_delta_calc_duplicate_allowed<std::uint32_t>(const std::vector<std::uint32_t>&, size_t);
template std::vector<std::uint64_t> get_poison_values_delta_calc_duplicate_allowed<std::uint64_t>(const std::vector<std::uint64_t>&, size_t);
template std::vector<std::uint32_t> get_optimal_poison_values_brute_force<std::uint32_t>(const std::vector<std::uint32_t>&, size_t, bool);
template std::vector<std::uint64_t> get_optimal_poison_values_brute_force<std::uint64_t>(const std::vector<std::uint64_t>&, size_t, bool);

} // namespace poisoning 