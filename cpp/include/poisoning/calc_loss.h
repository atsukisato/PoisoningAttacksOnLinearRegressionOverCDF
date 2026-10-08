#pragma once
#include "common/types.h"

namespace poisoning {

// Calculate residual variance of linear regression for xs
double calc_loss(const std::vector<double>& xs);

// Specialized version for uint64_t with improved numerical stability
double calc_loss(const std::vector<std::uint64_t>& xs);

// Maximum point-wise error for calc_loss (the linear regression model is trianed by minimizing MSE)
double calc_max_point_error_MSE_train(const std::vector<std::uint64_t>& xs);

// Maximum point-wise error for calc_loss (the linear regression model is trained by minimizing minimax error)
double calc_max_point_error_minimax_train(const std::vector<std::uint64_t>& xs);

double calc_mean_point_error_MSE_train(const std::vector<std::uint64_t>& xs);

double calc_mean_log_point_error_MSE_train(const std::vector<std::uint64_t>& xs);

// Linear regression model is trained on xs, and calculate MSE loss on legitimate_xs
double calc_loss_on_legitimate(const std::vector<std::uint64_t>& xs, const std::vector<std::uint64_t>& legitimate_xs);

// Maximum point-wise error for calc_loss on legitimate data (the linear regression model is trained by minimizing MSE)
double calc_max_point_error_MSE_train_on_legitimate(const std::vector<std::uint64_t>& xs, const std::vector<std::uint64_t>& legitimate_xs);

// Maximum point-wise error for calc_loss on legitimate data (the linear regression model is trained by minimizing minimax error)
double calc_max_point_error_minimax_train_on_legitimate(const std::vector<std::uint64_t>& xs, const std::vector<std::uint64_t>& legitimate_xs);

double calc_mean_point_error_MSE_train_on_legitimate(const std::vector<std::uint64_t>& xs, const std::vector<std::uint64_t>& legitimate_xs);

double calc_mean_log_point_error_MSE_train_on_legitimate(const std::vector<std::uint64_t>& xs, const std::vector<std::uint64_t>& legitimate_xs);

// Template version (automatically convert uint32/uint64)
template<typename T>
double calc_loss(const std::vector<T>& xs);

// Helper structure for internal calculations (for debugging/testing)
struct LinearRegressionStats {
    long double mean_x, mean_y;
    long double var_x, var_y;
    long double cov_xy;
    long double loss;  // Var_y - Cov_xy^2 / Var_x
};

// Calculate statistics (for debugging/testing)
LinearRegressionStats compute_stats(const std::vector<double>& xs);

// Specialized version for uint64_t
LinearRegressionStats compute_stats(const std::vector<std::uint64_t>& xs);

// Helper function for difference calculation
double loss_after_insert(double x_new, size_t pos,
                           size_t n, double sum_x, double sum_x2, double s_xy,
                           double total_sum_x, const std::vector<double>& prefix_sum);

} // namespace poisoning 