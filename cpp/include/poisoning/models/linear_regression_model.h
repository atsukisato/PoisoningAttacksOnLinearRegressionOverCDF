#pragma once
#include <vector>
#include <cstdint>
#include <cstddef>

namespace poisoning::models {

// Simple linear regression model that maps keys -> predicted positions.
// Constructed from sorted data (will sort internally if needed).
class LinearRegressionModel {
public:
    explicit LinearRegressionModel(const std::vector<std::uint64_t>& data);

    // Predict index for key (lower_bound). Returns an index in [0, data_size].
    std::size_t lower_bound(std::uint64_t key) const;
    std::size_t std_lower_bound(std::uint64_t key) const;

private:
    std::vector<std::uint64_t> data_;
    double a_ = 0.0; // slope
    double b_ = 0.0; // intercept

    void train_linear_regression();
};

} // namespace poisoning::models
