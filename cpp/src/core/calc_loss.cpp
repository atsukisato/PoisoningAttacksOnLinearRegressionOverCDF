#include "poisoning/calc_loss.h"
#include <cmath>
#include <numeric>
#include <stdexcept>
#include <algorithm>
#include <limits>

namespace poisoning {

static inline std::pair<long double, long double> mean_var_y(std::size_t n) {
    if (n == 0) throw std::runtime_error("Empty data");
    long double mean_y = (static_cast<long double>(n) + 1.0L) / 2.0L;
    long double var_y  = (static_cast<long double>(n) * n - 1.0L) / 12.0L;
    return {mean_y, var_y};
}

static inline void normalize_u64_to_ld(
    const std::vector<std::uint64_t>& xs,
    long double& offset_ld, long double& scale_ld,
    std::vector<long double>& xnorm
) {
    if (xs.empty()) throw std::runtime_error("Empty data");
    auto [mn_it, mx_it] = std::minmax_element(xs.begin(), xs.end());
    std::uint64_t offset = *mn_it;
    std::uint64_t range  = *mx_it - offset;

    unsigned shift = 0;
    if (range > 0) {
        unsigned lg = 63u - static_cast<unsigned>(__builtin_clzll(range));
        shift = (lg > 40u) ? (lg - 40u) : 0u;
    }
    std::uint64_t scale = (shift == 0) ? 1ULL : (1ULL << shift);

    offset_ld = static_cast<long double>(offset);
    scale_ld  = static_cast<long double>(scale);

    xnorm.resize(xs.size());
    for (std::size_t i = 0; i < xs.size(); ++i) {
        xnorm[i] = static_cast<long double>(xs[i] - offset) / scale_ld;
    }
}

template <class XContainer>
static inline LinearRegressionStats stats_from_centered_scaled_x(const XContainer& xnorm,
                                                                 long double offset_ld,
                                                                 long double scale_ld)
{
    const std::size_t n = xnorm.size();
    if (n == 0) throw std::runtime_error("Empty data");

    auto [mean_y, var_y] = mean_var_y(n);

    long double mean_xp = 0.0L;
    for (std::size_t i = 0; i < n; ++i) {
        mean_xp += xnorm[i];
    }
    mean_xp /= static_cast<long double>(n);

    long double sum_xy = 0.0L;
    for (std::size_t i = 0; i < n; ++i) {
        long double yi = static_cast<long double>(i + 1);
        sum_xy += xnorm[i] * yi;
    }
    long double cov_xp = sum_xy / static_cast<long double>(n) - mean_xp * mean_y;

    long double sum_x2 = 0.0L;
    for (const auto& x : xnorm) {
        sum_x2 += x * x;
    }
    long double var_xp = sum_x2 / static_cast<long double>(n) - mean_xp * mean_xp;

    long double var_x  = var_xp * (scale_ld * scale_ld);
    long double cov_xy = cov_xp * scale_ld;

    long double mean_x = offset_ld + scale_ld * mean_xp;

    if (var_x == 0.0L)
        throw std::runtime_error("Zero variance in x values");

    long double loss = var_y - (cov_xy * cov_xy) / var_x;

    return {mean_x, mean_y, var_x, var_y, cov_xy, loss};
}


double calc_loss(const std::vector<std::uint64_t>& xs_u64) {
    long double offset_ld, scale_ld;
    std::vector<long double> xnorm;
    normalize_u64_to_ld(xs_u64, offset_ld, scale_ld, xnorm);
    auto s = stats_from_centered_scaled_x(xnorm, offset_ld, scale_ld);
    return static_cast<double>(s.loss);
}

std::vector<long double> get_errors_MSE_train(const std::vector<std::uint64_t>& xs_u64) {
    long double offset_ld, scale_ld;
    std::vector<long double> xnorm;
    normalize_u64_to_ld(xs_u64, offset_ld, scale_ld, xnorm);
    auto s = stats_from_centered_scaled_x(xnorm, 0L, 1.0L);
    long double a = s.cov_xy / s.var_x; // slope on normalized x
    long double b = s.mean_y - a * s.mean_x; // intercept on normalized x
    std::vector<long double> errors;
    for (std::size_t i = 0; i < xnorm.size(); ++i) {
        long double y_true = static_cast<long double>(i + 1);
        long double y_hat  = a * xnorm[i] + b;
        long double err    = fabsl(y_hat - y_true);
        errors.push_back(err);
    }
    return errors;
}

// maximum point-wise error calculation for uint64_t input (the linear regression model is trianed by minimizing MSE)
double calc_max_point_error_MSE_train(const std::vector<std::uint64_t>& xs_u64) {
    auto errors = get_errors_MSE_train(xs_u64);
    long double max_err = 0.0L;
    for (const auto& err : errors) {
        if (err > max_err) {
            max_err = err;
        }
    }
    return static_cast<double>(max_err);
}

// maximum point-wise error calculation for uint64_t input (the linear regression model is trained by minimizing minimax error)
double calc_max_point_error_minimax_train(const std::vector<std::uint64_t>& xs_u64) {
    (void)xs_u64; // unused for stub
    return 0.0;
}

double calc_mean_point_error_MSE_train(const std::vector<std::uint64_t>& xs_u64) {
    auto errors = get_errors_MSE_train(xs_u64);
    long double total_err = 0.0L;
    for (const auto& err : errors) {
        total_err += err;
    }
    long double mean_err = total_err / static_cast<long double>(errors.size());
    return static_cast<double>(mean_err);
}

double calc_mean_log_point_error_MSE_train(const std::vector<std::uint64_t>& xs_u64) {
    auto errors = get_errors_MSE_train(xs_u64);
    long double total_log_err = 0.0L;
    for (const auto& err : errors) {
        total_log_err += logl(err + 1.0L);
    }
    long double mean_err = total_log_err / static_cast<long double>(errors.size());
    return static_cast<double>(mean_err);
}

std::vector<size_t> get_legitimate_ranks(const std::vector<std::uint64_t>& xs, const std::vector<std::uint64_t>& legitimate_xs) {
    // get the rank of each legitimate key (1-index)
    std::vector<size_t> ranks;
    size_t legit_i = 0;
    for (size_t i = 0; i < xs.size(); ++i) {
        if (legit_i < legitimate_xs.size() && xs[i] == legitimate_xs[legit_i]) {
            ranks.push_back(i + 1); // 1-indexed
            ++legit_i;
        }
    }
    if (legit_i != legitimate_xs.size()) {
        throw std::runtime_error("Some legitimate keys not found in xs");
    }
    return ranks;
}

// Linear regression model is trained on xs, and calculate MSE loss on legitimate_xs
double calc_loss_on_legitimate(const std::vector<std::uint64_t>& xs, const std::vector<std::uint64_t>& legitimate_xs) {
    auto legit_ranks = get_legitimate_ranks(xs, legitimate_xs);
    auto errors = get_errors_MSE_train(xs);
    long double total_err = 0.0L;
    for (size_t i = 0; i < legitimate_xs.size(); ++i) {
        total_err += errors[legit_ranks[i] - 1] * errors[legit_ranks[i] - 1]; // squared error (legit_ranks is 1-indexed)
    }
    long double mse_loss = total_err / static_cast<long double>(legitimate_xs.size());
    return static_cast<double>(mse_loss);
}

// Maximum point-wise error for calc_loss on legitimate data (the linear regression model is trained by minimizing MSE)
double calc_max_point_error_MSE_train_on_legitimate(const std::vector<std::uint64_t>& xs, const std::vector<std::uint64_t>& legitimate_xs) {
    auto legit_ranks = get_legitimate_ranks(xs, legitimate_xs);
    auto errors = get_errors_MSE_train(xs);
    long double max_err = 0.0L;
    for (size_t i = 0; i < legitimate_xs.size(); ++i) {
        long double err = errors[legit_ranks[i] - 1]; // 1-indexed
        if (err > max_err) {
            max_err = err;
        }
    }
    return static_cast<double>(max_err);
}

// Maximum point-wise error for calc_loss on legitimate data (the linear regression model is trained by minimizing minimax error)
double calc_max_point_error_minimax_train_on_legitimate(const std::vector<std::uint64_t>& xs, const std::vector<std::uint64_t>& legitimate_xs) {
    (void)xs; (void)legitimate_xs; // unused for stub
    return 0.0;
}

double calc_mean_point_error_MSE_train_on_legitimate(const std::vector<std::uint64_t>& xs, const std::vector<std::uint64_t>& legitimate_xs) {
    auto legit_ranks = get_legitimate_ranks(xs, legitimate_xs);
    auto errors = get_errors_MSE_train(xs);
    long double total_err = 0.0L;
    for (size_t i = 0; i < legitimate_xs.size(); ++i) {
        total_err += errors[legit_ranks[i] - 1]; // 1-indexed
    }
    long double mean_err = total_err / static_cast<long double>(legitimate_xs.size());
    return static_cast<double>(mean_err);
}

double calc_mean_log_point_error_MSE_train_on_legitimate(const std::vector<std::uint64_t>& xs, const std::vector<std::uint64_t>& legitimate_xs) {
    auto legit_ranks = get_legitimate_ranks(xs, legitimate_xs);
    auto errors = get_errors_MSE_train(xs);
    long double total_log_err = 0.0L;
    for (size_t i = 0; i < legitimate_xs.size(); ++i) {
        total_log_err += logl(errors[legit_ranks[i] - 1] + 1.0L); // 1-indexed
    }
    long double mean_err = total_log_err / static_cast<long double>(legitimate_xs.size());
    return static_cast<double>(mean_err);
}

double calc_loss(const std::vector<double>& xs) {
    if (xs.empty()) throw std::runtime_error("Empty data");
    std::vector<long double> xld(xs.begin(), xs.end());
    auto s = stats_from_centered_scaled_x(xld, /*offset*/0.0L, /*scale*/1.0L);
    return static_cast<double>(s.loss);
}

LinearRegressionStats compute_stats(const std::vector<std::uint64_t>& xs_u64) {
    long double offset_ld, scale_ld;
    std::vector<long double> xnorm;
    normalize_u64_to_ld(xs_u64, offset_ld, scale_ld, xnorm);
    return stats_from_centered_scaled_x(xnorm, offset_ld, scale_ld);
}

LinearRegressionStats compute_stats(const std::vector<double>& xs) {
    if (xs.empty()) throw std::runtime_error("Empty data");
    std::vector<long double> xld(xs.begin(), xs.end());
    return stats_from_centered_scaled_x(xld, 0.0L, 1.0L);
}

template<>
double calc_loss(const std::vector<std::uint32_t>& xs) {
    std::vector<std::uint64_t> xs_u64(xs.begin(), xs.end());
    return calc_loss(xs_u64);
}

template<>
double calc_loss(const std::vector<std::uint64_t>& xs) {
    return calc_loss(xs);
}

template<>
double calc_loss(const std::vector<double>& xs) {
    return calc_loss(xs);
}

double loss_after_insert(double x_new, size_t pos,
                           size_t n, double sum_x, double sum_x2, double s_xy,
                           double total_sum_x, const std::vector<double>& prefix_sum) {
    size_t n_new = n + 1;
    double sum_x_new = sum_x + x_new;
    double sum_x2_new = sum_x2 + x_new * x_new;
    double mean_x_new = sum_x_new / n_new;
    double var_x_new = sum_x2_new / n_new - mean_x_new * mean_x_new;
    
    double mean_y_new = static_cast<double>(n_new + 1) / static_cast<double>(2);
    double var_y_new = static_cast<double>(n_new * n_new - 1) / static_cast<double>(12);
    
    double sum_after = total_sum_x - (pos > 0 ? prefix_sum[pos - 1] : double(0));
    double s_xy_new = s_xy + x_new * (pos + static_cast<double>(1)) + sum_after;
    double cov_xy_new = s_xy_new / n_new - mean_x_new * mean_y_new;
    
    return var_y_new - cov_xy_new * cov_xy_new / var_x_new;
}

} // namespace poisoning