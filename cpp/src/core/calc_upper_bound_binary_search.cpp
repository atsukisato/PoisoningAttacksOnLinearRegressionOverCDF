#include "poisoning/calc_upper_bound_binary_search.h"
#include <cmath>
#include <chrono>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <cassert>

namespace poisoning {

namespace {

struct FeasibleChecker {
    std::vector<double> centers;  // -b / (2a)
    std::vector<double> inv2as;   // 1 / (2a)
    std::vector<double> four_as;  // 4a
    std::vector<double> kappas;   // b^2 - 4ac

    double t_low_max = 0.0;

    explicit FeasibleChecker(const std::vector<Quad>& quads) {
        const size_t m = quads.size();
        centers.reserve(m);
        inv2as.reserve(m);
        four_as.reserve(m);
        kappas.reserve(m);

        double tlow = 0.0;
        for (const auto& q : quads) {
            const auto& [a, b, c] = q;
            assert(a > 0.0 && "a must be positive");

            const double inv2a  = 0.5 / a;
            const double center = -b * inv2a;
            const double four_a = 4.0 * a;
            const double kappa  = b * b - 4.0 * a * c;

            centers.push_back(center);
            inv2as.push_back(inv2a);
            four_as.push_back(four_a);
            kappas.push_back(kappa);

            const double min_val = c - (b * b) / (4.0 * a);
            if (min_val > tlow) tlow = min_val;
        }
        t_low_max = tlow;
    }

    // Feasibility check: return intersection interval [L,R].
    inline std::tuple<bool,double,double> operator()(double t) const {
        double L = -std::numeric_limits<double>::infinity();
        double R =  std::numeric_limits<double>::infinity();

        const size_t m = centers.size();
        for (size_t i = 0; i < m; ++i) {
            const double s = kappas[i] + four_as[i] * t;  // b^2 - 4ac + 4at
            if (s < 0.0) return {false, L, R};

            const double rad = std::sqrt(s) * inv2as[i];  // sqrt(s)/(2a)
            const double left  = centers[i] - rad;
            const double right = centers[i] + rad;

            if (left  > L) L = left;
            if (right < R) R = right;

            if (L > R) return {false, L, R};
        }
        return {true, L, R};
    }
};

} // anonymous namespace

std::pair<double, double> minimax_quadratics_binary_search_over_y(
    const std::vector<Quad>& quads,
    double eps,
    int max_iter
) {
    FeasibleChecker chk(quads);
    double t_low = chk.t_low_max;

    double t_high = t_low;
    double L_feasible = 0.0, R_feasible = 0.0;
    while (true) {
        auto [ok, L, R] = chk(t_high);
        if (ok) {
            L_feasible = L;
            R_feasible = R;
            break;
        }
        t_high = 2.0 * t_high + 1e-12;
    }

    for (int iter = 0; iter < max_iter; ++iter) {
        if (t_high - t_low <= eps) {
            break;
        }

        const double mid = 0.5 * (t_low + t_high);
        auto [ok, L, R] = chk(mid);

        if (ok) {
            t_high = mid;       // Feasible -> tighten upper bound
            L_feasible = L;
            R_feasible = R;
        } else {
            t_low = mid;        // Infeasible -> increase lower bound
        }
    }
    
    // Middle of the final feasible interval is w*
    double w_star = (L_feasible + R_feasible) / 2.0;
    return {w_star, t_high};
}


std::tuple<double, double, double> calc_upper_bound_binary_search(
    const std::vector<double>& xs,
    int p,
    double eps,
    int max_iter
) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Generate quadratic functions
    auto quads = get_quads(xs, p);
    
    // Solve minimax (binary search based)
    auto [w_star, rss_star] = minimax_quadratics_binary_search_over_y(quads, eps, max_iter);
    
    const int n = xs.size();
    double mse_star = rss_star / static_cast<double>(n + p);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    double time_taken = duration.count() / 1e6;  // Convert to seconds
    
    return {w_star, mse_star, time_taken};
}

} // namespace poisoning 