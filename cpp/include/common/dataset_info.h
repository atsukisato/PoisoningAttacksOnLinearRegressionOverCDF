#pragma once

#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace common {

struct DatasetInfo {
    std::string dataset_name;
    std::string n;
    std::string R;
    std::string seed;
    std::string data_type;
    std::string lambda;
};

inline std::string shorten_dataset_name(const std::string& folder_or_prefix) {
    if (folder_or_prefix.find("books") == 0) return "books";
    if (folder_or_prefix.find("fb") == 0) return "fb";
    if (folder_or_prefix.find("osm_cellids") == 0 || folder_or_prefix.find("osm") == 0) return "osm";
    if (folder_or_prefix == "uniform" || folder_or_prefix.find("uniform_") == 0) return "uniform";
    if (folder_or_prefix == "normal" || folder_or_prefix.find("normal_") == 0) return "normal";
    if (folder_or_prefix == "exponential" || folder_or_prefix.find("exponential_") == 0) return "exponential";
    return "unknown";
}

inline DatasetInfo parse_filename(const std::string& filename) {
    DatasetInfo info;
    info.n = "unknown";
    info.R = "0";
    info.seed = "0";
    info.lambda = "0";
    info.data_type = "unknown";
    info.dataset_name = "unknown";

    if (filename.find("_uint32") != std::string::npos) {
        info.data_type = "uint32";
    } else if (filename.find("_uint64") != std::string::npos) {
        info.data_type = "uint64";
    }

    // Nested layout: .../{dataset}/n{n}/[R{R}/]seed{seed}/[lambda{λ}/]{method}_{dtype}
    std::vector<std::string> parts;
    {
        std::string normalized = filename;
        for (char& c : normalized) {
            if (c == '\\') c = '/';
        }
        std::stringstream ss(normalized);
        std::string part;
        while (std::getline(ss, part, '/')) {
            if (!part.empty()) parts.push_back(part);
        }
    }

    auto is_digits = [](const std::string& s, size_t start) {
        return start < s.size() &&
               std::all_of(s.begin() + static_cast<std::ptrdiff_t>(start), s.end(),
                           [](unsigned char c) { return std::isdigit(c) != 0; });
    };

    bool found_n = false;
    for (size_t i = 0; i < parts.size(); ++i) {
        const std::string& p = parts[i];
        if (p.size() > 1 && p[0] == 'n' && is_digits(p, 1)) {
            info.n = p.substr(1);
            found_n = true;
            if (i > 0) {
                info.dataset_name = shorten_dataset_name(parts[i - 1]);
            }
        } else if (p.size() > 1 && p[0] == 'R' && is_digits(p, 1)) {
            info.R = p.substr(1);
        } else if (p.size() > 4 && p.compare(0, 4, "seed") == 0 && is_digits(p, 4)) {
            info.seed = p.substr(4);
        } else if (p.size() > 6 && p.compare(0, 6, "lambda") == 0 && is_digits(p, 6)) {
            info.lambda = p.substr(6);
        }
    }

    if (found_n) {
        if (info.dataset_name == "uniform" || info.dataset_name == "normal" ||
            info.dataset_name == "exponential") {
            if (info.R == "0") info.R = "unknown";
        } else {
            info.R = "0";
        }
        return info;
    }

    // Legacy flat filename fallback
    std::regex seed_regex("seed(\\d+)");
    std::smatch seed_match;
    if (std::regex_search(filename, seed_match, seed_regex)) {
        info.seed = seed_match[1].str();
    }

    std::regex lambda_regex("lambda(\\d+)");
    std::smatch lambda_match;
    if (std::regex_search(filename, lambda_match, lambda_regex)) {
        info.lambda = lambda_match[1].str();
    }

    std::regex n_regex("_n(\\d+)_");
    std::smatch n_match;
    if (std::regex_search(filename, n_match, n_regex)) {
        info.n = n_match[1].str();
    }

    std::string basename = filename;
    size_t slash = basename.find_last_of("/\\");
    if (slash != std::string::npos) {
        basename = basename.substr(slash + 1);
    }

    if (basename.find("uniform_") == 0 || basename.find("normal_") == 0 ||
        basename.find("exponential_") == 0) {
        info.dataset_name = shorten_dataset_name(basename);
        std::regex R_regex("_R(\\d+)_");
        std::smatch R_match;
        if (std::regex_search(basename, R_match, R_regex)) {
            info.R = R_match[1].str();
        } else {
            info.R = "unknown";
        }
    } else {
        info.dataset_name = shorten_dataset_name(basename);
        info.R = "0";
    }

    return info;
}

} // namespace common
