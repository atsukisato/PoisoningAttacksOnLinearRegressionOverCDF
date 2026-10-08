#pragma once
#include "common/types.h"
#include "poisoning/get_quads.h"
#include <vector>
#include <tuple>
#include <string>
#include <limits>

namespace poisoning {

// Structure representing a segment (left endpoint, right endpoint, dominating parabola index)
struct Segment {
    double left;
    double right;
    int quad_index;
    
    Segment(double l, double r, int idx) : left(l), right(r), quad_index(idx) {}
};

/**
 * intersect_quads - Find the intersection of two parabolas
 * 
 * @param p1 First parabola
 * @param p2 Second parabola
 * @param eps Tolerance for numerical errors (default: 1e-12)
 * @return Vector of intersection points (sorted)
 */
std::vector<double> intersect_quads(const Quad& p1, const Quad& p2, double eps = 1e-12L);

/**
 * merge_envelopes - Merge two envelopes (linear time)
 * 
 * @param env1 First envelope
 * @param env2 Second envelope
 * @param quads List of all parabolas
 * @return Merged envelope
 */
std::vector<Segment> merge_envelopes(
    const std::vector<Segment>& env1, 
    const std::vector<Segment>& env2, 
    const std::vector<Quad>& quads
);

/**
 * build_envelope - Build envelope using divide & conquer
 * 
 * @param indices List of parabola indices
 * @param quads List of all parabolas
 * @return Envelope
 */
std::vector<Segment> build_envelope(const std::vector<int>& indices, const std::vector<Quad>& quads);

/**
 * eval_quad - Calculate the value of a parabola (inline function)
 * 
 * @param quad Coefficients of the parabola
 * @param x Evaluation point
 * @return Value of the parabola
 */
inline double eval_quad(const Quad& quad, double x) {
    return std::get<0>(quad) * x * x + std::get<1>(quad) * x + std::get<2>(quad);
}

/**
 * eval_max_from_envelope - Calculate the maximum value at a given point from the envelope
 * 
 * @param envelope Envelope
 * @param quads List of all parabolas
 * @param x Evaluation point
 * @return Maximum value
 */
double eval_max_from_envelope(const std::vector<Segment>& envelope, const std::vector<Quad>& quads, double x);

/**
 * minimax_quadratics_exact - Solve the minimax problem for quadratic functions (O(n log n))
 * 
 * min_w max_i (a_i * w^2 + b_i * w + c_i) 
 * Use divide & conquer algorithm to build the envelope
 * 
 * @param quads List of quadratic function coefficients [(a1,b1,c1), ...]  (all a_i > 0)
 * @return (w_star, t_star) = (argmin_w, min_w max_i f_i(w))
 */
std::pair<double, double> minimax_quadratics_exact(const std::vector<Quad>& quads);

/**
 * calc_upper_bound_strict - Calculate upper bound (strict solution based)
 * 
 * Use strict solution (O(n log n)) to calculate upper bound
 * 
 * @param xs Sorted data sequence (x1, x2, ..., xn)
 * @param p Number of poison values (lambda)
 * @return (w_star, mse_star, time_taken) 
 */
std::tuple<double, double, double> calc_upper_bound_strict(
    const std::vector<double>& xs,
    int p
);



// Calculate upper bound for generic type T (convert to double by subtracting offset)
template <typename T>
inline std::tuple<double, double, double> calc_upper_bound_strict(
    const std::vector<T>& xs,
    int p
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
    return calc_upper_bound_strict(xs_decimal, p);
}

} // namespace poisoning 