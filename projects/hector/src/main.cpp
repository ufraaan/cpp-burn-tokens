#include "hector/hector.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <span>
#include <vector>

static void usage(const char* prog) {
    fprintf(stderr, R"(Hector — header-only vector similarity search

Usage:
  %s build --type (flat|ivf) --dims N --metric (l2|cosine|ip)
           [--nlist M] [--output FILE] < vectors.bin

  %s query --index FILE --k N [--nprobe M] < queries.bin > results.txt

Vector binary format: [num(uint32_t)] [dims(uint32_t)] [f32...]
Query output:         query_idx,rank,dist,id per line (CSV to stdout)

Flags:
  --type     Index type: flat (brute force) or ivf (inverted file)
  --dims     Vector dimensionality
  --metric   Distance metric: l2, cosine, inner_product
  --nlist    Number of IVF centroids (default: 256)
  --nprobe   Number of centroid lists to search (default: 1)
  --index    Path to serialized index file
  --output   Output file for serialized index (default: stdout)
  --k        Number of nearest neighbours to return
  --help     Print this message

Examples:
  # Generate 10000 random 128-d vectors, build flat index, query
  python3 -c "
  import struct, random, sys
  n, d = 10000, 128
  sys.stdout.buffer.write(struct.pack('II', n, d))
  for _ in range(n * d): sys.stdout.buffer.write(struct.pack('f', random.gauss(0,1)))
  " | %s build --type flat --dims 128 --metric l2 --output index.bin

  # Query with the first vector
  python3 -c "
  import struct, sys
  n, d = 1, 128
  sys.stdout.buffer.write(struct.pack('II', n, d))
  for _ in range(d): sys.stdout.buffer.write(struct.pack('f', 0.0))
  " | %s query --index index.bin --k 5
)", prog, prog, prog, prog);
}

static bool match(const char* arg, const char* flag) {
    return strcmp(arg, flag) == 0;
}

static int parse_int(const char*& arg, int& i, int argc, char** argv) {
    if (i + 1 >= argc) { fprintf(stderr, "error: %s requires an argument\n", arg); exit(1); }
    return atoi(argv[++i]);
}

static const char* parse_str(const char*& arg, int& i, int argc, char** argv) {
    if (i + 1 >= argc) { fprintf(stderr, "error: %s requires an argument\n", arg); exit(1); }
    return argv[++i];
}

static std::vector<float> read_vectors(FILE* f, int expected_dims) {
    uint32_t num, nd;
    if (fread(&num, sizeof(num), 1, f) != 1 || fread(&nd, sizeof(nd), 1, f) != 1) {
        fprintf(stderr, "error: failed to read vector header\n");
        exit(1);
    }
    if (expected_dims > 0 && static_cast<int>(nd) != expected_dims) {
        fprintf(stderr, "error: expected dims=%d but data has %u\n", expected_dims, nd);
        exit(1);
    }
    size_t total = static_cast<size_t>(num) * nd;
    std::vector<float> v(total);
    if (fread(v.data(), sizeof(float), total, f) != total) {
        fprintf(stderr, "error: failed to read vector data\n");
        exit(1);
    }
    return v;
}

// -----------------------------------------------------------------------
// save helper
// -----------------------------------------------------------------------
static std::ostream* open_output(const char* path) {
    if (!path) return &std::cout;
    auto* ofs = new std::ofstream(path, std::ios::binary);
    if (!*ofs) { fprintf(stderr, "error: cannot open %s for writing\n", path); exit(1); }
    return ofs;
}

// -----------------------------------------------------------------------
// Build helpers — each metric + type combination
// -----------------------------------------------------------------------
template <hector::metric M>
static int build_flat(int dims, const char* output) {
    std::vector<float> vectors = read_vectors(stdin, dims);
    auto t0 = std::chrono::high_resolution_clock::now();

    hector::index_flat<M> idx(dims);
    idx.add(vectors);
    idx.build();

    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::unique_ptr<std::ostream> os(open_output(output));
    idx.save(*os);

    fprintf(stderr, "Built flat index: %zu vectors x %d, %.2f ms\n",
            vectors.size() / static_cast<size_t>(dims), dims, ms);
    return 0;
}

template <hector::metric M>
static int build_ivf(int dims, int nlist, const char* output) {
    std::vector<float> vectors = read_vectors(stdin, dims);
    auto t0 = std::chrono::high_resolution_clock::now();

    hector::index_ivf<M> idx(dims, nlist);
    idx.add(vectors);
    idx.build();

    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::unique_ptr<std::ostream> os(open_output(output));
    idx.save(*os);

    fprintf(stderr, "Built ivf index: %zu vectors x %d, nlist=%d, %.2f ms\n",
            vectors.size() / static_cast<size_t>(dims), dims, nlist, ms);
    return 0;
}

// -----------------------------------------------------------------------
// Query
// -----------------------------------------------------------------------
template <typename Index>
static void run_queries(Index& idx, int k, int nprobe) {
    std::vector<float> queries = read_vectors(stdin, idx.dims());
    uint32_t nq = static_cast<uint32_t>(queries.size() / static_cast<size_t>(idx.dims()));
    auto t0 = std::chrono::high_resolution_clock::now();

    for (uint32_t q = 0; q < nq; ++q) {
        std::span<const float> qv(queries.data() + q * static_cast<size_t>(idx.dims()),
                                  static_cast<size_t>(idx.dims()));
        auto results = idx.query(qv, k, nprobe);

        for (size_t r = 0; r < results.size(); ++r) {
            printf("%u,%zu,%.6f,%u\n", q, r, results[r].first, results[r].second);
        }
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    fprintf(stderr, "Queried %u vectors, k=%d, %.2f ms (%.1f us/q)\n",
            nq, k, ms, ms * 1000.0 / static_cast<double>(nq));
}

static int query_cmd(int argc, char** argv) {
    const char* index_path = nullptr;
    int k = 0;
    int nprobe = 1;

    for (int i = 2; i < argc; ++i) {
        const char* arg = argv[i];
        if (match(arg, "--index"))  index_path = parse_str(arg, i, argc, argv);
        else if (match(arg, "--k")) k = parse_int(arg, i, argc, argv);
        else if (match(arg, "--nprobe")) nprobe = parse_int(arg, i, argc, argv);
        else if (match(arg, "--help")) { usage(argv[0]); return 0; }
        else { fprintf(stderr, "error: unknown flag %s\n", arg); return 1; }
    }

    if (!index_path) { fprintf(stderr, "error: --index is required\n"); return 1; }
    if (k <= 0)      { fprintf(stderr, "error: --k is required and must be > 0\n"); return 1; }

    std::ifstream ifs(index_path, std::ios::binary);
    if (!ifs) { fprintf(stderr, "error: cannot open %s\n", index_path); return 1; }

    uint32_t tag;
    ifs.read(reinterpret_cast<char*>(&tag), sizeof(tag));
    ifs.clear();
    ifs.seekg(0);

    switch (tag) {
        case 0: { auto idx = hector::index_flat<hector::metric_l2>::load(ifs);  run_queries(idx, k, nprobe); break; }
        case 1: { auto idx = hector::index_flat<hector::metric_cos>::load(ifs); run_queries(idx, k, nprobe); break; }
        case 2: { auto idx = hector::index_flat<hector::metric_ip>::load(ifs);  run_queries(idx, k, nprobe); break; }
        case 3: { auto idx = hector::index_ivf<hector::metric_l2>::load(ifs);   run_queries(idx, k, nprobe); break; }
        case 4: { auto idx = hector::index_ivf<hector::metric_cos>::load(ifs);  run_queries(idx, k, nprobe); break; }
        case 5: { auto idx = hector::index_ivf<hector::metric_ip>::load(ifs);   run_queries(idx, k, nprobe); break; }
        default: fprintf(stderr, "error: unknown index tag %u\n", tag); return 1;
    }
    return 0;
}

// -----------------------------------------------------------------------
// Build dispatch
// -----------------------------------------------------------------------
static int build_cmd(int argc, char** argv) {
    const char* type = nullptr;
    const char* metric = nullptr;
    const char* output = nullptr;
    int dims = 0;
    int nlist = 0;

    for (int i = 2; i < argc; ++i) {
        const char* arg = argv[i];
        if (match(arg, "--type"))   type = parse_str(arg, i, argc, argv);
        else if (match(arg, "--dims"))   dims = parse_int(arg, i, argc, argv);
        else if (match(arg, "--metric")) metric = parse_str(arg, i, argc, argv);
        else if (match(arg, "--nlist"))  nlist = parse_int(arg, i, argc, argv);
        else if (match(arg, "--output")) output = parse_str(arg, i, argc, argv);
        else if (match(arg, "--help")) { usage(argv[0]); return 0; }
        else { fprintf(stderr, "error: unknown flag %s\n", arg); return 1; }
    }

    if (!type)   { fprintf(stderr, "error: --type is required (flat|ivf)\n"); return 1; }
    if (!metric) { fprintf(stderr, "error: --metric is required\n"); return 1; }
    if (dims <= 0) { fprintf(stderr, "error: --dims is required and must be > 0\n"); return 1; }

    if (strcmp(type, "flat") == 0) {
        if (strcmp(metric, "l2") == 0) return build_flat<hector::metric_l2>(dims, output);
        if (strcmp(metric, "cosine") == 0) return build_flat<hector::metric_cos>(dims, output);
        if (strcmp(metric, "inner_product") == 0) return build_flat<hector::metric_ip>(dims, output);
        fprintf(stderr, "error: unknown metric '%s'\n", metric);
        return 1;
    }

    if (strcmp(type, "ivf") == 0) {
        if (nlist <= 0) nlist = 256;
        if (strcmp(metric, "l2") == 0) return build_ivf<hector::metric_l2>(dims, nlist, output);
        if (strcmp(metric, "cosine") == 0) return build_ivf<hector::metric_cos>(dims, nlist, output);
        if (strcmp(metric, "inner_product") == 0) return build_ivf<hector::metric_ip>(dims, nlist, output);
        fprintf(stderr, "error: unknown metric '%s'\n", metric);
        return 1;
    }

    fprintf(stderr, "error: unknown index type '%s' (use flat or ivf)\n", type);
    return 1;
}

int main(int argc, char** argv) {
    if (argc < 2) { usage(argv[0]); return 1; }

    if (match(argv[1], "build")) return build_cmd(argc, argv);
    if (match(argv[1], "query")) return query_cmd(argc, argv);
    if (match(argv[1], "--help") || match(argv[1], "-h")) { usage(argv[0]); return 0; }

    fprintf(stderr, "error: unknown command '%s' (use build or query)\n", argv[1]);
    return 1;
}
