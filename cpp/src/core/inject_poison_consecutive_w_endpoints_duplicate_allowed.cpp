#include "poisoning/inject_poison_consecutive_w_endpoints_duplicate_allowed.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <numeric>
#include <regex>
#include <stdexcept>
#include <cassert>
#include <optional>
#include <vector>
#include <cstdint>
#include <limits>
#include <array>
#include <set>


namespace poisoning {

// ---- numeric helpers -------------------------------------------------

static inline double cuberoot_inline(double x) {
    return x >= 0 ? std::pow(x, 1.0/3.0) : -std::pow(-x, 1.0/3.0);
}

// Solve cubic: A x^3 + B x^2 + C x + D = 0 (real roots only).
// Falls back to quadratic/linear when A ~ 0.
// Returns all real roots in ascending order (unique-ish within tol).
static std::vector<double> cubic_real_roots_inline(double A, double B, double C, double D) {
    const double eps = 1e-12;
    std::vector<double> roots;

    auto push_unique = [&](double r) {
        for (double q : roots) { if (std::fabs(q - r) < 1e-9) return; }
        roots.push_back(r);
    };

    if (std::fabs(A) < eps) {
        // Quadratic: B x^2 + C x + D = 0
        if (std::fabs(B) < eps) {
            // Linear: C x + D = 0
            if (std::fabs(C) < eps) return roots; // no or infinite solutions; treat as none
            push_unique(-D / C);
        } else {
            double disc = C*C - 4*B*D;
            if (disc > eps) {
                double s = std::sqrt(disc);
                push_unique((-C - s) / (2*B));
                push_unique((-C + s) / (2*B));
            } else if (std::fabs(disc) <= eps) {
                push_unique(-C / (2*B));
            } // else no real roots
        }
        std::sort(roots.begin(), roots.end());
        return roots;
    }

    // Normalize to depressed cubic: t^3 + p t + q = 0 via x = t - B/(3A)
    double invA = 1.0 / A;
    double a = B * invA;
    double b = C * invA;
    double c = D * invA;
    double a_over_3 = a / 3.0;

    double p = b - a*a_over_3;                   // p = b - a^2/3
    double q = 2*a*a*a/27.0 - a*b/3.0 + c;       // q = 2a^3/27 - ab/3 + c
    double disc = (q*q)/4.0 + (p*p*p)/27.0;      // discriminant of depressed cubic

    if (disc > eps) {
        // one real root
        double s1 = -q/2.0;
        double s2 = std::sqrt(disc);
        double u = cuberoot_inline(s1 + s2);
        double v = cuberoot_inline(s1 - s2);
        double t = u + v;
        double x = t - a_over_3;
        push_unique(x);
    } else if (std::fabs(disc) <= eps) {
        // multiple roots (at least two equal)
        double t = cuberoot_inline(-q/2.0); // since disc ~ 0, u = v
        double x1 = 2*t - a_over_3;
        double x2 = -t - a_over_3;
        push_unique(x1);
        push_unique(x2);
    } else {
        // three real roots
        double r = std::sqrt(-p/3.0);
        double phi = std::acos(std::clamp((-q/(2.0*r*r*r)), -1.0, 1.0));
        double t1 = 2*r*std::cos(phi/3.0);
        double t2 = 2*r*std::cos((phi + 2.0*M_PI)/3.0);
        double t3 = 2*r*std::cos((phi + 4.0*M_PI)/3.0);
        push_unique(t1 - a_over_3);
        push_unique(t2 - a_over_3);
        push_unique(t3 - a_over_3);
    }

    std::sort(roots.begin(), roots.end());
    return roots;
}

// ---- core: MSE ingredients for Seg+E ---------------------------------

struct Prefix {
    std::vector<double> S; // sum k
    std::vector<double> T; // sum k^2
    std::vector<double> U; // sum k * rank
};

static Prefix build_prefix(const std::vector<double>& k) {
    size_t n = k.size();
    Prefix P;
    P.S.assign(n+1, 0.0);
    P.T.assign(n+1, 0.0);
    P.U.assign(n+1, 0.0);
    for (size_t t = 1; t <= n; ++t) {
        P.S[t] = P.S[t-1] + k[t-1];
        P.T[t] = P.T[t-1] + k[t-1]*k[t-1];
        P.U[t] = P.U[t-1] + k[t-1] * static_cast<double>(t);
    }
    return P;
}

struct MSEParts {
    double varK;
    double varR;
    double covKR;
};

static inline MSEParts calc_parts_ab_i(
    const std::vector<double>& k, const Prefix& P,
    int a, int b, int i, int lambda
) {
    // k: 0..n-1, i is 1-based index in paper; here expect i in [2..n-1] => convert carefully
    // We'll treat i as 1-based to follow formulas; in code pass i as 1..n (human-friendly).
    int n = static_cast<int>(k.size());
    int c = lambda - a - b;
    int N = n + lambda;

    double k1 = k[0];
    double ki = k[i-1];
    double kn = k[n-1];

    // means
    double Mx   = (a*k1 + b*ki + c*kn + P.S[n]) / N;
    double Mx2  = (a*k1*k1 + b*ki*ki + c*kn*kn + P.T[n]) / N;
    double Mr   = (N + 1) / 2.0;
    double Mr2  = ((N + 1) * (2.0*N + 1)) / 6.0;

    // Mxr per provided closed form:
    // Mxr = (1/N)[ U_n + a S_n + b (S_n - S_{i-1})
    //           + a(a+1)/2 * k1
    //           + ( b (a + i) + b(b-1)/2 ) * ki
    //           + ( c N - c(c-1)/2 ) * kn ]
    double Sn  = P.S[n];
    double Si1 = P.S[i-1]; // S_{i-1}
    double Un  = P.U[n];

    double Mxr_num = Un
        + a * Sn
        + b * (Sn - Si1)
        + (a * (a + 1) / 2.0) * k1
        + ( b * (a + i) + (b * (b - 1) / 2.0) ) * ki
        + ( c * N - (c * (c - 1) / 2.0) ) * kn;

    double Mxr = Mxr_num / N;

    MSEParts parts;
    parts.varK  = Mx2 - Mx*Mx;
    parts.varR  = Mr2 - Mr*Mr;
    parts.covKR = Mxr - Mx*Mr;
    return parts;
}

static inline double calc_mse_from_parts(const MSEParts& p) {
    // VarR - Cov^2 / VarK
    // Guard VarK>0; in relaxed setting with endpoints present this should hold,
    // but add epsilon for safety.
    const double eps = 1e-15;
    double varK = std::max(p.varK, eps);
    return p.varR - (p.covKR * p.covKR) / varK;
}

// Build the cubic coefficients A, B, C, D for d/d b MSE(a,b,i) = 0
// Based on: -2 (Cov'(b)) VarK(b) + Cov(b) VarK'(b) = 0,
// where Cov(b), VarK(b) are quadratic in b.
// We derive their quadratics and multiply out to obtain a cubic.
struct Quad {
    // q(b) = qb2*b^2 + qb1*b + qb0
    double qb2, qb1, qb0;
};

static Quad varK_quadratic(
    const std::vector<double>& k, const Prefix& P,
    int a, int i, int lambda
) {
    int n = (int)k.size();
    int N = n + lambda;
    double invN = 1.0 / N;

    double k1 = k[0];
    double ki = k[i-1];
    double kn = k[n-1];

    // Mx(b) = (a*k1 + b*ki + (lambda-a-b)*kn + S_n)/N
    //       = ( (ki - kn) b + [a k1 + (lambda-a)kn + S_n] ) / N
    double A1 = (ki - kn) * invN;                       // coeff of b
    double B1 = (a*k1 + (lambda - a)*kn + P.S[n]) * invN; // constant

    // Mx2(b) = (a k1^2 + b ki^2 + (lambda-a-b) kn^2 + T_n) / N
    //        = ( (ki^2 - kn^2) b + [a k1^2 + (lambda-a) kn^2 + T_n] ) / N
    double A2 = (ki*ki - kn*kn) * invN;                 // coeff of b
    double B2 = (a*k1*k1 + (lambda - a)*kn*kn + P.T[n]) * invN; // constant

    // varK(b) = Mx2(b) - Mx(b)^2
    // = (A2 b + B2) - (A1 b + B1)^2
    // = (A2 - A1^2) b^2 + (2(B2*A1) - 2(B1*A1))? let's expand carefully:
    // (A1 b + B1)^2 = A1^2 b^2 + 2 A1 B1 b + B1^2
    // varK(b) = (A2 b + B2) - (A1^2 b^2 + 2 A1 B1 b + B1^2)
    //         = (-A1^2) b^2 + (A2 - 2 A1 B1) b + (B2 - B1^2)
    Quad q;
    q.qb2 = -A1*A1;
    q.qb1 = A2 - 2.0*A1*B1;
    q.qb0 = B2 - B1*B1;
    return q;
}

static Quad cov_quadratic(
    const std::vector<double>& k, const Prefix& P,
    int a, int i, int lambda
) {
    int n = (int)k.size();
    int N = n + lambda;
    double invN = 1.0 / N;

    double k1 = k[0];
    double ki = k[i-1];
    double kn = k[n-1];
    double Sn = P.S[n];
    double Si1 = P.S[i-1];
    double Un = P.U[n];

    // Mx(b) as above:
    double A1 = (ki - kn) * invN;
    double B1 = (a*k1 + (lambda - a)*kn + Sn) * invN;
    // Mr is constant (N+1)/2
    double Mr = (N + 1) / 2.0;

    // Mxr(b) numerator:
    // Num(b) = Un + a Sn + b (Sn - S_{i-1})
    //        + a(a+1)/2 * k1
    //        + ( b (a + i) + b(b-1)/2 ) * ki
    //        + ( c N - c(c-1)/2 ) kn, with c = lambda - a - b
    //
    // Expand Num(b) as quadratic in b:
    // Terms in b^2:
    //   from ki * [b(b-1)/2] -> (ki/2)(b^2 - b)
    //   from kn * [- c(c-1)/2] with c = L - b:
    //        -kn/2 * [(L - b)(L - b - 1)]
    //        = -kn/2 * [(L^2 - (2L-1)b + b^2 - L)]
    //        = (-kn/2) b^2 + kn*(2L-1)/2 * b + const
    //
    // Collect systematically:
    int L = lambda;
    // Coeffs for Num(b) = nb2 b^2 + nb1 b + nb0
    double nb2 = 0.0;
    double nb1 = 0.0;
    double nb0 = 0.0;

    // Base constants:
    nb0 += Un + a*Sn + (a*(a+1)/2.0)*k1;

    // + b (Sn - Si1)
    nb1 += (Sn - Si1);

    // + ( b (a + i) + b(b-1)/2 ) * ki
    //   -> ki * (a+i) * b + ki * (1/2)(b^2 - b)
    nb2 += (ki * 0.5);
    nb1 += (ki * (a + i - 0.5));
    // no nb0 from this line

    // + ( c N - c(c-1)/2 ) * kn , c = L - a - b
    //   c N = (L - a - b) N = const - N b
    //   - c(c-1)/2 = -[(L-a-b)(L-a-b-1)]/2
    // Expand -[(L-a-b)(L-a-b-1)]/2 =
    //   = -[ (L-a)^2 - (2(L-a)-1)b + b^2 ] / 2
    //   = - (L-a)^2/2 + (2(L-a)-1)/2 * b - (1/2) b^2
    // Multiply by kn and add cN*kn:
    // Num_kn =
    //   + kn * [ (L - a) N ]        (const)
    //   - kn * [ N b ]              (b)
    //   + kn * [ - (L-a)^2/2 ]      (const)
    //   + kn * [ (2(L-a)-1)/2 * b ] (b)
    //   + kn * [ - (1/2) b^2 ]      (b^2)
    nb2 += (-0.5 * kn);
    nb1 += (-N * kn) + ( (2.0*(L - a) - 1.0)/2.0 * kn );
    nb0 += ( (L - a) * N * kn ) - ( ((L - a)*(L - a))/2.0 * kn );

    double A_mxr = nb2 * invN; // coeff of b^2
    double B_mxr = nb1 * invN; // coeff of b
    double C_mxr = nb0 * invN; // constant

    // Mxr(b) = A_mxr b^2 + B_mxr b + C_mxr
    // cov(b) = Mxr(b) - Mx(b)*Mr
    //        = (A_mxr) b^2 + (B_mxr - A1*Mr) b + (C_mxr - B1*Mr)
    Quad q;
    q.qb2 = A_mxr;
    q.qb1 = B_mxr - A1 * Mr;
    q.qb0 = C_mxr - B1 * Mr;
    return q;
}

struct Cubic {
    double A, B, C, D; // A b^3 + B b^2 + C b + D = 0
};

// Build cubic coefficients for d/d b MSE = 0:
// -2 cov'(b) * varK(b) + cov(b) * varK'(b) = 0
static Cubic derivative_eq_cubic(
    const std::vector<double>& k, const Prefix& P,
    int a, int i, int lambda
) {
    Quad v = varK_quadratic(k, P, a, i, lambda);
    Quad c = cov_quadratic(k, P, a, i, lambda);

    // varK(b) = v2 b^2 + v1 b + v0
    // cov(b)  = c2 b^2 + c1 b + c0
    // varK'(b) = 2 v2 b + v1
    // cov'(b)  = 2 c2 b + c1
    double v2 = v.qb2, v1 = v.qb1, v0 = v.qb0;
    double c2 = c.qb2, c1 = c.qb1, c0 = c.qb0;

    // -2*cov'(b)*varK(b) + cov(b)*varK'(b) = 0
    // Expand each polynomial:
    // cov'(b) = 2c2 b + c1
    // varK(b) = v2 b^2 + v1 b + v0
    // Part1 = -2 * (2c2 b + c1) * (v2 b^2 + v1 b + v0)
    //       = -2 * [ (2c2 v2) b^3 + (2c2 v1 + c1 v2) b^2 + (2c2 v0 + c1 v1) b + (c1 v0) ]
    // cov(b)*varK'(b) = (c2 b^2 + c1 b + c0) * (2 v2 b + v1)
    //                 = (2 c2 v2) b^3 + (c2 v1 + 2 c1 v2) b^2 + (c1 v1 + 2 c0 v2) b + (c0 v1)
    //
    // Sum: coefficients of cubic A..D
    double A = -2.0 * (2*c2*v2) + (2*c2*v2);
    double B = -2.0 * (2*c2*v1 + c1*v2) + (c2*v1 + 2*c1*v2);
    double C = -2.0 * (2*c2*v0 + c1*v1) + (c1*v1 + 2*c0*v2);
    double D = -2.0 * (c1*v0) + (c0*v1);

    Cubic poly{A, B, C, D};
    return poly;
}

// Choose optimal integer b in [0, L] for fixed (a,i), using the cubic’s real roots and endpoints.
static int get_optimal_b_for_ai(
    const std::vector<double>& k, const Prefix& P,
    int a, int i, int lambda, const MSEParts& basePartsForCheck
) {
    int L = lambda - a;
    if (L <= 0) return 0;

    Cubic cu = derivative_eq_cubic(k, P, a, i, lambda);
    std::vector<double> roots = cubic_real_roots_inline(cu.A, cu.B, cu.C, cu.D);

    // candidate integers: endpoints and neighbors around each root
    std::set<int> cand;
    cand.insert(0);
    cand.insert(L);

    auto add_nb = [&](double r) {
        // Collect up to 6 neighbors: floor, ceil, round ±1 and clamp into [0,L]
        std::array<int,6> xs = {
            (int)std::floor(r),
            (int)std::ceil(r),
            (int)std::llround(r),
            (int)std::llround(r) - 1,
            (int)std::llround(r) + 1,
            (int)std::floor(r) - 1
        };
        for (int x : xs) {
            if (x >= 0 && x <= L) cand.insert(x);
        }
    };
    for (double r : roots) add_nb(r);

    // Evaluate all candidates and pick the best by MSE
    double best_mse = -std::numeric_limits<double>::infinity();
    int    best_b   = 0;
    for (int b : cand) {
        auto parts = calc_parts_ab_i(k, P, a, b, i, lambda);
        double mse = calc_mse_from_parts(parts);
        if (mse > best_mse) {
            best_mse = mse;
            best_b   = b;
        }
    }
    return best_b;
}

// ---- public API -------------------------------------------------------

std::vector<std::uint64_t> get_poison_values_consecutive_w_endpoints_duplicate_allowed(
    const std::vector<double>& data,
    size_t poison_num
) {
    if (data.empty()) {
        throw std::runtime_error("Empty data");
    }
    if (poison_num == 0) {
        return {};
    }
    if (data.size() < 3) {
        std::vector<std::uint64_t> poisons(poison_num, static_cast<std::uint64_t>(std::llround(data.back())));
        return poisons;
    }

    std::vector<double> k = data;
    if (!std::is_sorted(k.begin(), k.end())) {
        std::sort(k.begin(), k.end());
    }
    const int n = (int)k.size();
    const int lambda = (int)poison_num;

    // Precompute S, T, U
    Prefix P = build_prefix(k);

    // Global search over a in [0..lambda], i in [2..n-1]
    double best_mse = -std::numeric_limits<double>::infinity();
    int best_a = 0, best_b = 0, best_i = 2;

    for (int a = 0; a <= lambda; ++a) {
        for (int i = 2; i <= n-1; ++i) {
            // pick best b for this (a,i)
            // (basePartsForCheck unused here but kept to show intended extension)
            MSEParts dummy{};
            int b = get_optimal_b_for_ai(k, P, a, i, lambda, dummy);
            // evaluate
            auto parts = calc_parts_ab_i(k, P, a, b, i, lambda);
            double mse = calc_mse_from_parts(parts);
            if (mse > best_mse) {
                best_mse = mse;
                best_a = a; best_b = b; best_i = i;
            }
        }
    }

    int a = best_a;
    int b = best_b;
    int c = lambda - a - b;

    // Build poison values: a copies of k1, b copies of k_i, c copies of k_n
    std::vector<std::uint64_t> best_poison;
    best_poison.reserve(poison_num);
    for (int t = 0; t < a; ++t) best_poison.push_back((std::uint64_t)std::llround(k.front()));
    for (int t = 0; t < b; ++t) best_poison.push_back((std::uint64_t)std::llround(k[best_i-1]));
    for (int t = 0; t < c; ++t) best_poison.push_back((std::uint64_t)std::llround(k.back()));
    return best_poison;
}


template<typename T>
std::vector<T> get_poison_values_consecutive_w_endpoints_duplicate_allowed(const std::vector<T>& data, 
                                           size_t poison_num) {
    // Convert to double
    T offset = data.front();
    std::vector<double> data_decimal(data.size());
    for (size_t i = 0; i < data.size(); ++i) {
        data_decimal[i] = static_cast<double>(data[i] - offset);
    }
    
    auto poison_values_uint64 = get_poison_values_consecutive_w_endpoints_duplicate_allowed(data_decimal, poison_num);
    
    // Convert back to T
    std::vector<T> poison_values(poison_values_uint64.size());
    for (size_t i = 0; i < poison_values_uint64.size(); ++i) {
        poison_values[i] = static_cast<T>(poison_values_uint64[i] + offset);
    }
    
    return poison_values;
}

PoisonResultConsecutiveWEndpointsDuplicateAllowed inject_poison_consecutive_w_endpoints_duplicate_allowed_to_file(const std::string& input_file, 
                                  const std::string& output_file,
                                  size_t poison_num) {
    if (!std::filesystem::exists(input_file)) {
        throw std::runtime_error("Input file not found: " + input_file);
    }
    
    std::string filename = std::filesystem::path(input_file).filename();
    
    common::DataType dtype;
    std::string dtype_str;
    if (filename.length() >= 7 && filename.substr(filename.length() - 7) == "_uint32") {
        dtype = common::DataType::UINT32;
        dtype_str = "uint32";
    } else if (filename.length() >= 7 && filename.substr(filename.length() - 7) == "_uint64") {
        dtype = common::DataType::UINT64;
        dtype_str = "uint64";
    } else {
        throw std::runtime_error("Unknown data type for file: " + filename);
    }
    
    std::vector<std::uint64_t> poison_values;
    double mse;
    
    if (dtype == common::DataType::UINT32) {
        // Read uint32 data
        auto data = common::read_from_binary<std::uint32_t>(input_file);
        
        // Generate poison values
        auto start_time = std::chrono::high_resolution_clock::now();
        auto poison_values_uint32 = get_poison_values_consecutive_w_endpoints_duplicate_allowed(data, poison_num);
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        double time_taken = static_cast<double>(duration.count()) / 1000000.0;
        
        // Add poison values
        std::vector<std::uint32_t> poisoned_data;
        poisoned_data.reserve(data.size() + poison_values_uint32.size());
        poisoned_data.insert(poisoned_data.end(), data.begin(), data.end());
        poisoned_data.insert(poisoned_data.end(), poison_values_uint32.begin(), poison_values_uint32.end());
        
        // Sort
        std::sort(poisoned_data.begin(), poisoned_data.end());
        
        // Calculate MSE
        mse = calc_loss(poisoned_data);
        
        // Save
        common::write_to_binary(poisoned_data, output_file);
        
        // Convert to uint64 and return
        poison_values.resize(poison_values_uint32.size());
        for (size_t i = 0; i < poison_values_uint32.size(); ++i) {
            poison_values[i] = static_cast<std::uint64_t>(poison_values_uint32[i]);
        }
        
        return {std::filesystem::path(output_file).filename().string(), static_cast<double>(mse), time_taken, poison_values};
        
    } else { // uint64
        // Read uint64 data
        auto data = common::read_from_binary<std::uint64_t>(input_file);
        
        // Generate poison values
        auto start_time = std::chrono::high_resolution_clock::now();
        poison_values = get_poison_values_consecutive_w_endpoints_duplicate_allowed(data, poison_num);
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        double time_taken = static_cast<double>(duration.count()) / 1000000.0;
        
        // Add poison values
        std::vector<std::uint64_t> poisoned_data;
        poisoned_data.reserve(data.size() + poison_values.size());
        poisoned_data.insert(poisoned_data.end(), data.begin(), data.end());
        poisoned_data.insert(poisoned_data.end(), poison_values.begin(), poison_values.end());
        
        // Sort
        std::sort(poisoned_data.begin(), poisoned_data.end());
        
        // Calculate MSE
        mse = calc_loss(poisoned_data);
        
        // Save
        common::write_to_binary(poisoned_data, output_file);
        
        return {std::filesystem::path(output_file).filename().string(), static_cast<double>(mse), time_taken, poison_values};
    }
}

// Explicit template instantiations
template std::vector<std::uint32_t> get_poison_values_consecutive_w_endpoints_duplicate_allowed<std::uint32_t>(const std::vector<std::uint32_t>&, size_t);
template std::vector<std::uint64_t> get_poison_values_consecutive_w_endpoints_duplicate_allowed<std::uint64_t>(const std::vector<std::uint64_t>&, size_t);

} // namespace poisoning 