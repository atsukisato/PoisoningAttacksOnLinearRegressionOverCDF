#include "poisoning/calc_loss.h"
#include "poisoning/calc_upper_bound_strict.h"
#include "poisoning/data_generators.h"
#include "poisoning/inject_poison.h"
#include "poisoning/inject_poison_consecutive_w_endpoints.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr double kAbsTol = 1e-12;

int g_failures = 0;

void fail(const std::string& msg) {
    std::cerr << "[FAIL] " << msg << "\n";
    ++g_failures;
}

void expect_true(bool cond, const std::string& msg) {
    if (!cond) {
        fail(msg);
    }
}

void expect_eq_u64(std::uint64_t got, std::uint64_t want, const std::string& msg) {
    if (got != want) {
        std::ostringstream oss;
        oss << msg << " (got=" << got << ", want=" << want << ")";
        fail(oss.str());
    }
}

void expect_near(double got, double want, const std::string& msg, double tol = kAbsTol) {
    if (!(std::isfinite(got) && std::abs(got - want) <= tol)) {
        std::ostringstream oss;
        oss << msg << " (got=" << got << ", want=" << want << ", tol=" << tol << ")";
        fail(oss.str());
    }
}

std::vector<std::uint64_t> merge_sorted(std::vector<std::uint64_t> data,
                                        const std::vector<std::uint64_t>& poisons) {
    data.insert(data.end(), poisons.begin(), poisons.end());
    std::sort(data.begin(), data.end());
    return data;
}

std::vector<std::uint64_t> arithmetic_keys(size_t n, std::uint64_t step) {
    std::vector<std::uint64_t> xs(n);
    for (size_t i = 0; i < n; ++i) {
        xs[i] = step * static_cast<std::uint64_t>(i);
    }
    return xs;
}

void test_a_loss_sanity() {
    const auto xs = arithmetic_keys(20, 100);
    expect_near(poisoning::calc_loss(xs), 0.0, "A: arithmetic sequence loss");
}

void test_b_greedy_inject() {
    const auto data = arithmetic_keys(10, 100);
    expect_near(poisoning::calc_loss(data), 0.0, "B: clean loss");

    auto poisons = poisoning::get_poison_values_delta_calc(data, 2);
    expect_eq_u64(poisons.size(), 2, "B: poison count");
    std::sort(poisons.begin(), poisons.end());
    expect_eq_u64(poisons[0], 701, "B: poison[0]");
    expect_eq_u64(poisons[1], 702, "B: poison[1]");

    const auto merged = merge_sorted(data, poisons);
    expect_near(poisoning::calc_loss(merged), 0.35933116949787092, "B: poisoned loss");
}

void test_c_datagenerator_inject() {
    poisoning::DataGenerator gen(42);
    auto data = gen.generate_uniform<std::uint64_t>(50, 1000);
    expect_true(!data.empty(), "C: generated non-empty");
    expect_true(std::is_sorted(data.begin(), data.end()), "C: sorted");

    auto poisons = poisoning::get_poison_values_delta_calc(data, 3);
    expect_eq_u64(poisons.size(), 3, "C: poison count");

    const auto merged = merge_sorted(data, poisons);
    expect_near(poisoning::calc_loss(merged), 5.6415911617615677, "C: poisoned loss");
}

void test_d_upper_bound_strict() {
    const auto data = arithmetic_keys(10, 100);
    const auto [w, mse, t] = poisoning::calc_upper_bound_strict(data, 2);
    expect_true(std::isfinite(w) && w >= 0.0, "D: w finite non-negative");
    expect_true(std::isfinite(mse) && mse >= 0.0, "D: mse finite non-negative");
    expect_true(std::isfinite(t) && t >= 0.0, "D: time finite non-negative");
    // Upper-bound numerics can differ slightly across compilers; keep a looser tol.
    expect_near(w, 0.012, "D: w golden", 1e-9);
    expect_near(mse, 0.36666666666666664, "D: mse golden", 1e-9);
}

void test_e_consecutive() {
    const auto data = arithmetic_keys(10, 100);
    auto poisons = poisoning::get_poison_values_consecutive_w_endpoints(data, 2);
    expect_eq_u64(poisons.size(), 2, "E: poison count");
    std::sort(poisons.begin(), poisons.end());
    expect_eq_u64(poisons[0], 701, "E: poison[0]");
    expect_eq_u64(poisons[1], 702, "E: poison[1]");

    const auto merged = merge_sorted(data, poisons);
    expect_near(poisoning::calc_loss(merged), 0.35933116949787092, "E: poisoned loss");
}

}  // namespace

int main() {
    try {
        test_a_loss_sanity();
        test_b_greedy_inject();
        test_c_datagenerator_inject();
        test_d_upper_bound_strict();
        test_e_consecutive();
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] exception: " << e.what() << "\n";
        return 1;
    }

    if (g_failures > 0) {
        std::cerr << "smoke_test: " << g_failures << " failure(s)\n";
        return 1;
    }
    std::cout << "smoke_test: all checks passed\n";
    return 0;
}
