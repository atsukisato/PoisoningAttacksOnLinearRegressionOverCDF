#include "common/binary_io.h"
#include "common/types.h"
#include "common/dataset_info.h"
#include "poisoning/models/linear_regression_model.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <input_file> <legitimate_keys_file> <json_output_file>" << std::endl;
        std::cerr << "Example: " << argv[0] << " data/books_200M_n100_seed0_uint64 data/books_200M_n100_seed0_uint64_legitimate_keys results/query_time/result.json" << std::endl;
        std::cerr << "Output: JSON format with keys: dataset_name, n, R, seed, data_type, lambda, avg_query_time_us" << std::endl;
        return 1;
    }

    std::string filename = argv[1];
    std::string legitimate_keys_file = argv[2];
    std::string json_output_file = argv[3];
    
    try {
        // Infer data type and load data into unified vector<uint64_t>
        auto data_type = common::infer_data_type(filename);

        std::vector<std::uint64_t> xs;
        std::vector<std::uint64_t> legitimate_keys;
        if (data_type == common::DataType::UINT32) {
            auto data32 = common::read_uint32_from_binary(filename);
            auto legitimate_keys32 = common::read_uint32_from_binary(legitimate_keys_file);
            xs.assign(data32.begin(), data32.end());
            legitimate_keys.assign(legitimate_keys32.begin(), legitimate_keys32.end());
        } else { // UINT64
            xs = common::read_uint64_from_binary(filename);
            legitimate_keys = common::read_uint64_from_binary(legitimate_keys_file);
        }

        // subtract offset
        std::uint64_t offset_xs = xs.front();
        std::uint64_t offset_legitimate = legitimate_keys.front();
        for (auto& x : xs) x -= offset_xs;
        for (auto& leg_x : legitimate_keys) leg_x -= offset_legitimate;

        // Verify all legitimate keys are in the dataset
        for (auto& leg_x : legitimate_keys) {
            if (std::find(xs.begin(), xs.end(), leg_x) == xs.end()) {
                throw std::runtime_error("Error: legitimate key " + std::to_string(leg_x) + " not found in dataset!");
            }
        }

        std::vector<std::uint64_t> queries = legitimate_keys;

        // Build regression model and measure query time
        poisoning::models::LinearRegressionModel model(xs);

        // Query every element QUERY_REPEATS times. QUERY_REPEATS is a constexpr so it's optimized and easy to change.
        constexpr int QUERY_REPEATS = 1000; // number of times to query each element

        // Warm-up control: enable to stabilize caches/branch predictors before timing
        constexpr bool ENABLE_WARMUP = true;
        constexpr int WARMUP_PASSES = 10; // number of full passes for warm-up

        // Warm-up both methods before timed measurement, and assert they agree
        if (ENABLE_WARMUP && !queries.empty()) {
            for (int p = 0; p < WARMUP_PASSES; ++p) {
                for (auto q : queries) {
                    auto idx_std = model.std_lower_bound(q);
                    auto idx_model = model.lower_bound(q);
                    if (idx_std != idx_model) exit(1);
                    assert(idx_std == idx_model);
                    volatile auto sink_std = idx_std;
                    volatile auto sink_model = idx_model;
                    (void)sink_std;
                    (void)sink_model;
                }
            }
        }

        // Alternate measurement order each repeat to reduce order bias:
        // even r: std -> model, odd r: model -> std
        long long total_std_us = 0;
        long long total_model_us = 0;

        auto measure_std = [&]() {
            auto t0 = std::chrono::high_resolution_clock::now();
            for (auto q : queries) {
                volatile auto idx = model.std_lower_bound(q);
                (void)idx;
            }
            auto t1 = std::chrono::high_resolution_clock::now();
            total_std_us += std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
        };
        auto measure_model = [&]() {
            auto t0 = std::chrono::high_resolution_clock::now();
            for (auto q : queries) {
                volatile auto idx = model.lower_bound(q);
                (void)idx;
            }
            auto t1 = std::chrono::high_resolution_clock::now();
            total_model_us += std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
        };

        for (int r = 0; r < QUERY_REPEATS; ++r) {
            if (r % 2 == 0) {
                measure_std();
                measure_model();
            } else {
                measure_model();
                measure_std();
            }
        }

        const double denom = queries.empty() ? 0.0
            : static_cast<double>(queries.size()) * QUERY_REPEATS;
        const double avg_per_std_lower_bound_us = (denom == 0.0) ? 0.0
            : static_cast<double>(total_std_us) / denom;
        const double avg_per_query_us = (denom == 0.0) ? 0.0
            : static_cast<double>(total_model_us) / denom;

        // Extract detailed information from filename
        std::string basename = std::filesystem::path(filename).filename().string();
        common::DatasetInfo info = common::parse_filename(basename);
        
        // Build data in JSON format
        std::ostringstream json_entry;
        json_entry << "{\n"
            << "  \"dataset_name\": \"" << info.dataset_name << "\",\n"
            << "  \"n\": " << info.n << ",\n"
            << "  \"R\": " << info.R << ",\n"
            << "  \"seed\": " << info.seed << ",\n"
            << "  \"data_type\": \"" << info.data_type << "\",\n"
            << "  \"lambda\": " << info.lambda << ",\n"
            << "  \"avg_query_time_us\": " << std::fixed << std::setprecision(15) << avg_per_query_us << ",\n"
            << "  \"avg_std_lower_bound_query_time_us\": " << std::fixed << std::setprecision(15) << avg_per_std_lower_bound_us << "\n"
            << "}";
        
        // Write to JSON file (as individual files)
        std::ofstream json_file(json_output_file);
        if (json_file.is_open()) {
            json_file << json_entry.str();
            json_file.close();
        } else {
            std::cerr << "Error: Cannot open JSON output file: " << json_output_file << std::endl;
            return 1;
        }
        
        // Output for progress display
        // std::cout << "Added entry to " << json_output_file << ": " 
        //          << info.dataset_name << " (lambda=" << info.lambda << ")" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}