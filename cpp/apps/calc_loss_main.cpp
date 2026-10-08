#include "common/binary_io.h"
#include "poisoning/calc_loss.h"
#include "common/types.h"
#include "common/dataset_info.h"
#include <iostream>
#include <iomanip>
#include <filesystem>
#include <fstream>
#include <sstream>

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <input_file> <legitimate_file> <json_output_file>" << std::endl;
        std::cerr << "Example: " << argv[0] << " data/books_200M_n100_seed0_lambda10_uint64 data/books_200M_n100_seed0_uint64 results/loss/result.json" << std::endl;
        std::cerr << "Output: JSON format with keys: dataset_name, n, R, seed, data_type, lambda, loss" << std::endl;
        return 1;
    }
    
    std::string filename = argv[1];
    std::string legitimate_filename = argv[2];
    std::string json_output_file = argv[3];
    
    try {
        // Infer data type
        auto data_type = common::infer_data_type(filename);
        
        double loss;
        double max_point_error_MSE_train;
        double max_point_error_minimax_train;
        double mean_point_error_MSE_train;
        double mean_log_point_error_MSE_train;
        double loss_on_legitimate;
        double max_point_error_MSE_train_on_legitimate;
        double max_point_error_minimax_train_on_legitimate;
        double mean_point_error_MSE_train_on_legitimate;
        double mean_log_point_error_MSE_train_on_legitimate;
        
        if (data_type == common::DataType::UINT32) {
            auto data = common::read_uint32_from_binary(filename);
            auto legitimate_data = common::read_uint32_from_binary(legitimate_filename);
            // subtract offset
            std::uint64_t offset_xs = data.front();
            std::uint64_t offset_legitimate = legitimate_data.front();
            for (auto& x : data) x -= offset_xs;
            for (auto& leg_x : legitimate_data) leg_x -= offset_legitimate;
            // Verify all legitimate keys are in the dataset
            for (auto& leg_x : legitimate_data) {
                if (std::lower_bound(data.begin(), data.end(), leg_x) == data.end() || *(std::lower_bound(data.begin(), data.end(), leg_x)) != leg_x) {
                    throw std::runtime_error("Error: legitimate key " + std::to_string(leg_x) + " not found in dataset!");
                }
            }
            std::vector<std::uint64_t> xs(data.begin(), data.end());
            std::vector<std::uint64_t> legitimate_xs(legitimate_data.begin(), legitimate_data.end());
            loss = poisoning::calc_loss(xs);
            max_point_error_MSE_train = poisoning::calc_max_point_error_MSE_train(xs);
            max_point_error_minimax_train = poisoning::calc_max_point_error_minimax_train(xs);
            mean_point_error_MSE_train = poisoning::calc_mean_point_error_MSE_train(xs);
            mean_log_point_error_MSE_train = poisoning::calc_mean_log_point_error_MSE_train(xs);
            loss_on_legitimate = poisoning::calc_loss_on_legitimate(xs, legitimate_xs);
            max_point_error_MSE_train_on_legitimate = poisoning::calc_max_point_error_MSE_train_on_legitimate(xs, legitimate_xs);
            max_point_error_minimax_train_on_legitimate = poisoning::calc_max_point_error_minimax_train_on_legitimate(xs, legitimate_xs);
            mean_point_error_MSE_train_on_legitimate = poisoning::calc_mean_point_error_MSE_train_on_legitimate(xs, legitimate_xs);
            mean_log_point_error_MSE_train_on_legitimate = poisoning::calc_mean_log_point_error_MSE_train_on_legitimate(xs, legitimate_xs);
        } else { // UINT64
            auto data = common::read_uint64_from_binary(filename);
            auto legitimate_data = common::read_uint64_from_binary(legitimate_filename);
            // subtract offset
            std::uint64_t offset_xs = data.front();
            std::uint64_t offset_legitimate = legitimate_data.front();
            for (auto& x : data) x -= offset_xs;
            for (auto& leg_x : legitimate_data) leg_x -= offset_legitimate;
            // Verify all legitimate keys are in the dataset
            for (auto& leg_x : legitimate_data) {
                if (std::lower_bound(data.begin(), data.end(), leg_x) == data.end() || *(std::lower_bound(data.begin(), data.end(), leg_x)) != leg_x) {
                    throw std::runtime_error("Error: legitimate key " + std::to_string(leg_x) + " not found in dataset!");
                }
            }
            loss = poisoning::calc_loss(data);
            max_point_error_MSE_train = poisoning::calc_max_point_error_MSE_train(data);
            max_point_error_minimax_train = poisoning::calc_max_point_error_minimax_train(data);
            mean_point_error_MSE_train = poisoning::calc_mean_point_error_MSE_train(data);
            mean_log_point_error_MSE_train = poisoning::calc_mean_log_point_error_MSE_train(data);
            loss_on_legitimate = poisoning::calc_loss_on_legitimate(data, legitimate_data);
            max_point_error_MSE_train_on_legitimate = poisoning::calc_max_point_error_MSE_train_on_legitimate(data, legitimate_data);
            max_point_error_minimax_train_on_legitimate = poisoning::calc_max_point_error_minimax_train_on_legitimate(data, legitimate_data);
            mean_point_error_MSE_train_on_legitimate = poisoning::calc_mean_point_error_MSE_train_on_legitimate(data, legitimate_data);
            mean_log_point_error_MSE_train_on_legitimate = poisoning::calc_mean_log_point_error_MSE_train_on_legitimate(data, legitimate_data);
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
                  << "  \"lambda\": " << info.lambda << ",\n"
                  << "  \"loss\": " << std::fixed << std::setprecision(15) << loss << ",\n"
                  << "  \"max_point_error_MSE_train\": " << std::fixed << std::setprecision(15) << max_point_error_MSE_train << ",\n"
                  << "  \"max_point_error_minimax_train\": " << std::fixed << std::setprecision(15) << max_point_error_minimax_train << ",\n"
                  << "  \"mean_point_error_MSE_train\": " << std::fixed << std::setprecision(15) << mean_point_error_MSE_train << ",\n"
                  << "  \"mean_log_point_error_MSE_train\": " << std::fixed << std::setprecision(15) << mean_log_point_error_MSE_train << ",\n"
                  << "  \"loss_on_legitimate\": " << std::fixed << std::setprecision(15) << loss_on_legitimate << ",\n"
                  << "  \"max_point_error_MSE_train_on_legitimate\": " << std::fixed << std::setprecision(15) << max_point_error_MSE_train_on_legitimate << ",\n"
                  << "  \"max_point_error_minimax_train_on_legitimate\": " << std::fixed << std::setprecision(15) << max_point_error_minimax_train_on_legitimate << ",\n"
                  << "  \"mean_point_error_MSE_train_on_legitimate\": " << std::fixed << std::setprecision(15) << mean_point_error_MSE_train_on_legitimate << ",\n"
                  << "  \"mean_log_point_error_MSE_train_on_legitimate\": " << std::fixed << std::setprecision(15) << mean_log_point_error_MSE_train_on_legitimate << "\n"
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