#include "poisoning/gen_real.h"
#include <iostream>
#include <string>
#include <sstream>
#include <algorithm>

// Helper function: Convert comma-separated string to vector
std::vector<std::string> split_string(const std::string& str, char delimiter) {
    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string item;
    
    while (std::getline(ss, item, delimiter)) {
        // Remove leading and trailing spaces
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

// Helper function: Create RealDatasetInfo
poisoning::RealDatasetInfo parse_dataset_info(const std::string& dataset_spec) {
    // Format: "name:total_size:dtype" or "name" (use defaults)
    auto parts = split_string(dataset_spec, ':');
    
    if (parts.size() == 1) {
        // Use default values
        if (parts[0] == "books_200M") {
            return {"books_200M", 200000000, "uint64"};
        } else if (parts[0] == "fb_200M") {
            return {"fb_200M", 200000000, "uint64"};
        } else if (parts[0] == "osm_cellids_200M") {
            return {"osm_cellids_200M", 200000000, "uint64"};
        } else {
            throw std::invalid_argument("Unknown dataset: " + parts[0]);
        }
    } else if (parts.size() == 3) {
        // Fully specified
        size_t total_size = std::stoull(parts[1]);
        return {parts[0], total_size, parts[2]};
    } else {
        throw std::invalid_argument("Invalid dataset specification: " + dataset_spec);
    }
}

int main(int argc, char* argv[]) {
    // Custom parameters
    std::vector<poisoning::RealDatasetInfo> real_datasets;
    std::vector<size_t> ns;
    std::vector<std::uint64_t> seeds;
    
    bool has_params = false;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--real_dataset_names" && i + 1 < argc) {
            auto dataset_specs = split_string(argv[++i], ',');
            real_datasets.clear();
            for (const auto& spec : dataset_specs) {
                real_datasets.push_back(parse_dataset_info(spec));
            }
            has_params = true;
        } else if (arg == "--ns" && i + 1 < argc) {
            ns = parse_size_t_vector(argv[++i]);
            has_params = true;
        } else if (arg == "--seeds" && i + 1 < argc) {
            seeds = parse_uint64_vector(argv[++i]);
            has_params = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [parameters]\\n";
            std::cout << "Real dataset sampling tool\\n\\n";
            std::cout << "Parameters:\\n";
            std::cout << "  --real_dataset_names NAMES   Comma-separated dataset names\\n";
            std::cout << "                                Format: name or name:size:dtype\\n";
            std::cout << "                                Examples: books_200M,fb_200M\\n";
            std::cout << "                                         books_200M:200000000:uint64\\n";
            std::cout << "  --ns SIZES                    Comma-separated sample sizes (e.g., 100,1000,5000)\\n";
            std::cout << "  --seeds SEEDS                 Comma-separated random seeds (e.g., 0,1,2,3)\\n";
            std::cout << "  --help, -h                    Show this help message\\n\\n";
            std::cout << "Available datasets: books_200M, fb_200M, osm_cellids_200M\\n";
            std::cout << "Expected data files location: ../data/raw/\\n";
            std::cout << "Output files location: ../data/generated/\\n\\n";
            std::cout << "Examples:\\n";
            std::cout << "  " << argv[0] << " --real_dataset_names books_200M,fb_200M --ns 100,1000 --seeds 0,1,2\\n";
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
    
    try {
        // Get default settings
        poisoning::RealSamplingConfig config = poisoning::get_real_sampling_config(false); // quick mode as default
        
        // Override with custom parameters
        if (!real_datasets.empty()) {
            config.datasets = real_datasets;
        }
        if (!ns.empty()) {
            config.ns = ns;
        }
        if (!seeds.empty()) {
            config.seeds = seeds;
        }
        
        std::cout << "=== Real Dataset Sampling (C++ Implementation) ===" << std::endl;
        std::cout << "Datasets to process: " << config.datasets.size() << std::endl;
        for (const auto& dataset : config.datasets) {
            std::cout << "  - " << dataset.name << " (" << dataset.total_size << ", " << dataset.dtype_str << ")" << std::endl;
        }
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
        std::cout << std::endl << std::endl;
        
        // Actual sampling and file output
        poisoning::sample_and_save_real_datasets(config);
        
        std::cout << std::endl << "=== Sampling completed ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}