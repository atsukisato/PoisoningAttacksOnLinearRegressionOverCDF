#include "poisoning/models/linear_regression_model.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include "poisoning/calc_loss.h"

namespace poisoning::models {

LinearRegressionModel::LinearRegressionModel(const std::vector<std::uint64_t>& data)
    : data_(data)
{
    if (!std::is_sorted(data_.begin(), data_.end())) {
        std::sort(data_.begin(), data_.end());
    }
    train_linear_regression();
}

void LinearRegressionModel::train_linear_regression() {
    const std::size_t n = data_.size();
    if (n == 0) { a_ = 0.0; b_ = 0.0; return; }
    if (n == 1) { a_ = 0.0; b_ = 0.0; return; }
    auto s = poisoning::compute_stats(data_);

    // slope = cov_xy / var_x
    long double slope_ld = s.cov_xy / s.var_x;
    long double intercept_ld = s.mean_y - slope_ld * s.mean_x - 1.0L; // 0-indexed

    a_ = static_cast<double>(slope_ld);
    b_ = static_cast<double>(intercept_ld);
}

std::size_t LinearRegressionModel::lower_bound(std::uint64_t key) const {
    const std::size_t n = data_.size();
    if (n == 0) return 0;

    if (key <= data_.front()) return 0;
    if (key > data_.back())   return n;

    const double pred = a_ * key + b_;

    std::size_t p;
    if (pred <= 0.0) {
        p = 0;
    } else if (pred >= n - 1) {
        p = n - 1;
    } else {
        p = static_cast<std::size_t>(pred);
    }

    std::size_t lo = 0;
    std::size_t hi = 0;

    if (data_[p] < key) {
        std::size_t step = 1;

        lo = p + 1;
        hi = std::min(n, lo + step);

        while (hi < n && data_[hi - 1] < key) {
            lo  = hi;
            step <<= 1;
            hi  = std::min(n, lo + step);
        }
    } else {
        std::size_t step = 1;

        hi = p + 1;
        lo = (p == 0) ? 0 : p;

        while (lo > 0 && data_[lo - 1] >= key) {
            hi = lo;
            step <<= 1;
            if (lo > step) {
                lo -= step;
            } else {
                lo = 0;
            }
        }
    }

    auto it = std::lower_bound(data_.begin() + lo, data_.begin() + hi, key);
    return static_cast<std::size_t>(it - data_.begin());
}

std::size_t LinearRegressionModel::std_lower_bound(std::uint64_t key) const {
    if (data_.empty()) return 0;
    auto it = std::lower_bound(data_.begin(), data_.end(), key);
    return static_cast<std::size_t>(it - data_.begin());
}

} // namespace poisoning::models
