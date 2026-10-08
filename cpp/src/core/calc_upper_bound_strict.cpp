#include "poisoning/calc_upper_bound_strict.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <iterator>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace poisoning {

constexpr double kEps = 1e-12;

// eval_quad function is removed (inline defined in header)

std::vector<double> intersect_quads(const Quad& p1, const Quad& p2, double eps) {
    const auto [a1, b1, c1] = p1;
    const auto [a2, b2, c2] = p2;

    const double A = a1 - a2;
    const double B = b1 - b2;
    const double C = c1 - c2;

    // Degenerates to linear case or parallel (no solution)
    if (std::abs(A) < eps) {
        if (std::abs(B) < eps) {
            return {};
        }
        return {-C / B};
    }

    const double D = B * B - 4.0 * A * C;
    if (D < -eps) {
        return {}; // Complex roots
    }

    const double sqrtD = std::sqrt(D);  // std::max removed (already checked D >= -eps)
    if (sqrtD < eps) {
        return {-B / (2.0 * A)};
    }

    double r1 = (-B - sqrtD) / (2.0 * A);
    double r2 = (-B + sqrtD) / (2.0 * A);
    if (r1 > r2) std::swap(r1, r2);
    return {r1, r2};
}

std::vector<Segment> merge_envelopes(const std::vector<Segment>& env1,
                                     const std::vector<Segment>& env2,
                                     const std::vector<Quad>& quads) {
    std::vector<Segment> result;
    result.reserve(env1.size() + env2.size());

    std::size_t i = 0, j = 0;
    while (i < env1.size() && j < env2.size()) {
        const double L = std::max(env1[i].left,  env2[j].left);
        const double R = std::min(env1[i].right, env2[j].right);
        if (R <= L + kEps) { // no overlap - advance the shorter interval
            (env1[i].right < env2[j].right - kEps) ? ++i : ++j;
            continue;
        }

        const Quad& p = quads[env1[i].quad_index];
        const Quad& q = quads[env2[j].quad_index];

        // Collect intersection roots within (L, R)
        auto raw_roots = intersect_quads(p, q);
        std::array<double, 4> xs_buf{L, R, 0, 0};
        std::size_t xs_cnt = 2;
        for (double rt : raw_roots) {
            if (rt > L && rt < R) xs_buf[xs_cnt++] = rt;
        }
        std::sort(xs_buf.begin(), xs_buf.begin() + xs_cnt);

        // Decide dominant quadratic on each subinterval
        for (std::size_t k = 0; k + 1 < xs_cnt; ++k) {
            const double a = xs_buf[k];
            const double b = xs_buf[k + 1];
            if (b - a < kEps) continue;
            const double mid = (std::isfinite(a) && std::isfinite(b)) ? (a + b) * 0.5
                                 : (std::isinf(a) ? b - 1.0 : a + 1.0);
            const int dom_idx = (eval_quad(p, mid) >= eval_quad(q, mid)) ? env1[i].quad_index
                                                                    : env2[j].quad_index;
            // Concatenate segments if they refer to the same quadratic and are adjacent
            if (!result.empty() && std::abs(result.back().right - a) < kEps && result.back().quad_index == dom_idx) {
                result.back().right = b;
            } else {
                result.emplace_back(a, b, dom_idx);
            }
        }

        // Advance whichever envelope finishes first on the right
        (env1[i].right < env2[j].right - kEps) ? ++i
                                               : (env2[j].right < env1[i].right - kEps ? ++j : (++i, ++j));
    }
    return result;
}

// Recursive helper on iterator ranges to avoid O(n log n) copies.
template <typename It>
static std::vector<Segment> build_envelope_range(It first, It last, const std::vector<Quad>& quads) {
    const auto len = static_cast<std::size_t>(std::distance(first, last));
    if (len == 1) {
        return { Segment(-std::numeric_limits<double>::infinity(),
                         std::numeric_limits<double>::infinity(),
                         *first) };
    }
    It mid = first + len / 2;
    auto left_env  = build_envelope_range(first, mid, quads);
    auto right_env = build_envelope_range(mid,   last, quads);
    return merge_envelopes(left_env, right_env, quads);
}

std::vector<Segment> build_envelope(const std::vector<int>& indices, const std::vector<Quad>& quads) {
    return build_envelope_range(indices.begin(), indices.end(), quads);
}

// Evaluate maximum on envelope at x (hot path - keep tight)
double eval_max_from_envelope(const std::vector<Segment>& envelope,
                                 const std::vector<Quad>& quads,
                                 double x) {
    // Envelope segments are disjoint & ordered; binary-search rather than O(n)
    std::size_t lo = 0, hi = envelope.size();
    while (lo < hi) {
        const std::size_t mid = (lo + hi) >> 1;
        if (x < envelope[mid].left)        hi = mid;
        else if (x > envelope[mid].right)  lo = mid + 1;
        else {
            return eval_quad(quads[envelope[mid].quad_index], x);
        }
    }
    return -std::numeric_limits<double>::infinity();
}

std::pair<double, double> minimax_quadratics_exact(const std::vector<Quad>& quads) {
    if (quads.empty()) throw std::invalid_argument("List of quadratic functions is empty");
    for (const auto& q : quads) if (std::get<0>(q) <= 0) throw std::invalid_argument("All quadratic functions must be concave (a > 0)");

    // Build envelope (O(n log n))
    std::vector<int> idx(quads.size());
    std::iota(idx.begin(), idx.end(), 0);
    const auto envelope = build_envelope(idx, quads);

    double best_val = std::numeric_limits<double>::infinity();
    double best_x   = 0.0;

    for (const auto& seg : envelope) {
        const Quad& quad = quads[seg.quad_index];
        const auto [a, b, c] = quad;

        // Candidate points: vertex & segment endpoints (finite)
        std::array<double, 3> cand{};
        std::size_t cnt = 0;
        const double vx = -b / (2.0 * a);
        if (vx >= seg.left - kEps && vx <= seg.right + kEps) cand[cnt++] = vx;
        if (std::isfinite(seg.left))  cand[cnt++] = seg.left;
        if (std::isfinite(seg.right)) cand[cnt++] = seg.right;

        for (std::size_t i = 0; i < cnt; ++i) {
            const double val = a * cand[i] * cand[i] + b * cand[i] + c;
            if (val < best_val - 1e-9) {
                best_val = val;
                best_x   = cand[i];
            }
        }
    }
    return {best_x, best_val};
}



std::tuple<double, double, double> calc_upper_bound_strict(
    const std::vector<double>& xs,
    int p
) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    if (xs.empty()) {
        throw std::invalid_argument("Data is empty");
    }
    
    auto [x_min_it, x_max_it] = std::minmax_element(xs.begin(), xs.end());
    double x_min = *x_min_it;
    double x_max = *x_max_it;
    
    // Check data range
    double data_range = x_max - x_min;
    if (data_range < 1e-15) {
        // All values are almost the same
        throw std::invalid_argument("Data range is too small (all values are almost the same)");
    }
    
    // Normalize to [0, 1]
    std::vector<double> xs_normalized;
    xs_normalized.reserve(xs.size());
    for (double x : xs) {
        xs_normalized.push_back((x - x_min) / data_range);
    }
    
    // Generate quadratic functions (using normalized data)
    auto quads = get_quads(xs_normalized, p);
    
    // Solve minimax (exact solution)
    auto [w_star_norm, rss_star_norm] = minimax_quadratics_exact(quads);
    
    // Convert back to original scale (correct inverse transformation)
    // Normalization: x_norm = (x - x_min) / data_range
    // Inverse transformation: x = x_norm * data_range + x_min
    // Inverse transformation of w (slope): w_original = w_norm / data_range
    double w_star = w_star_norm / data_range;
    
    // Inverse transformation of MSE: 
    // Use MSE in normalized space (relative error independent of scale)
    double mse_star = rss_star_norm;
    
    const int n = xs.size();
    mse_star = mse_star / static_cast<double>(n + p);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    double time_taken = duration.count() / 1e6;  // Convert to seconds
    
    return {w_star, mse_star, time_taken};
}

} // namespace poisoning
