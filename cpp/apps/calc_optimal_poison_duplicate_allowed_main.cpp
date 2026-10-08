#include "poisoning/inject_poison.h"
#include "poisoning/calc_loss.h"
#include "common/binary_io.h"
#include "common/types.h"
#include "common/dataset_info.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

std::string get_data_output_filename(const std::string& filename, const size_t lambda) {
    std::string basename = filename;
    size_t last_slash = basename.find_last_of("/\\");
    if (last_slash != std::string::npos) {
        basename = basename.substr(last_slash + 1);
    }
    size_t pos = basename.find("_uint");
    if (pos != std::string::npos) {
        std::string prefix = basename.substr(0, pos);
        std::string suffix = basename.substr(pos);
        std::string new_basename = prefix + "_lambda" + std::to_string(lambda) + "_optimal_poison_duplicate_allowed" + suffix;
        std::string dir_path = filename.substr(0, last_slash + 1);
        return dir_path + new_basename;
    }
    return "optimal_poison_duplicate_allowed_" + basename;
}

int main(int argc, char* argv[]) {
    try {
        if (argc != 4) {
            std::cerr << "Usage: " << argv[0] << " <filename> <lambda> <json_output_file>" << std::endl;
            std::cerr << "  filename: binary data file (_uint32 or _uint64)" << std::endl;
            std::cerr << "  lambda: poison number (integer)" << std::endl;
            std::cerr << "  json_output_file: JSON file to save results" << std::endl;
            std::cerr << "  Algorithm: Brute-force optimal poisoning (duplicate allowed)" << std::endl;
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

        // Read data according to data type (as integers; do NOT normalize here)
        double max_mse = 0.0;
        double time_taken_s = 0.0;

        if (dtype == common::DataType::UINT32) {
            std::vector<std::uint32_t> data = common::read_uint32_from_binary(filename);
            if (data.empty()) {
                std::cerr << "Error: No data read from file: " << filename << std::endl;
                return 1;
            }
            if (!std::is_sorted(data.begin(), data.end())) {
                std::cerr << "Warning: Data is not sorted, sorting now..." << std::endl;
                std::sort(data.begin(), data.end());
            }

            auto start = std::chrono::high_resolution_clock::now();
            std::vector<std::uint32_t> poisons = poisoning::get_optimal_poison_values_brute_force<std::uint32_t>(data, static_cast<size_t>(lambda), true);
            auto end = std::chrono::high_resolution_clock::now();
            time_taken_s = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000000.0;

            std::vector<std::uint32_t> new_xs;
            new_xs.reserve(data.size() + poisons.size());
            new_xs.insert(new_xs.end(), data.begin(), data.end());
            new_xs.insert(new_xs.end(), poisons.begin(), poisons.end());
            std::sort(new_xs.begin(), new_xs.end());

            max_mse = poisoning::calc_loss<std::uint32_t>(new_xs);
            
            // Save poisoned data to file
            std::string output_filename = get_data_output_filename(filename, lambda);
            common::write_to_binary(new_xs, output_filename);
            std::cout << "Poisoned data saved to: " << output_filename << std::endl;
        } else if (dtype == common::DataType::UINT64) {
            std::vector<std::uint64_t> data = common::read_uint64_from_binary(filename);
            if (data.empty()) {
                std::cerr << "Error: No data read from file: " << filename << std::endl;
                return 1;
            }
            if (!std::is_sorted(data.begin(), data.end())) {
                std::cerr << "Warning: Data is not sorted, sorting now..." << std::endl;
                std::sort(data.begin(), data.end());
            }

            auto start = std::chrono::high_resolution_clock::now();
            std::vector<std::uint64_t> poisons = poisoning::get_optimal_poison_values_brute_force<std::uint64_t>(data, static_cast<size_t>(lambda), true);
            auto end = std::chrono::high_resolution_clock::now();
            time_taken_s = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000000.0;

            std::vector<std::uint64_t> new_xs;
            new_xs.reserve(data.size() + poisons.size());
            new_xs.insert(new_xs.end(), data.begin(), data.end());
            new_xs.insert(new_xs.end(), poisons.begin(), poisons.end());
            std::sort(new_xs.begin(), new_xs.end());

            max_mse = poisoning::calc_loss<std::uint64_t>(new_xs);
            
            // Save poisoned data to file
            std::string output_filename = get_data_output_filename(filename, lambda);
            common::write_to_binary(new_xs, output_filename);
            std::cout << "Poisoned data saved to: " << output_filename << std::endl;
        } else {
            std::cerr << "Error: Unsupported file type: " << filename << std::endl;
            std::cerr << "File must end with _uint32 or _uint64" << std::endl;
            return 1;
        }

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
                  << "  \"algorithm\": \"brute_force_duplicate_allowed\",\n"
                  << "  \"loss\": " << std::fixed << std::setprecision(15) << max_mse << ",\n"
                  << "  \"time\": " << time_taken_s << "\n"
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

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown error occurred" << std::endl;
        return 1;
    }
}

