#include "common/types.h"
#include <algorithm>
#include <stdexcept>

namespace common {

DataType infer_data_type(const std::string& filename) {
    if (filename.find("uint32") != std::string::npos) {
        return DataType::UINT32;
    }
    else if (filename.find("uint64") != std::string::npos) {
        return DataType::UINT64;
    }
    else {
        throw std::runtime_error("Unknown data type for file: " + filename);
    }
}

} // namespace common