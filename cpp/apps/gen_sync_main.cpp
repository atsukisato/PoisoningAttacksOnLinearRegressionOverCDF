#include "poisoning/data_generators.h"
#include <iostream>
#include <sstream>
#include <algorithm>

// Helper function: Convert comma-separated string to vector
std::vector<std::string> split_string(const std::string& str, char delimiter) {
    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string item;
    
    while (std::getline(ss, item, delimiter)) {
        // Remove leading and trailing whitespace
        item.erase(item.begin(), std::find_if(item.begin(), item.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        }));
        item.erase(std::find_if(item.rbegin(), item.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
        }).base(), item.end());
        
        if (!item.empty()) {
            result.push_back(item);
        }
    }
    return result;
}

// Helper function: Convert string to size_t vector
std::vector<size_t> parse_size_t_vector(const std::string& str) {
    std::vector<size_t> result;
    auto string_vec = split_string(str, ',');
    for (const auto& s : string_vec) {
        try {
            result.push_back(std::stoull(s));
        } catch (const std::exception& e) {
            throw std::invalid_argument("Invalid number: " + s);
        }
    }
    return result;
}

// Helper function: Convert string to uint64_t vector
std::vector<std::uint64_t> parse_uint64_vector(const std::string& str) {
    std::vector<std::uint64_t> result;
    auto string_vec = split_string(str, ',');
    for (const auto& s : string_vec) {
        try {
            result.push_back(std::stoull(s));
        } catch (const std::exception& e) {
            throw std::invalid_argument("Invalid number: " + s);
        }
    }
    return result;
}

int main(int argc, char* argv[]) {
    try {
        // Custom parameters
        std::vector<std::string> sync_dataset_names;
        std::vector<size_t> ns;
        std::vector<std::uint64_t> seeds;
        std::vector<std::uint64_t> Rs;
        
        bool has_params = false;
        
        // Parse command line arguments
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--sync_dataset_names" && i + 1 < argc) {
                sync_dataset_names = split_string(argv[++i], ',');
                has_params = true;
            } else if (arg == "--ns" && i + 1 < argc) {
                ns = parse_size_t_vector(argv[++i]);
                has_params = true;
            } else if (arg == "--seeds" && i + 1 < argc) {
                seeds = parse_uint64_vector(argv[++i]);
                has_params = true;
            } else if (arg == "--Rs" && i + 1 < argc) {
                Rs = parse_uint64_vector(argv[++i]);
                has_params = true;
            } else if (arg == "--help" || arg == "-h") {
                std::cout << "Usage: " << argv[0] << " [parameters]\\n";
                std::cout << "Synthetic dataset generation tool\\n\\n";
                std::cout << "Parameters:\\n";
                std::cout << "  --sync_dataset_names NAMES  Comma-separated distribution names\\n";
                std::cout << "                               (e.g., uniform,normal,exponential)\\n";
                std::cout << "  --ns SIZES                   Comma-separated sample sizes (e.g., 100,1000,5000)\\n";
                std::cout << "  --seeds SEEDS                Comma-separated random seeds (e.g., 0,1,2,3)\\n";
                std::cout << "  --Rs RANGES                  Comma-separated range values (e.g., 1000,10000,100000)\\n";
                std::cout << "  --help, -h                   Show this help message\\n\\n";
                std::cout << "Examples:\\n";
                std::cout << "  " << argv[0] << " --sync_dataset_names uniform,normal --ns 100,1000 --seeds 0,1,2\\n";
                std::cout << "  " << argv[0] << " --Rs 1000,10000 --seeds 0,1,2,3\\n";
                return 0;
            } else {
                std::cerr << "Unknown argument: " << arg << std::endl;
                std::cerr << "Use --help for usage information." << std::endl;
                return 1;
            }
        }
        
        if (!has_params) {
            std::cerr << "Error: At least one parameter must be specified." << std::endl;
            std::cerr << "Use --help for usage information." << std::endl;
            return 1;
        }
        
        // Get default settings
        auto config = poisoning::get_generation_config(false); // quick mode as default
        
        // Override with custom parameters
        if (!sync_dataset_names.empty()) {
            config.distributions = sync_dataset_names;
        }
        if (!ns.empty()) {
            config.ns = ns;
        }
        if (!seeds.empty()) {
            config.seeds = seeds;
        }
        if (!Rs.empty()) {
            config.Rs = Rs;
        }
        
        std::cout << "=== Synthetic Dataset Generation ===" << std::endl;
        std::cout << "Distributions: ";
        for (size_t i = 0; i < config.distributions.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << config.distributions[i];
        }
        std::cout << std::endl;
        
        std::cout << "Sample sizes: ";
        for (size_t i = 0; i < config.ns.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << config.ns[i];
        }
        std::cout << std::endl;
        
        std::cout << "Seeds: ";
        for (size_t i = 0; i < config.seeds.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << config.seeds[i];
        }
        std::cout << std::endl;
        
        std::cout << "Range values: ";
        for (size_t i = 0; i < config.Rs.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << config.Rs[i];
        }
        std::cout << std::endl << std::endl;
        
        // Execute data generation and file saving
        poisoning::generate_and_save_datasets(config);
        
        std::cout << std::endl << "=== Generation completed ===" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}