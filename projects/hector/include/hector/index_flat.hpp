#pragma once

#include "distance.hpp"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <istream>
#include <numeric>
#include <ostream>
#include <span>
#include <vector>

namespace hector {

template <metric Metric>
class index_flat {
public:
    explicit index_flat(int dims) noexcept : dims_(dims) {
        assert(dims > 0);
    }

    void add(std::span<const float> vectors) {
        assert(vectors.size() % static_cast<size_t>(dims_) == 0);
        size_t n = vectors.size() / static_cast<size_t>(dims_);
        for (size_t i = 0; i < n; ++i) {
            ids_.push_back(next_id_++);
        }
        data_.insert(data_.end(), vectors.begin(), vectors.end());
    }

    void build() noexcept {}

    auto query(std::span<const float> query, int k, int = 1) const
        -> std::vector<std::pair<float, uint32_t>>
    {
        assert(static_cast<int>(query.size()) == dims_);
        int n = size();
        if (n == 0) return {};

        std::vector<std::pair<float, uint32_t>> results;
        results.reserve(static_cast<size_t>(n));

        for (int i = 0; i < n; ++i) {
            float d = distance<Metric>(query.data(), data_.data() + i * dims_, dims_);
            results.emplace_back(d, ids_[i]);
        }

        auto nth = results.begin() + std::min(k, n);
        std::nth_element(results.begin(), nth, results.end());
        results.resize(static_cast<size_t>(nth - results.begin()));
        std::sort(results.begin(), results.end());
        return results;
    }

    void save(std::ostream& os) const {
        uint32_t tag = 0;
        save_tag(tag);
        os.write(reinterpret_cast<const char*>(&tag), sizeof(tag));

        uint32_t dim = static_cast<uint32_t>(dims_);
        os.write(reinterpret_cast<const char*>(&dim), sizeof(dim));

        uint32_t n = static_cast<uint32_t>(size());
        os.write(reinterpret_cast<const char*>(&n), sizeof(n));

        os.write(reinterpret_cast<const char*>(data_.data()),
                 static_cast<std::streamsize>(n * dim * sizeof(float)));
        os.write(reinterpret_cast<const char*>(ids_.data()),
                 static_cast<std::streamsize>(n * sizeof(uint32_t)));
    }

    static index_flat load(std::istream& is) {
        uint32_t tag;
        is.read(reinterpret_cast<char*>(&tag), sizeof(tag));

        uint32_t dim;
        is.read(reinterpret_cast<char*>(&dim), sizeof(dim));

        uint32_t n;
        is.read(reinterpret_cast<char*>(&n), sizeof(n));

        index_flat idx(static_cast<int>(dim));
        idx.data_.resize(n * dim);
        idx.ids_.resize(n);
        is.read(reinterpret_cast<char*>(idx.data_.data()),
                static_cast<std::streamsize>(n * dim * sizeof(float)));
        is.read(reinterpret_cast<char*>(idx.ids_.data()),
                static_cast<std::streamsize>(n * sizeof(uint32_t)));
        idx.next_id_ = static_cast<uint32_t>(n);
        return idx;
    }

    int size() const noexcept {
        return static_cast<int>(data_.size() / static_cast<size_t>(dims_));
    }

    int dims() const noexcept { return dims_; }

private:
    static void save_tag(uint32_t& tag) {
        if constexpr (std::is_same_v<Metric, metric_l2>)  tag = 0;
        else if constexpr (std::is_same_v<Metric, metric_cos>) tag = 1;
        else if constexpr (std::is_same_v<Metric, metric_ip>)  tag = 2;
    }

    int dims_;
    uint32_t next_id_ = 0;
    std::vector<float> data_;
    std::vector<uint32_t> ids_;
};

} // namespace hector
