#include "poisoning/get_quads.h"
#include <cassert>
#include <cmath>
#include <algorithm>
#include <vector>
#include <limits>

namespace poisoning {

// ---------------- Ball interval: x \in [m - r, m + r] ----------------
struct Ball {
    double m; // center
    double r; // radius (>=0)
    Ball() : m(0.0), r(0.0) {}
    explicit Ball(double x) : m(x), r(std::abs(x)*std::numeric_limits<double>::epsilon()*2.0 + 1e-18) {}
    Ball(double m_, double r_) : m(m_), r(r_ < 0.0 ? 0.0 : r_) {}

    inline double lo() const { return m - r; }
    inline double hi() const { return m + r; }
};

static inline Ball C(double x) { return Ball(x); }
static constexpr double SAFE_K = 1.0 + 1e-18;

static inline double absv(double x) { return std::fabs(x); }

static inline Ball add(const Ball& a, const Ball& b) {
    return Ball(a.m + b.m, SAFE_K * (a.r + b.r));
}
static inline Ball sub(const Ball& a, const Ball& b) {
    return Ball(a.m - b.m, SAFE_K * (a.r + b.r));
}

static inline Ball mul(const Ball& a, const Ball& b) {
    const double m = a.m * b.m;
    const double r = SAFE_K * (absv(a.m)*b.r + absv(b.m)*a.r + a.r*b.r);
    return Ball(m, r);
}

static inline Ball square(const Ball& x) {
    const double m = x.m * x.m;
    const double r = SAFE_K * (2.0 * absv(x.m) * x.r + x.r * x.r);
    return Ball(m, r);
}

static inline Ball div(const Ball& a, const Ball& b) {
    const double b_lo = b.lo();
    const double b_hi = b.hi();
    assert(!(b_lo <= 0.0 && b_hi >= 0.0));

    const double m = a.m / b.m;
    const double inv_b_abs = 1.0 / absv(b.m);
    double r = absv(inv_b_abs) * a.r + absv(a.m) * (inv_b_abs*inv_b_abs) * b.r;
    r += (a.r * b.r) * (inv_b_abs * inv_b_abs);
    return Ball(m, SAFE_K * r);
}

static inline Ball adds(const Ball& x, double s) { return Ball(x.m + s, SAFE_K * x.r); }
static inline Ball subs(const Ball& x, double s) { return Ball(x.m - s, SAFE_K * x.r); }
static inline Ball ssub(double s, const Ball& x) { return Ball(s - x.m, SAFE_K * x.r); }

static inline Ball muls(const Ball& x, double s) {
    const double am = absv(s);
    return Ball(x.m * s, SAFE_K * (am * x.r));
}
static inline Ball divs(const Ball& x, double s) {
    assert(s != 0.0);
    const double inv = 1.0 / s;
    return Ball(x.m * inv, SAFE_K * (absv(inv) * x.r));
}
static inline Ball sdiv(double s, const Ball& x) {
    const double x_lo = x.lo(), x_hi = x.hi();
    assert(!(x_lo <= 0.0 && x_hi >= 0.0));
    const double m = s / x.m;
    const double inv_abs = 1.0 / absv(x.m);
    double r = absv(inv_abs) * 0.0 + absv(s) * (inv_abs * inv_abs) * x.r;
    r += 0.0;
    return Ball(m, SAFE_K * r);
}

std::vector<Quad> get_quads(const std::vector<double>& xs, int p) {
    const int n = (int)xs.size();
    assert(n >= 1);
    assert(p >= 0);

    const int total = n + p;
    const double Dtotal = (double)total;

    const Ball B0 = C(0.0);
    const Ball B1 = C(1.0);
    const Ball B2 = C(2.0);

    const Ball D  = C(Dtotal);
    const Ball Dp = C((double)p);

    // S_tot = D*(D+1)/2, S5 = (D-1)D(D+1)/12
    const Ball Dp1 = adds(D, 1.0);   // D + 1
    const Ball Dm1 = subs(D, 1.0);   // D - 1
    const Ball S_tot = divs(mul(D, Dp1), 2.0);
    const Ball S5    = divs(mul(mul(Dm1, D), Dp1), 12.0);

    std::vector<Ball> xB; xB.reserve(n);
    std::vector<Ball> x2B; x2B.reserve(n);
    Ball sum_x = B0, sum_x2 = B0;

    for (double x : xs) {
        Ball xb = C(x);
        xB.emplace_back(xb);
        Ball x2 = square(xb);
        x2B.emplace_back(x2);
        sum_x  = add(sum_x, xb);
        sum_x2 = add(sum_x2, x2);
    }

    std::vector<Quad> quads;
    quads.reserve((size_t)(p + 1 + n));

    // Case-1
    const Ball x1 = xB.front();
    const Ball xn = xB.back();
    const Ball mid_sum = sub(sub(sum_x, x1), xn); // sum_x - x1 - xn

    // A = sum_{j=1}^{n-2} x_j * j
    Ball A = B0;
    for (int j = 1; j < n - 1; ++j) {
        A = add(A, muls(xB[j], (double)j));
    }
    const Ball B = mid_sum;
    const Ball n_minus_1 = C((double)(n - 1));

    for (int k = 0; k <= p; ++k) {
        const Ball c1 = adds(B1, (double)k);
        const Ball cn = adds(B1, (double)(p - k));

        const Ball S1 = add(sum_x, add(muls(x1, (double)k), muls(xn, (double)(p - k))));
        const Ball S4 = add(sum_x2, add(muls(x2B.front(), (double)k), muls(x2B.back(), (double)(p - k))));

        Ball S3 = mid_sum;
        S3 = add(S3, mul(x1, div(mul(c1, adds(c1, 1.0)), B2)));
        S3 = add(S3, mul(xn, div(mul(cn, adds(cn, 1.0)), B2)));

        Ball S2 = add(muls(B, (double)k), A);
        S2 = add(S2, mul(mul(xn, cn), adds(n_minus_1, (double)k)));

        const Ball aI = sub(S4, div(mul(S1, S1), D));
        const Ball t  = sub(add(S2, S3), mul(div(S1, D), S_tot));
        const Ball bI = muls(t, -2.0);
        const Ball cI = S5;

        double a = aI.hi(); if (a < 0.0) a = 0.0;
        const double bcoef = bI.hi();
        const double ccoef = cI.hi();

        quads.emplace_back(a, bcoef, ccoef);
    }

    // Case-2
    Ball S2_base = B0;
    Ball prefix  = B0;
    for (int i = 0; i < n; ++i) {
        S2_base = add(S2_base, mul(xB[i], prefix));
        prefix  = add(prefix, B1);
    }
    const Ball S3_base = sum_x;

    std::vector<Ball> suffix_sum_x(n, B0);
    Ball running = B0;
    for (int idx = n - 1; idx >= 0; --idx) {
        suffix_sum_x[idx] = running;
        running = add(running, xB[idx]);
    }

    const Ball inc = sub(div(mul(adds(Dp, 1.0), adds(Dp, 2.0)), B2), B1);

    for (int i = 0; i < n; ++i) {
        const Ball& x_i  = xB[i];
        const Ball& x2_i = x2B[i];

        const Ball S1 = add(sum_x, mul(Dp, x_i));
        const Ball S4 = add(sum_x2, mul(Dp, x2_i));
        const Ball S3 = add(S3_base, mul(x_i, inc));
        const Ball S2 = add(S2_base, mul(Dp, add(muls(x_i, (double)i), suffix_sum_x[i])));

        const Ball aI = sub(S4, div(mul(S1, S1), D));
        const Ball t  = sub(add(S2, S3), mul(div(S1, D), S_tot));
        const Ball bI = muls(t, -2.0);
        const Ball cI = S5;

        double a = aI.hi(); if (a < 0.0) a = 0.0;
        const double bcoef = bI.hi();
        const double ccoef = cI.hi();

        quads.emplace_back(a, bcoef, ccoef);
    }

    return quads;
}

} // namespace poisoning
