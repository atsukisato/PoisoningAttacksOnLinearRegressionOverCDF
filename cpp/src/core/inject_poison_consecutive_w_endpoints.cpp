#include "poisoning/inject_poison_consecutive_w_endpoints.h"
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
#include <set>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <vector>
#include <cstdint>
#include <limits>
#include <tuple>

namespace poisoning {
    
class PreprocessLeft {
public:
    using u64  = std::uint64_t;
    using u128 = unsigned __int128;

    explicit PreprocessLeft(const std::vector<u64>& data, u64 default_p = 1)
        : x_(data), n_(data.size()), default_p_(default_p)
    {
        if (x_.empty()) throw std::runtime_error("data must be non-empty");
        for (size_t k = 1; k < n_; ++k) {
            if (!(x_[k-1] < x_[k])) throw std::runtime_error("data must be strictly increasing");
        }

        pref_sum_.assign(n_ + 1, 0);
        pref_sum_sq_.assign(n_ + 1, 0);
        pref_sum_idx_.assign(n_ + 1, 0);
        for (size_t j = 1; j <= n_; ++j) {
            u128 v = x_[j-1];
            pref_sum_[j]     = pref_sum_[j-1]     + v;
            pref_sum_sq_[j]  = pref_sum_sq_[j-1]  + v*v;
            pref_sum_idx_[j] = pref_sum_idx_[j-1] + v * j;
        }

        suf_sum_.assign(n_ + 2, 0);
        suf_sum_sq_.assign(n_ + 2, 0);
        suf_sum_idx_.assign(n_ + 2, 0);
        for (size_t j = n_; j >= 1; --j) {
            u128 v = x_[j-1];
            suf_sum_[j]     = suf_sum_[j+1]     + v;
            suf_sum_sq_[j]  = suf_sum_sq_[j+1]  + v*v;
            suf_sum_idx_[j] = suf_sum_idx_[j+1] + v * j;
            if (j == 1) break;
        }
    }

    std::tuple<u128,u128,u128,long long> get_sums(size_t i_left) const {
        return get_sums(i_left, default_p_);
    }

    std::tuple<u128,u128,u128,long long> get_sums(size_t i_left, u64 p) const {
        if (i_left >= n_) throw std::runtime_error("i_left out of range");
        if (p == 0)       throw std::runtime_error("p must be >= 1");

        const u64 xi = x_[i_left];
        const u64 A  = xi + 1;
        long long L  = last_added_value(i_left, p);
        
        if (L == 0) {
            return {0, 0, 0, -1LL};
        }
        
        if (L > static_cast<long long>(x_[n_ - 1])) {
            return {0, 0, 0, -1LL};
        }
        
        const u64 b  = L - xi;
        const size_t i1 = i_left + 1;

        const size_t idx = static_cast<size_t>(std::upper_bound(x_.begin(), x_.end(), L) - x_.begin());

        // S1, S3
        u128 S1 = pref_sum_[i1] + sum_range_u128(A, L) + ((idx + 1 <= n_) ? suf_sum_[idx+1] : 0);
        u128 S3 = pref_sum_sq_[i1] + sum_sq_range_u128(A, L) + ((idx + 1 <= n_) ? suf_sum_sq_[idx+1] : 0);

        // S2
        const u128 bb    = b;
        const u128 sum_t = bb * (bb + 1) / 2;
        const u128 sum_t2 = bb * (bb + 1) * (2*bb + 1) / 6;

        u128 S2_left  = pref_sum_idx_[i1];
        u128 S2_block = bb * u128(A - 1) * i1
                        + u128(A - 1) * sum_t
                        + u128(i1)    * sum_t
                        + sum_t2;
        u128 tail_sum_idx = (idx + 1 <= n_) ? suf_sum_idx_[idx+1] : 0;
        u128 tail_sum     = (idx + 1 <= n_) ? suf_sum_[idx+1]     : 0;
        u128 S2_tail      = tail_sum_idx + u128(p) * tail_sum;

        u128 S2 = S2_left + S2_block + S2_tail;
        return {S1, S3, S2, static_cast<long long>(L)}; // (Sx, Sxx, Sxy, last)
    }

private:
    static u128 sum_range_u128(u64 a, u64 b) {
        if (a > b) return 0;
        u128 A = a, B = b;
        return (A + B) * (B - A + 1) / 2;
    }
    static u128 sum_sq_range_u128(u64 a, u64 b) {
        auto tri2 = [](u128 u) -> u128 {
            return u * (u + 1) * (2*u + 1) / 6;
        };
        return tri2(b) - tri2(a-1);
    }

    long long missing_count(size_t i_left, u64 t) const {
        const u64 xi = x_[i_left];
        const size_t rank_t = static_cast<size_t>(std::upper_bound(x_.begin(), x_.end(), t) - x_.begin());
        const u64 originals_in = (rank_t > (i_left+1)) ? (rank_t - (i_left+1)) : 0;
        const u64 total_span   = (t > xi) ? (t - xi) : 0;
        return static_cast<long long>(total_span - originals_in);
    }

    long long last_added_value(size_t i_left, u64 p) const {
        const u64 xi = x_[i_left];
        u64 lo = xi + 1;
        u64 hi = xi + p;
        if (missing_count(i_left, hi) < static_cast<long long>(p)) {
            u64 step = (p == 0 ? 1 : p);
            while (missing_count(i_left, hi) < static_cast<long long>(p)) {
                if (hi > std::numeric_limits<u64>::max() - step) {
                    hi = std::numeric_limits<u64>::max(); break;
                }
                hi += step;
                if (step <= (std::numeric_limits<u64>::max() >> 1)) step <<= 1;
            }
        }
        while (lo < hi) {
            u64 mid = lo + (hi - lo) / 2;
            if (missing_count(i_left, mid) >= static_cast<long long>(p)) hi = mid;
            else                                  lo = mid + 1;
        }
        return static_cast<long long>(lo);
    }

private:
    std::vector<u64> x_;
    size_t n_;
    u64 default_p_;
    std::vector<u128> pref_sum_, pref_sum_sq_, pref_sum_idx_;
    std::vector<u128> suf_sum_,  suf_sum_sq_,  suf_sum_idx_;
};

// ======================= PreprocessRight =======================

class PreprocessRight {
public:
    using u64  = std::uint64_t;
    using u128 = unsigned __int128;

    explicit PreprocessRight(const std::vector<u64>& data, u64 default_p = 1)
        : x_(data), n_(data.size()), default_p_(default_p)
    {
        if (x_.empty()) throw std::runtime_error("data must be non-empty");
        for (size_t k = 1; k < n_; ++k) {
            if (!(x_[k-1] < x_[k])) throw std::runtime_error("data must be strictly increasing");
        }

        // prefix/suffix (1-indexed)
        pref_sum_.assign(n_ + 1, 0);
        pref_sum_sq_.assign(n_ + 1, 0);
        pref_sum_idx_.assign(n_ + 1, 0);
        for (size_t j = 1; j <= n_; ++j) {
            u128 v = x_[j-1];
            pref_sum_[j]     = pref_sum_[j-1]     + v;
            pref_sum_sq_[j]  = pref_sum_sq_[j-1]  + v*v;
            pref_sum_idx_[j] = pref_sum_idx_[j-1] + v * j;
        }

        suf_sum_.assign(n_ + 2, 0);
        suf_sum_sq_.assign(n_ + 2, 0);
        suf_sum_idx_.assign(n_ + 2, 0);
        for (size_t j = n_; j >= 1; --j) {
            u128 v = x_[j-1];
            suf_sum_[j]     = suf_sum_[j+1]     + v;
            suf_sum_sq_[j]  = suf_sum_sq_[j+1]  + v*v;
            suf_sum_idx_[j] = suf_sum_idx_[j+1] + v * j;
            if (j == 1) break;
        }
    }

    std::tuple<u128,u128,u128,long long> get_sums(size_t i_right) const {
        return get_sums(i_right, default_p_);
    }

    std::tuple<u128,u128,u128,long long> get_sums(size_t i_right, u64 p) const {
        if (i_right >= n_) throw std::runtime_error("i_right out of range");
        if (p == 0)        throw std::runtime_error("p must be >= 1");

        const size_t i1 = i_right + 1;
        const u64 xi = x_[i_right];
        const long long R = last_added_value_right(i_right, p);

        if (R == 0) {
            return {0, 0, 0, -1LL};
        }
        
        if (R < static_cast<long long>(x_[0])) {
            return {0, 0, 0, -1LL};
        }
        
        const long long b = xi - R;
        const long long A = R;

        // r = # originals <= R-1 = lower_bound(R)
        const size_t r0 = static_cast<size_t>(std::lower_bound(x_.begin(), x_.end(), R) - x_.begin());
        const size_t r1 = r0;

        // ---- S1, S3 ----
        //   1) prefix <= R-1: pref_sum[r1]
        //   2) block fully dense: sum_range(R, xi-1)
        //   3) suffix >= xi: suf_sum[i1]
        u128 S1 = pref_sum_[r1] + sum_range_u128(A, xi - 1) + suf_sum_[i1];
        u128 S3 = pref_sum_sq_[r1] + sum_sq_range_u128(A, xi - 1) + suf_sum_sq_[i1];

        // ---- S2 ----
        u128 S2_left = pref_sum_idx_[r1];

        const u128 bb = b;
        const u128 sum_t = bb * (bb + 1) / 2;
        const u128 sum_t2 = bb * (bb + 1) * (2*bb + 1) / 6;
        u128 S2_block = bb * u128(A - 1) * r1
                        + u128(A - 1) * sum_t
                        + u128(r1)    * sum_t
                        + sum_t2;

        u128 S2_tail = suf_sum_idx_[i1] + u128(p) * suf_sum_[i1];

        u128 S2 = S2_left + S2_block + S2_tail;

        return {S1, S3, S2, static_cast<long long>(R)};
    }

private:
    // #missing in [t, xi-1], with t>=1 and t<=xi-1
    long long missing_count_right(size_t i_right, u64 t) const {
        const size_t i1 = i_right + 1;
        const u64 xi = x_[i_right];
        if (t > xi - 1) return 0;
        // originals in [t, xi-1] = rank(xi-1) - rank(t-1) = (i1-1) - (# < t)
        const size_t rank_tminus1 = static_cast<size_t>(std::lower_bound(x_.begin(), x_.end(), t) - x_.begin());
        const u64 originals_in = (i1 - 1 >= rank_tminus1) ? u64(i1 - 1 - rank_tminus1) : 0;
        const u64 len = xi - t; // size of [t, xi-1]
        return len - originals_in;
    }

    long long last_added_value_right(size_t i_right, u64 p) const {
        const u64 xi = x_[i_right];
        if (xi == 0) {
            return 0LL;
        }
        const u64 lo_min = 1;
        long long lo = lo_min;
        long long hi = xi - 1;
        if (missing_count_right(i_right, lo) < static_cast<long long>(p)) {
            return 0LL;
        }
        while (lo < hi) {
            long long mid = lo + (hi - lo + 1) / 2;
            if (missing_count_right(i_right, mid) >= static_cast<long long>(p)) {
                lo = mid;
            } else {
                hi = mid - 1;
            }
        }
        return lo;
    }

    static u128 sum_range_u128(u64 a, u64 b) {
        if (a > b) return 0;
        u128 A = a, B = b;
        return (A + B) * (B - A + 1) / 2;
    }
    static u128 sum_sq_range_u128(u64 a, u64 b) {
        if (a > b) return 0;
        auto tri2 = [](u128 u) -> u128 {
            return u * (u + 1) * (2*u + 1) / 6;
        };
        return tri2(b) - tri2(a-1);
    }

private:
    std::vector<u64> x_;
    size_t n_;
    u64 default_p_;
    std::vector<u128> pref_sum_, pref_sum_sq_, pref_sum_idx_;
    std::vector<u128> suf_sum_,  suf_sum_sq_,  suf_sum_idx_;
};



// ====================== get_poison_values_consecutive_no_endpoint ======================

std::vector<std::uint64_t> get_poison_values_consecutive_no_endpoint(const std::vector<std::uint64_t>& data, size_t poison_num) {
    if (data.empty()) {
        throw std::runtime_error("Empty data");
    }
    if (data.size() < 2) {
        return {};
    }
    if (poison_num == 0) {
        return {};
    }

    using u128 = unsigned __int128;
    using boost::multiprecision::cpp_dec_float_50;

    const auto n = data.size();
    const auto N = n + poison_num;

    auto u128_to_dec = [&](u128 v) -> std::string
    {
        if (v == 0) return "0";
        std::string s;
        s.reserve(40);
        while (v > 0) {
            u128 q = v / 10;
            unsigned digit = static_cast<unsigned>(v - q * 10);
            s.push_back(static_cast<char>('0' + digit));
            v = q;
        }
        std::reverse(s.begin(), s.end());
        return s;
    };
    
    auto calc_loss_S = [&](const u128& Sx,
                           const u128& Sxx,
                           const u128& Sxy) -> double
    {
        const std::size_t n_sz = N;
        const u128 n_u = static_cast<u128>(n_sz);
        const u128 Sy  = (n_u * (n_u + 1)) / 2;

        const cpp_dec_float_50 n_mp   = cpp_dec_float_50(n_sz);
        const cpp_dec_float_50 Sx_mp  = cpp_dec_float_50(u128_to_dec(Sx));
        const cpp_dec_float_50 Sxx_mp = cpp_dec_float_50(u128_to_dec(Sxx));
        const cpp_dec_float_50 Sxy_mp = cpp_dec_float_50(u128_to_dec(Sxy));
        const cpp_dec_float_50 Sy_mp  = cpp_dec_float_50(u128_to_dec(Sy));

        const cpp_dec_float_50 VarY = (n_mp * n_mp - cpp_dec_float_50(1)) / cpp_dec_float_50(12);

        const cpp_dec_float_50 n2    = n_mp * n_mp;
        const cpp_dec_float_50 VarX  = (Sxx_mp * n_mp - Sx_mp * Sx_mp) / n2;
        const cpp_dec_float_50 CovXY = (Sxy_mp * n_mp - Sx_mp * Sy_mp) / n2;

        if (VarX == 0) return static_cast<double>(VarY);

        const cpp_dec_float_50 loss = VarY - (CovXY * CovXY) / VarX;
        return static_cast<double>(loss);
    };

    auto prep_left = PreprocessLeft(data, poison_num);
    auto prep_right = PreprocessRight(data, poison_num);

    double best_loss = -1.0 * std::numeric_limits<double>::infinity();
    size_t best_i_left;
    bool best_is_left = false;
    for (size_t i_left = 0; i_left < n; ++i_left) {
        auto [Sx, Sxx, Sxy, L] = prep_left.get_sums(i_left, poison_num);

        // std::cout << " - i_left: " << i_left << ", L: " << L << ", Sx: " << static_cast<long long>(Sx) << ", Sxx: " << static_cast<long long>(Sxx) << ", Sxy: " << static_cast<long long>(Sxy) << std::endl;

        if (L == -1) {
            continue;
        }
        double loss = calc_loss_S(Sx, Sxx, Sxy);
        if (loss > best_loss) {
            best_loss = loss;
            best_i_left = i_left;
            best_is_left = true;
        }
    }

    size_t best_i_right;
    for (size_t i_right = 0; i_right < n; ++i_right) {
        auto [Sx, Sxx, Sxy, R] = prep_right.get_sums(i_right, poison_num);

        // std::cout << " - i_right: " << i_right << ", R: " << R << ", Sx: " << static_cast<long long>(Sx) << ", Sxx: " << static_cast<long long>(Sxx) << ", Sxy: " << static_cast<long long>(Sxy) << std::endl;

        if (R == -1) {
            continue;
        }
        double loss = calc_loss_S(Sx, Sxx, Sxy);
        if (loss > best_loss) {
            best_loss = loss;
            best_i_right = i_right;
            best_is_left = false;
        }
    }

    std::vector<std::uint64_t> best_poison;
    if (best_is_left) {
        std::uint64_t now = data[best_i_left] + 1;
        while (best_poison.size() < poison_num) {
            if (best_i_left + 1 < n && now == data[best_i_left + 1]) {
                best_i_left++;
                now++;
                continue;
            }
            best_poison.push_back(now);
            now++;
        }
    } else {
        std::uint64_t now = data[best_i_right] - 1;
        while (best_poison.size() < poison_num) {
            if (best_i_right >= (size_t)1 && now == data[best_i_right - 1]) {
                best_i_right--;
                now--;
                continue;
            }
            best_poison.push_back(now);
            now--;
        }
        std::reverse(best_poison.begin(), best_poison.end());
    }

    return best_poison;
}

// ====================== get_poison_values_consecutive_no_endpoint ======================



std::vector<std::uint64_t> merge_data(const std::vector<std::uint64_t>& a, const std::vector<std::uint64_t>& b) {
    assert(std::is_sorted(a.begin(), a.end()));
    assert(std::is_sorted(b.begin(), b.end()));

    std::vector<std::uint64_t> out = a;
    out.insert(out.end(), b.begin(), b.end());
    std::inplace_merge(out.begin(), out.begin() + a.size(), out.end());
    return out;
}

void get_left_right_endpoint_candidates(
    const std::vector<std::uint64_t>& data,
    size_t max_endpoint_num,
    std::vector<std::uint64_t>& left_endpoint_candidates,
    std::vector<std::uint64_t>& right_endpoint_candidates
) {
    left_endpoint_candidates.clear();
    right_endpoint_candidates.clear();

    if (data.size() < 2) {
        return;
    }

    std::uint64_t left_endpoint_candidate = data[0] + 1;
    size_t left_idx = 1;
    while (left_endpoint_candidates.size() < max_endpoint_num && left_idx < data.size()) {
        while (left_idx < data.size() && data[left_idx] < left_endpoint_candidate) {
            left_idx++;
        }
        if (left_idx == data.size()) {
            break;
        }
        if (left_endpoint_candidate < data[left_idx]) {
            left_endpoint_candidates.push_back(left_endpoint_candidate);
        }
        if (left_endpoint_candidate == data[data.size() - 1]) {
            break;
        }
        left_endpoint_candidate++;
    }

    std::uint64_t right_endpoint_candidate = data[data.size() - 1] - 1;
    int right_idx = static_cast<int>(data.size()) - 2;
    while (right_endpoint_candidates.size() < max_endpoint_num && right_idx >= 0) {
        while (right_idx >= 0 && data[right_idx] > right_endpoint_candidate) {
            right_idx--;
        }
        if (right_idx < 0) {
            break;
        }
        if (right_endpoint_candidate > data[right_idx]) {
            right_endpoint_candidates.push_back(right_endpoint_candidate);
        }
        if (right_endpoint_candidate == data[0]) {
            break;
        }
        right_endpoint_candidate--;
    }
}


std::vector<std::uint64_t> get_poison_values_consecutive_w_endpoints(const std::vector<std::uint64_t>& data, size_t poison_num) {
    if (data.empty()) {
        throw std::runtime_error("Empty data");
    }
    if (poison_num == 0) {
        return {};
    }

    // std::cout << "--------------------------------" << std::endl;
    // std::cout << "data: ";
    // for (auto x : data) std::cout << x << " ";
    // std::cout << std::endl;
    // std::cout << "poison_num: " << poison_num << std::endl;
    // std::cout << "--------------------------------" << std::endl;

    double best_loss = -1.0 * std::numeric_limits<double>::infinity();
    std::vector<std::uint64_t> best_poison;

    size_t MAX_ENDPOINT_NUM = poison_num;

    std::vector<std::uint64_t> left_endpoint_candidates;
    std::vector<std::uint64_t> right_endpoint_candidates;
    get_left_right_endpoint_candidates(data, MAX_ENDPOINT_NUM, left_endpoint_candidates, right_endpoint_candidates);

    for (size_t left_endpoint_num = 0; left_endpoint_num <= left_endpoint_candidates.size(); ++left_endpoint_num) {
        for (size_t right_endpoint_num = 0; right_endpoint_num <= right_endpoint_candidates.size(); ++right_endpoint_num) {
            if (left_endpoint_num + right_endpoint_num > poison_num) {
                continue;
            }
            std::vector<std::uint64_t> poisons_left, poisons_right;
            for (size_t i = 0; i < left_endpoint_num; ++i) {
                poisons_left.push_back(left_endpoint_candidates[i]);
            }
            for (int i = right_endpoint_num - 1; i >= 0; --i) {
                poisons_right.push_back(right_endpoint_candidates[i]);
            }
            std::vector<std::uint64_t> data_ = merge_data(data, poisons_left);
            data_ = merge_data(data_, poisons_right);

            if (data_.size() == data.size() + left_endpoint_num + right_endpoint_num && std::is_sorted(data_.begin(), data_.end())) {
                // OK
            } else {
                throw std::runtime_error("Data size mismatch after inserting endpoints: " + std::to_string(data_.size()) + " vs " + std::to_string(data.size() + left_endpoint_num + right_endpoint_num) + ", is_sorted=" + (std::is_sorted(data_.begin(), data_.end()) ? "true" : "false"));
            }

            // std::cout << " - left_endpoint_num: " << left_endpoint_num << ", right_endpoint_num: " << right_endpoint_num << std::endl;
            // std::cout << " - data_: ";
            // for (auto x : data_) std::cout << x << " ";
            // std::cout << std::endl;
            // std::cout << " - poison_num - left_endpoint_num - right_endpoint_num: " << poison_num - left_endpoint_num - right_endpoint_num << std::endl;

            auto poison_values_consecutive = get_poison_values_consecutive_no_endpoint(data_, poison_num - left_endpoint_num - right_endpoint_num);

            // std::cout << " - poison_values_consecutive: ";
            // for (auto x : poison_values_consecutive) std::cout << x << " ";
            // std::cout << std::endl << std::endl;

            std::vector<std::uint64_t> merged_data = merge_data(data_, poison_values_consecutive);
            double loss = calc_loss(merged_data);
            if (loss > best_loss) {
                best_loss = loss;
                best_poison = std::move(poison_values_consecutive);
                best_poison.insert(best_poison.end(), left_endpoint_candidates.begin(), left_endpoint_candidates.begin() + left_endpoint_num);
                best_poison.insert(best_poison.end(), right_endpoint_candidates.begin(), right_endpoint_candidates.begin() + right_endpoint_num);
                std::sort(best_poison.begin(), best_poison.end());
            }
        }
    }

    return best_poison;
}


template<typename T>
std::vector<T> get_poison_values_consecutive_w_endpoints(const std::vector<T>& data, 
                                           size_t poison_num) {
    // Convert to uint64_t with offset
    T offset = data.front();
    std::vector<std::uint64_t> data_decimal(data.size());
    for (size_t i = 0; i < data.size(); ++i) {
        data_decimal[i] = static_cast<std::uint64_t>(data[i] - offset);
    }
    
    auto poison_values_uint64 = get_poison_values_consecutive_w_endpoints(data_decimal, poison_num);
    
    // Convert back to T
    std::vector<T> poison_values(poison_values_uint64.size());
    for (size_t i = 0; i < poison_values_uint64.size(); ++i) {
        poison_values[i] = static_cast<T>(poison_values_uint64[i] + offset);
    }
    
    return poison_values;
}

template<typename T>
bool has_duplicate(const std::vector<T>& vec) {
    std::set<T> seen;
    for (const auto& val : vec) {
        seen.insert(val);
    }
    return seen.size() != vec.size();
}

PoisonResultConsecutiveWEndpoints inject_poison_consecutive_w_endpoints_to_file(const std::string& input_file, 
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
        auto offset = data.front();
        for (auto& x : data) {
            x -= offset;
        }
        
        // Generate poison values
        auto start_time = std::chrono::high_resolution_clock::now();
        auto poison_values_uint32 = get_poison_values_consecutive_w_endpoints(data, poison_num);
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        double time_taken = static_cast<double>(duration.count()) / 1000000.0;

        if (has_duplicate(poison_values_uint32)) {
            throw std::runtime_error("Duplicate poison values detected");
        }
        
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
        auto offset = data.front();
        for (auto& x : data) {
            x -= offset;
        }

        // data = {1, 2, 4, 17, 20, 21};
        // poison_num = 3;
        
        // Generate poison values
        auto start_time = std::chrono::high_resolution_clock::now();
        poison_values = get_poison_values_consecutive_w_endpoints(data, poison_num);
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        double time_taken = static_cast<double>(duration.count()) / 1000000.0;

        if (has_duplicate(poison_values)) {
            throw std::runtime_error("Duplicate poison values detected");
        }
        
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
template std::vector<std::uint32_t> get_poison_values_consecutive_w_endpoints<std::uint32_t>(const std::vector<std::uint32_t>&, size_t);
template std::vector<std::uint64_t> get_poison_values_consecutive_w_endpoints<std::uint64_t>(const std::vector<std::uint64_t>&, size_t);

} // namespace poisoning 