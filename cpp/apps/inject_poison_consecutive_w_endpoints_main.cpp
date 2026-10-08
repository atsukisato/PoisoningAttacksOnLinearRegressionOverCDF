#include "poisoning/inject_poison_consecutive_w_endpoints.h"
#include "common/dataset_info.h"
#include <iostream>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <sstream>

void print_usage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " <input_file> <output_file> <poison_num> <json_output_file>" << std::endl;
    std::cout << "Inject poison into a single dataset file using consecutive approach with endpoints" << std::endl;
    std::cout << std::endl;
    std::cout << "Arguments:" << std::endl;
    std::cout << "  input_file       Input dataset file path" << std::endl;
    std::cout << "  output_file      Output dataset file path" << std::endl;
    std::cout << "  poison_num       Number of poison values to inject" << std::endl;
    std::cout << "  json_output_file JSON file to append results" << std::endl;
    std::cout << std::endl;
    std::cout << "Output: JSON format with keys: dataset_name, n, R, seed, data_type, lambda, mse, time" << std::endl;
}

int main(int argc, char* argv[]) {
    try {
        if (argc < 5) {
            print_usage(argv[0]);
            return 1;
        }
        
        std::string input_file = argv[1];
        std::string output_file = argv[2];
        size_t poison_num = std::stoull(argv[3]);
        std::string json_output_file = argv[4];
        
        // Inject poison using consecutive approach with endpoints
        auto result = poisoning::inject_poison_consecutive_w_endpoints_to_file(input_file, output_file, poison_num);
        
        {
            // Extract detailed information from filename
            std::string filename = std::filesystem::path(input_file).filename().string();
            common::DatasetInfo info = common::parse_filename(filename);
            
            // Build data in JSON format
            std::ostringstream json_entry;
            json_entry << "{\n"
                      << "  \"dataset_name\": \"" << info.dataset_name << "\",\n"
                      << "  \"n\": " << info.n << ",\n"
                      << "  \"R\": " << info.R << ",\n"
                      << "  \"seed\": " << info.seed << ",\n"
                      << "  \"data_type\": \"" << info.data_type << "\",\n"
                      << "  \"lambda\": " << poison_num << ",\n"
                      << "  \"mse\": " << result.mse << ",\n"
                      << "  \"time\": " << result.time_taken << ",\n"
                      << "  \"approach\": \"consecutive_w_endpoints\"\n"
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
            //          << info.dataset_name << " (lambda=" << poison_num << ", consecutive_w_endpoints)" << std::endl;
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
} 