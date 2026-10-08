#include "poisoning/calc_upper_bound_strict.h"
#include "common/binary_io.h"
#include "common/types.h"
#include "common/dataset_info.h"
#include <iostream>
#include <vector>
#include <string>
#include <stdexcept>
#include <iomanip>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <regex>
#include <filesystem>

using namespace poisoning;


int main(int argc, char* argv[]) {
    try {
        if (argc != 4) {
            std::cerr << "Usage: " << argv[0] << " <filename> <lambda> <json_output_file>" << std::endl;
            std::cerr << "  filename: binary data file (_uint32 or _uint64)" << std::endl;
            std::cerr << "  lambda: poison number (integer)" << std::endl;
            std::cerr << "  json_output_file: JSON file to save results" << std::endl;
            std::cerr << "  Algorithm: Exact/Strict Solution (O(n log n))" << std::endl;
            return 1;
        }
        
        std::string filename = argv[1];
        int lambda = std::stoi(argv[2]);
        std::string json_output_file = argv[3];
        
        if (lambda < 0) {
            std::cerr << "Error: lambda must be non-negative" << std::endl;
            return 1;
        }
        
        // Determine file type
        common::DataType dtype = common::infer_data_type(filename);
        
        std::vector<double> xs;
        
        // Read data according to data type
        if (dtype == common::DataType::UINT32) {
            auto data = common::read_uint32_from_binary(filename);
            auto x_min = data.front();
            xs.reserve(data.size());
            for (auto x : data) {
                xs.push_back(static_cast<double>(x - x_min));  // Normalize by subtracting x_min
            }
        } else if (dtype == common::DataType::UINT64) {
            auto data = common::read_uint64_from_binary(filename);
            auto x_min = data.front();
            xs.reserve(data.size());
            for (auto x : data) {
                xs.push_back(static_cast<double>(x - x_min));  // Normalize by subtracting x_min
            }
        } else {
            std::cerr << "Error: Unsupported file type: " << filename << std::endl;
            std::cerr << "File must end with _uint32 or _uint64" << std::endl;
            return 1;
        }
        
        if (xs.empty()) {
            std::cerr << "Error: No data read from file: " << filename << std::endl;
            return 1;
        }
        
        // Check if sorted
        if (!std::is_sorted(xs.begin(), xs.end())) {
            std::cerr << "Warning: Data is not sorted, sorting now..." << std::endl;
            std::sort(xs.begin(), xs.end());
        }
        
        // Calculate upper bound (strict solution based)
        auto [w_star, mse_star, time_taken] = calc_upper_bound_strict(xs, lambda);
        
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
                  << "  \"lambda\": " << lambda << ",\n"
                  << "  \"algorithm\": \"strict\",\n"
                  << "  \"mse_upper_bound\": " << std::fixed << std::setprecision(15) << mse_star << ",\n"
                  << "  \"time\": " << time_taken << "\n"
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
        //          << info.dataset_name << " (lambda=" << lambda << ", algorithm=strict)" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown error occurred" << std::endl;
        return 1;
    }
} 