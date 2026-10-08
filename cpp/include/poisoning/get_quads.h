#pragma once
#include "common/types.h"
#include <vector>
#include <tuple>

namespace poisoning {

// Coefficients of a quadratic function (a, b, c) for a*w^2 + b*w + c
using Quad = std::tuple<double, double, double>;

/**
 * get_quads - Generate quadratic functions efficiently
 * 
 * Case-1: Allocate poison at endpoints
 * Case-2: Allocate poison at a single point
 * 
 * @param xs Sorted data sequence
 * @param p Number of poison values
 * @return List of quadratic function coefficients
 */
std::vector<Quad> get_quads(const std::vector<double>& xs, int p);

// Template version for generic type T (convert to double by subtracting offset)
template <typename T>
inline std::vector<Quad> get_quads(const std::vector<T>& xs, int p) {
    if (xs.empty()) return {};
    const T offset = xs.front();
    std::vector<double> xs_decimal;
    xs_decimal.reserve(xs.size());
    for (const auto& v : xs) {
        xs_decimal.push_back(static_cast<double>(v - offset));
    }
    return get_quads(xs_decimal, p);
}

} // namespace poisoning