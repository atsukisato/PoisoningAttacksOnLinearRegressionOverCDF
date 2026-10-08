#pragma once
#include "common/types.h"
#include "poisoning/get_quads.h"
#include <vector>
#include <tuple>
#include <string>

namespace poisoning {

/**
 * minimax_quadratics_golden_section_search_over_w - Solve minimax problem for quadratic functions (golden section search based)
 * 
 * min_w max_i (a_i * w^2 + b_i * w + c_i) 
 * W-axis (w) is searched for the optimal solution
 * 
 * @param quads List of quadratic function coefficients [(a1,b1,c1), ...]  (all a_i > 0)
 * @param eps Tolerance (default: 1e-3)
 * @param max_iter Maximum number of iterations (default: 200)
 * @return (w_star, t_star) = (argmin_w, min_w max_i f_i(w))
 */
std::pair<double, double> minimax_quadratics_golden_section_search_over_w(
    const std::vector<Quad>& quads,
    double eps = 1e-3L,
    int max_iter = 200
);

/**
 * calc_upper_bound_golden_section - Calculate upper bound (golden section search based)
 * 
 * Use minimax algorithm based on golden section search to calculate upper bound
 * 
 * @param xs Sorted data sequence (x1, x2, ..., xn)
 * @param p Number of poison values (lambda)
 * @param eps Tolerance for minimax solution (default: 1e-3)
 * @param max_iter Maximum number of iterations for minimax solution (default: 200)
 * @return (w_star, mse_star, time_taken) 
 */
std::tuple<double, double, double> calc_upper_bound_golden_section(
    const std::vector<double>& xs,
    int p,
    double eps = 1e-3L,
    int max_iter = 200
);



// Calculate upper bound for generic type T (convert to double by subtracting offset)
template <typename T>
inline std::tuple<double, double, double> calc_upper_bound_golden_section(
    const std::vector<T>& xs,
    int p,
    double eps = 1e-3L,
    int max_iter = 200
) {
    if (xs.empty()) {
        return {0.0, 0.0, 0.0};
    }
    const T offset = xs.front();
    std::vector<double> xs_decimal;
    xs_decimal.reserve(xs.size());
    for (const auto& v : xs) {
        xs_decimal.push_back(static_cast<double>(v - offset));
    }
    return calc_upper_bound_golden_section(xs_decimal, p, eps, max_iter);
}

} // namespace poisoning 