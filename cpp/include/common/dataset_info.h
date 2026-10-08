#pragma once

#include <string>
#include <regex>

namespace common {

struct DatasetInfo {
    std::string dataset_name;
    std::string n;
    std::string R;
    std::string seed;
    std::string data_type;
    std::string lambda;
};

inline DatasetInfo parse_filename(const std::string& filename) {
    DatasetInfo info;

    if (filename.find("_uint32") != std::string::npos) {
        info.data_type = "uint32";
    } else if (filename.find("_uint64") != std::string::npos) {
        info.data_type = "uint64";
    } else {
        info.data_type = "unknown";
    }

    std::regex seed_regex("seed(\\d+)");
    std::smatch seed_match;
    if (std::regex_search(filename, seed_match, seed_regex)) {
        info.seed = seed_match[1].str();
    } else {
        info.seed = "0";
    }

    std::regex lambda_regex("lambda(\\d+)");
    std::smatch lambda_match;
    if (std::regex_search(filename, lambda_match, lambda_regex)) {
        info.lambda = lambda_match[1].str();
    } else {
        info.lambda = "0";
    }

    std::regex n_regex("_n(\\d+)_");
    std::smatch n_match;
    if (std::regex_search(filename, n_match, n_regex)) {
        info.n = n_match[1].str();
    } else {
        info.n = "unknown";
    }

    if (filename.find("uniform_") == 0 || filename.find("normal_") == 0 ||
        filename.find("exponential_") == 0) {
        if (filename.find("uniform_") == 0) info.dataset_name = "uniform";
        else if (filename.find("normal_") == 0) info.dataset_name = "normal";
        else if (filename.find("exponential_") == 0) info.dataset_name = "exponential";

        std::regex R_regex("_R(\\d+)_");
        std::smatch R_match;
        if (std::regex_search(filename, R_match, R_regex)) {
            info.R = R_match[1].str();
        } else {
            info.R = "unknown";
        }
    } else {
        if (filename.find("books_") == 0) {
            info.dataset_name = "books";
        } else if (filename.find("fb_") == 0) {
            info.dataset_name = "fb";
        } else if (filename.find("osm_cellids_") == 0) {
            info.dataset_name = "osm";
        } else if (filename.find("osm_") == 0) {
            info.dataset_name = "osm";
        } else {
            info.dataset_name = "unknown";
        }

        info.R = "0";
    }

    return info;
}

} // namespace common

