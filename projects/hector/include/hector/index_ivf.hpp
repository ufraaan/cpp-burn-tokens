#pragma once

#include "distance.hpp"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <istream>
#include <limits>
#include <numeric>
#include <ostream>
#include <random>
#include <span>
#include <vector>

namespace hector {

template <metric Metric>
class index_ivf {
public:
    explicit index_ivf(int dims, int nlist) noexcept
        : dims_(dims), nlist_(nlist)
    {
        assert(dims > 0);
        assert(nlist > 0);
    }

    void add(std::span<const float> vectors) {
        assert(vectors.size() % static_cast<size_t>(dims_) == 0);
        size_t n = vectors.size() / static_cast<size_t>(dims_);
        for (size_t i = 0; i < n; ++i) {
            ids_.push_back(next_id_++);
        }
        data_.insert(data_.end(), vectors.begin(), vectors.end());
    }

    void build() {
        int n = size();
        if (n == 0) return;

        int nc = std::min(nlist_, n);
        centroids_.resize(static_cast<size_t>(nc) * dims_);
        assignments_.resize(static_cast<size_t>(n));
        init_kmeans_pp(nc);

        std::vector<int> counts(static_cast<size_t>(nc), 0);

        for (int iter = 0; iter < 20; ++iter) {
            for (int i = 0; i < n; ++i) {
                const float* v = data_.data() + i * dims_;
                int best = 0;
                float best_d = distance<Metric>(v, centroids_.data(), dims_);
                for (int c = 1; c < nc; ++c) {
                    float d = distance<Metric>(v, centroids_.data() + c * dims_, dims_);
                    if (d < best_d) { best_d = d; best = c; }
                }
                assignments_[i] = best;
            }

            std::fill(counts.begin(), counts.end(), 0);
            std::fill(centroids_.begin(), centroids_.end(), 0.0f);
            for (int i = 0; i < n; ++i) {
                int c = assignments_[i];
                counts[c]++;
                for (int d = 0; d < dims_; ++d) {
                    centroids_[c * dims_ + d] += data_[i * dims_ + d];
                }
            }
            for (int c = 0; c < nc; ++c) {
                if (counts[c] > 0) {
                    float inv = 1.0f / static_cast<float>(counts[c]);
                    for (int d = 0; d < dims_; ++d) {
                        centroids_[c * dims_ + d] *= inv;
                    }
                }
            }
        }

        lists_.resize(static_cast<size_t>(nc));
        for (int c = 0; c < nc; ++c) {
            lists_[c].clear();
        }
        for (int i = 0; i < n; ++i) {
            int c = assignments_[i];
            const float* v = data_.data() + i * dims_;
            auto& list = lists_[c];
            size_t base = list.size();
            list.resize(base + static_cast<size_t>(dims_) + 1);
            std::memcpy(&list[base], &ids_[i], sizeof(uint32_t));
            std::memcpy(&list[base] + 1, v, static_cast<size_t>(dims_) * sizeof(float));
        }
    }

    auto query(std::span<const float> query, int k, int nprobe = 1) const
        -> std::vector<std::pair<float, uint32_t>>
    {
        assert(static_cast<int>(query.size()) == dims_);
        int nc = nlist_;

        std::vector<std::pair<float, int>> cent_dists;
        cent_dists.reserve(static_cast<size_t>(nc));
        for (int c = 0; c < nc; ++c) {
            float d = distance<Metric>(query.data(), centroids_.data() + c * dims_, dims_);
            cent_dists.emplace_back(d, c);
        }
        std::sort(cent_dists.begin(), cent_dists.end());

        int probe = std::min(nprobe, nc);
        std::vector<std::pair<float, uint32_t>> results;

        for (int pi = 0; pi < probe; ++pi) {
            int c = cent_dists[pi].second;
            const auto& list = lists_[c];
            size_t list_n = list.size() / (static_cast<size_t>(dims_) + 1);
            for (size_t i = 0; i < list_n; ++i) {
                const float* entry = &list[i * (static_cast<size_t>(dims_) + 1)];
                uint32_t id;
                std::memcpy(&id, entry, sizeof(id));
                float d = distance<Metric>(query.data(), entry + 1, dims_);
                results.emplace_back(d, id);
            }
        }

        auto nth = results.begin() + std::min(k, static_cast<int>(results.size()));
        std::nth_element(results.begin(), nth, results.end());
        results.resize(static_cast<size_t>(nth - results.begin()));
        std::sort(results.begin(), results.end());
        return results;
    }

    void save(std::ostream& os) const {
        uint32_t tag = 3;
        {
            if constexpr (std::is_same_v<Metric, metric_l2>)  tag = 3;
            else if constexpr (std::is_same_v<Metric, metric_cos>) tag = 4;
            else if constexpr (std::is_same_v<Metric, metric_ip>)  tag = 5;
        }
        os.write(reinterpret_cast<const char*>(&tag), sizeof(tag));

        uint32_t dim = static_cast<uint32_t>(dims_);
        uint32_t nc  = static_cast<uint32_t>(nlist_);
        uint32_t n   = static_cast<uint32_t>(size());
        os.write(reinterpret_cast<const char*>(&dim), sizeof(dim));
        os.write(reinterpret_cast<const char*>(&nc), sizeof(nc));
        os.write(reinterpret_cast<const char*>(&n), sizeof(n));

        size_t cv = static_cast<size_t>(nc) * dim;
        os.write(reinterpret_cast<const char*>(centroids_.data()),
                 static_cast<std::streamsize>(cv * sizeof(float)));

        for (uint32_t c = 0; c < nc; ++c) {
            const auto& list = lists_[c];
            uint32_t ln = static_cast<uint32_t>(list.size() / (static_cast<size_t>(dims_) + 1));
            os.write(reinterpret_cast<const char*>(&ln), sizeof(ln));
            os.write(reinterpret_cast<const char*>(list.data()),
                     static_cast<std::streamsize>(list.size() * sizeof(float)));
        }

        os.write(reinterpret_cast<const char*>(data_.data()),
                 static_cast<std::streamsize>(n * dim * sizeof(float)));
        os.write(reinterpret_cast<const char*>(ids_.data()),
                 static_cast<std::streamsize>(n * sizeof(uint32_t)));
    }

    static index_ivf load(std::istream& is) {
        uint32_t tag;
        is.read(reinterpret_cast<char*>(&tag), sizeof(tag));

        uint32_t dim, nc, n;
        is.read(reinterpret_cast<char*>(&dim), sizeof(dim));
        is.read(reinterpret_cast<char*>(&nc), sizeof(nc));
        is.read(reinterpret_cast<char*>(&n), sizeof(n));

        index_ivf idx(static_cast<int>(dim), static_cast<int>(nc));
        idx.centroids_.resize(static_cast<size_t>(nc) * dim);
        is.read(reinterpret_cast<char*>(idx.centroids_.data()),
                static_cast<std::streamsize>(static_cast<size_t>(nc) * dim * sizeof(float)));

        idx.lists_.resize(static_cast<size_t>(nc));
        for (uint32_t c = 0; c < nc; ++c) {
            uint32_t ln;
            is.read(reinterpret_cast<char*>(&ln), sizeof(ln));
            idx.lists_[c].resize(static_cast<size_t>(ln) * (static_cast<size_t>(dim) + 1));
            is.read(reinterpret_cast<char*>(idx.lists_[c].data()),
                    static_cast<std::streamsize>(idx.lists_[c].size() * sizeof(float)));
        }

        idx.data_.resize(static_cast<size_t>(n) * dim);
        idx.ids_.resize(static_cast<size_t>(n));
        is.read(reinterpret_cast<char*>(idx.data_.data()),
                static_cast<std::streamsize>(static_cast<size_t>(n) * dim * sizeof(float)));
        is.read(reinterpret_cast<char*>(idx.ids_.data()),
                static_cast<std::streamsize>(static_cast<size_t>(n) * sizeof(uint32_t)));
        idx.next_id_ = static_cast<uint32_t>(n);

        idx.assignments_.resize(static_cast<size_t>(n));
        for (uint32_t i = 0; i < n; ++i) {
            float best_d = std::numeric_limits<float>::max();
            int best_c = 0;
            for (uint32_t c = 0; c < nc; ++c) {
                float d = distance<Metric>(idx.data_.data() + i * dim,
                                           idx.centroids_.data() + c * dim,
                                           static_cast<int>(dim));
                if (d < best_d) { best_d = d; best_c = static_cast<int>(c); }
            }
            idx.assignments_[i] = best_c;
        }
        return idx;
    }

    int size() const noexcept {
        return static_cast<int>(data_.size() / static_cast<size_t>(dims_));
    }
    int dims() const noexcept { return dims_; }
    int nlist() const noexcept { return nlist_; }

private:
    void init_kmeans_pp(int nc) {
        int n = size();
        if (n == 0 || nc <= 0) return;

        std::mt19937 rng(42);

        int first = std::uniform_int_distribution<int>(0, n - 1)(rng);
        std::memcpy(centroids_.data(), &data_[first * dims_],
                    static_cast<size_t>(dims_) * sizeof(float));

        std::vector<float> min_dists(static_cast<size_t>(n),
                                     std::numeric_limits<float>::max());

        for (int c = 1; c < nc; ++c) {
            const float* prev_cent = centroids_.data() + (c - 1) * dims_;
            for (int i = 0; i < n; ++i) {
                float d = distance<Metric>(&data_[i * dims_], prev_cent, dims_);
                if (d < min_dists[i]) min_dists[i] = d;
            }

            float total = 0;
            for (int i = 0; i < n; ++i) total += min_dists[i];

            std::uniform_real_distribution<float> dist(0.0f, total);
            float r = dist(rng);
            float cum = 0.0f;
            int chosen = 0;
            for (int i = 0; i < n; ++i) {
                cum += min_dists[i];
                if (cum >= r) { chosen = i; break; }
            }

            std::memcpy(centroids_.data() + c * dims_, &data_[chosen * dims_],
                        static_cast<size_t>(dims_) * sizeof(float));
        }
    }

    int dims_;
    int nlist_;
    uint32_t next_id_ = 0;
    std::vector<float> data_;
    std::vector<uint32_t> ids_;
    std::vector<int> assignments_;
    std::vector<float> centroids_;
    std::vector<std::vector<float>> lists_;
};

} // namespace hector
