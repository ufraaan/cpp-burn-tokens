#pragma once

#include "simd.hpp"
#include <cmath>
#include <type_traits>

namespace hector {

struct metric_l2   { static constexpr const char* name = "l2"; };
struct metric_cos  { static constexpr const char* name = "cosine"; };
struct metric_ip   { static constexpr const char* name = "inner_product"; };

namespace detail {

template <typename M> struct is_metric : std::false_type {};

template <> struct is_metric<metric_l2> : std::true_type {};
template <> struct is_metric<metric_cos> : std::true_type {};
template <> struct is_metric<metric_ip> : std::true_type {};

} // namespace detail

template <typename M>
concept metric = detail::is_metric<M>::value;

template <metric M>
inline float distance(const float* a, const float* b, int dims) noexcept {
    if constexpr (std::is_same_v<M, metric_l2>) {
        return simd::l2(a, b, dims);
    } else if constexpr (std::is_same_v<M, metric_cos>) {
        float dot = simd::dot(a, b, dims);
        float na = std::sqrt(simd::dot(a, a, dims));
        float nb = std::sqrt(simd::dot(b, b, dims));
        return 1.0f - dot / (na * nb + 1e-10f);
    } else if constexpr (std::is_same_v<M, metric_ip>) {
        return -simd::dot(a, b, dims);
    }
}

} // namespace hector
