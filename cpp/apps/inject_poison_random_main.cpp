#include "common/binary_io.h"
#include "common/dataset_info.h"
#include "poisoning/calc_loss.h"
#include <iostream>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <random>
#include <algorithm>
#include <set>

// Fixed seed for reproducible random poison selection
constexpr std::uint64_t kPoisonSelectionSeed = 42;

void print_usage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " <input_file> <output_file> <poison_num> <json_output_file>" << std::endl;
    std::cout << "Inject random poison into a single dataset file" << std::endl;
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

        std::string filename = std::filesystem::path(input_file).filename().string();

        // Determine data type from filename suffix
        if (filename.size() >= 7 && filename.substr(filename.size() - 7) == "_uint32") {
            // uint32
            auto data = common::read_from_binary<std::uint32_t>(input_file);

            if (data.empty()) {
                std::cerr << "Error: empty input data" << std::endl;
                return 1;
            }

            std::uint32_t minv = data.front();
            std::uint32_t maxv = data.back();

            std::mt19937_64 rng(kPoisonSelectionSeed);
            std::uniform_int_distribution<std::uint64_t> dist(minv, maxv);

            const size_t MAX_TRIALS = 100 * poison_num;
            std::set<std::uint32_t> data_set(data.begin(), data.end());
            std::vector<std::uint32_t> poisons;
            std::set<std::uint32_t> poisons_set;
            poisons.reserve(poison_num);
            while (poisons.size() < poison_num) {
                std::uint32_t candidate = static_cast<std::uint32_t>(dist(rng));
                if (data_set.count(candidate) > 0 || poisons_set.count(candidate) > 0) {
                    continue;
                }
                poisons.push_back(candidate);
                poisons_set.insert(candidate);
            }

            // Append and sort
            std::vector<std::uint32_t> poisoned_data;
            poisoned_data.reserve(data.size() + poisons.size());
            poisoned_data.insert(poisoned_data.end(), data.begin(), data.end());
            poisoned_data.insert(poisoned_data.end(), poisons.begin(), poisons.end());
            std::sort(poisoned_data.begin(), poisoned_data.end());

            double mse = poisoning::calc_loss(poisoned_data);

            // Save
            common::write_to_binary(poisoned_data, output_file);

            // Build JSON
            common::DatasetInfo info = common::parse_filename(input_file);
            std::ostringstream json_entry;
            json_entry << "{\n"
                      << "  \"dataset_name\": \"" << info.dataset_name << "\",\n"
                      << "  \"n\": " << info.n << ",\n"
                      << "  \"R\": " << info.R << ",\n"
                      << "  \"seed\": " << info.seed << ",\n"
                      << "  \"data_type\": \"" << info.data_type << "\",\n"
                      << "  \"lambda\": " << poison_num << ",\n"
                      << "  \"mse\": " << mse << ",\n"
                      << "  \"time\": 0\n"
                      << "}";

            std::ofstream json_file(json_output_file);
            if (json_file.is_open()) {
                json_file << json_entry.str();
                json_file.close();
            } else {
                std::cerr << "Error: Cannot open JSON output file: " << json_output_file << std::endl;
                return 1;
            }

        } else if (filename.size() >= 7 && filename.substr(filename.size() - 7) == "_uint64") {
            // uint64
            auto data = common::read_from_binary<std::uint64_t>(input_file);

            if (data.empty()) {
                std::cerr << "Error: empty input data" << std::endl;
                return 1;
            }

            std::uint64_t minv = data.front();
            std::uint64_t maxv = data.back();

            std::mt19937_64 rng(kPoisonSelectionSeed);
            std::uniform_int_distribution<std::uint64_t> dist(minv, maxv);
            
            const size_t MAX_TRIALS = 100 * poison_num;
            std::set<std::uint64_t> data_set(data.begin(), data.end());
            std::vector<std::uint64_t> poisons;
            std::set<std::uint64_t> poisons_set;
            poisons.reserve(poison_num);
            while (poisons.size() < poison_num) {
                std::uint64_t candidate = dist(rng);
                if (data_set.count(candidate) > 0 || poisons_set.count(candidate) > 0) {
                    continue;
                }
                poisons.push_back(candidate);
                poisons_set.insert(candidate);
            }

            // Append and sort
            std::vector<std::uint64_t> poisoned_data;
            poisoned_data.reserve(data.size() + poisons.size());
            poisoned_data.insert(poisoned_data.end(), data.begin(), data.end());
            poisoned_data.insert(poisoned_data.end(), poisons.begin(), poisons.end());
            std::sort(poisoned_data.begin(), poisoned_data.end());

            double mse = poisoning::calc_loss(poisoned_data);

            // Save
            common::write_to_binary(poisoned_data, output_file);

            // Build JSON
            common::DatasetInfo info = common::parse_filename(input_file);
            std::ostringstream json_entry;
            json_entry << "{\n"
                      << "  \"dataset_name\": \"" << info.dataset_name << "\",\n"
                      << "  \"n\": " << info.n << ",\n"
                      << "  \"R\": " << info.R << ",\n"
                      << "  \"seed\": " << info.seed << ",\n"
                      << "  \"data_type\": \"" << info.data_type << "\",\n"
                      << "  \"lambda\": " << poison_num << ",\n"
                      << "  \"mse\": " << mse << ",\n"
                      << "  \"time\": 0\n"
                      << "}";

            std::ofstream json_file(json_output_file);
            if (json_file.is_open()) {
                json_file << json_entry.str();
                json_file.close();
            } else {
                std::cerr << "Error: Cannot open JSON output file: " << json_output_file << std::endl;
                return 1;
            }

        } else {
            std::cerr << "Error: Unknown data type for file: " << filename << std::endl;
            return 1;
        }

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
