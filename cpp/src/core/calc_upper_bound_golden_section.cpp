#include "poisoning/calc_upper_bound_golden_section.h"
#include <cmath>
#include <chrono>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <cassert>

namespace poisoning {

std::pair<double, double> minimax_quadratics_golden_section_search_over_w(
    const std::vector<Quad>& quads,
    double eps,
    int max_iter
) {
    double L = std::numeric_limits<double>::infinity();
    double R = -std::numeric_limits<double>::infinity();
    for (const auto& [a, b, c] : quads) {
        double min_w = -b / (2.0 * a);
        L = std::min(L, min_w);
        R = std::max(R, min_w);
    }

    // Expand the search interval slightly
    double range = R - L;
    double w_a = L - 0.01 * range;
    double w_b = R + 0.01 * range;
    
    // Golden ratio
    const double phi = (1.0 + std::sqrt(5.0)) / 2.0;
    
    // Helper lambda to calculate the maximum value at each point
    auto eval_max = [&quads](double w) -> double {
        double max_val = -std::numeric_limits<double>::infinity();
        for (const auto& [a_coef, b_coef, c_coef] : quads) {
            double val = a_coef * w * w + b_coef * w + c_coef;
            max_val = std::max(max_val, val);
        }
        return max_val;
    };
    
    // Initial interior points
    double w_l = w_b - (w_b - w_a) / phi;
    double w_r = w_a + (w_b - w_a) / phi;
    double y_l = eval_max(w_l);
    double y_r = eval_max(w_r);
    double y_a = eval_max(w_a);
    double y_b = eval_max(w_b);
    
    // Main loop of golden section method
    for (int t = 0; t < max_iter; ++t) {
        if (std::max(y_a, y_b) - std::min(y_l, y_r) <= eps) {
            break;
        }
        
        if (y_l > y_r) {
            w_a = w_l;
            y_a = y_l;
            w_l = w_r;
            y_l = y_r;
            w_r = w_a + (w_b - w_a) / phi;
            y_r = eval_max(w_r);
        } else {
            w_b = w_r;
            y_b = y_r;
            w_r = w_l;
            y_r = y_l;
            w_l = w_b - (w_b - w_a) / phi;
            y_l = eval_max(w_l);
        }
    }
    
    // Final solution
    double w_star = (w_a + w_b) / 2.0;
    double t_high = eval_max(w_star);
    
    return {w_star, t_high};
}



std::tuple<double, double, double> calc_upper_bound_golden_section(
    const std::vector<double>& xs,
    int p,
    double eps,
    int max_iter
) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Generate quadratic functions
    auto quads = get_quads(xs, p);
    
    // Solve minimax (golden section search based)
    auto [w_star, rss_star] = minimax_quadratics_golden_section_search_over_w(quads, eps, max_iter);
    
    const int n = xs.size();
    double mse_star = rss_star / static_cast<double>(n + p);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    double time_taken = duration.count() / 1e6;  // Convert to seconds
    
    return {w_star, mse_star, time_taken};
}

} // namespace poisoning 