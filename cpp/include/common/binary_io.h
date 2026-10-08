#pragma once
#include "common/types.h"
#include <fstream>
#include <stdexcept>

namespace common {

template<typename T>
std::vector<T> read_from_binary(const std::string& filename);

template<typename T>
void write_to_binary(const std::vector<T>& data, const std::string& filename);

template<typename T>
void write_lookups_to_binary(const std::vector<T>& data, const std::string& filename);

// Dynamically determine data type and read
std::vector<std::uint32_t> read_uint32_from_binary(const std::string& filename);
std::vector<std::uint64_t> read_uint64_from_binary(const std::string& filename);

// Check if file exists
bool file_exists(const std::string& filename);

} // namespace common 

