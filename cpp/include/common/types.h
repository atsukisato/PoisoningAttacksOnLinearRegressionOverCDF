#pragma once
#include <cstdint>
#include <vector>
#include <string>

namespace common {

// Enumeration for data type determination
enum class DataType {
    UINT32,
    UINT64
};

// Infer data type from filename
DataType infer_data_type(const std::string& filename);

} // namespace common
