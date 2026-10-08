#include "common/binary_io.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <stdexcept>

namespace common {

template<typename T>
std::vector<T> read_from_binary(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }
    
    std::uint64_t size;
    file.read(reinterpret_cast<char*>(&size), sizeof(std::uint64_t));
    
    if (!file) {
        throw std::runtime_error("Cannot read size from file: " + filename);
    }
    
    std::vector<T> data(size);
    file.read(reinterpret_cast<char*>(data.data()), size * sizeof(T));
    
    if (!file) {
        throw std::runtime_error("Cannot read data from file: " + filename);
    }
    
    if (data.size() != size) {
        throw std::runtime_error("Data size mismatch in file: " + filename);
    }
    
    if (!std::is_sorted(data.begin(), data.end())) {
        throw std::runtime_error("Data is not sorted in file: " + filename);
    }
    
    return data;
}

template<typename T>
void write_to_binary(const std::vector<T>& data, const std::string& filename) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot create file: " + filename);
    }
    
    std::uint64_t size = data.size();
    file.write(reinterpret_cast<const char*>(&size), sizeof(std::uint64_t));
    
    file.write(reinterpret_cast<const char*>(data.data()), data.size() * sizeof(T));
    
    if (!file) {
        throw std::runtime_error("Cannot write to file: " + filename);
    }
}

template<typename T>
void write_lookups_to_binary(const std::vector<T>& data, const std::string& filename) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot create file: " + filename);
    }
    
    std::uint64_t size = data.size();
    file.write(reinterpret_cast<const char*>(&size), sizeof(std::uint64_t));
    
    for (const auto& value : data) {
        file.write(reinterpret_cast<const char*>(&value), sizeof(T));
        T zero = 0;
        file.write(reinterpret_cast<const char*>(&zero), sizeof(T));
    }
    
    if (!file) {
        throw std::runtime_error("Cannot write lookups to file: " + filename);
    }
}

// Explicit template instantiation
template std::vector<std::uint32_t> read_from_binary<std::uint32_t>(const std::string&);
template std::vector<std::uint64_t> read_from_binary<std::uint64_t>(const std::string&);
template void write_to_binary<std::uint32_t>(const std::vector<std::uint32_t>&, const std::string&);
template void write_to_binary<std::uint64_t>(const std::vector<std::uint64_t>&, const std::string&);
template void write_lookups_to_binary<std::uint32_t>(const std::vector<std::uint32_t>&, const std::string&);
template void write_lookups_to_binary<std::uint64_t>(const std::vector<std::uint64_t>&, const std::string&);

std::vector<std::uint32_t> read_uint32_from_binary(const std::string& filename) {
    return read_from_binary<std::uint32_t>(filename);
}

std::vector<std::uint64_t> read_uint64_from_binary(const std::string& filename) {
    return read_from_binary<std::uint64_t>(filename);
}

bool file_exists(const std::string& filename) {
    std::ifstream file(filename);
    return file.good();
}

} // namespace common
