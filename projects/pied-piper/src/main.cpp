#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <chrono>
#include <algorithm>
#include <filesystem>

#include "huffman.hpp"
#include "lzw.hpp"

namespace fs = std::filesystem;

// ─── File I/O ──────────────────────────────────────────────────────────
static bool read_file(const std::string& path, std::vector<uint8_t>& buf) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) { fprintf(stderr, "error: cannot open '%s'\n", path.c_str()); return false; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    if (sz < 0) { fclose(f); return false; }
    buf.resize((size_t)sz);
    if (sz > 0 && fread(buf.data(), 1, (size_t)sz, f) != (size_t)sz) {
        fclose(f); return false;
    }
    fclose(f);
    return true;
}

static bool write_file(const std::string& path, const std::vector<uint8_t>& buf) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) { fprintf(stderr, "error: cannot write '%s'\n", path.c_str()); return false; }
    if (!buf.empty() && fwrite(buf.data(), 1, buf.size(), f) != buf.size()) {
        fclose(f); return false;
    }
    fclose(f);
    return true;
}

static std::string size_str(uint64_t bytes) {
    char buf[64];
    if (bytes < 1024)               snprintf(buf, sizeof(buf), "%llu B", (unsigned long long)bytes);
    else if (bytes < 1024*1024)     snprintf(buf, sizeof(buf), "%.2f KB", (double)bytes/1024);
    else if (bytes < 1024LL*1024*1024) snprintf(buf, sizeof(buf), "%.2f MB", (double)bytes/(1024*1024));
    else                            snprintf(buf, sizeof(buf), "%.2f GB", (double)bytes/(1024LL*1024*1024));
    return buf;
}

// ─── Commands ──────────────────────────────────────────────────────────
static int cmd_compress(const std::string& input, const std::string& algo_str) {
    std::vector<uint8_t> data;
    if (!read_file(input, data)) return 1;

    bool use_huf = algo_str.empty() || algo_str == "auto" || algo_str == "huf" || algo_str == "huffman";
    bool use_lzw = algo_str.empty() || algo_str == "auto" || algo_str == "lzw";

    auto run = [&](const std::string& name, auto&& compress_fn) {
        auto t0 = std::chrono::high_resolution_clock::now();
        auto result = compress_fn(data);
        auto t1 = std::chrono::high_resolution_clock::now();
        double total_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

        std::string out_path = input + "." + name + ".cmp";
        write_file(out_path, result.compressed);

        printf("  %-8s  %s → %s  (ratio %.4f)  %8.2f ms  (encode %.2f  decode %.2f)\n",
               name.c_str(),
               size_str(data.size()).c_str(),
               size_str(result.compressed.size()).c_str(),
               result.ratio,
               total_ms,
               result.encode_ms,
               result.decode_ms);
    };

    printf("input:  %s  (%s, %llu bytes)\n\n", input.c_str(),
           size_str(data.size()).c_str(), (unsigned long long)data.size());

    if (use_huf) run("huffman", [&](auto& d) { return huffman::compress(d); });
    if (use_lzw) run("lzw",     [&](auto& d) { return lzw::compress(d); });

    return 0;
}

static int cmd_decompress(const std::string& path) {
    std::vector<uint8_t> compressed;
    if (!read_file(path, compressed)) return 1;

    if (compressed.size() < 4 || std::memcmp(compressed.data(), "CMP1", 4) != 0) {
        fprintf(stderr, "error: not a valid compressed file\n");
        return 1;
    }

    uint8_t algo = compressed[4];
    uint64_t orig_size;
    std::memcpy(&orig_size, compressed.data() + 6, 8);

    std::vector<uint8_t> result;
    auto t0 = std::chrono::high_resolution_clock::now();
    if (algo == 1) result = huffman::decompress(compressed);
    else if (algo == 2) result = lzw::decompress(compressed);
    else { fprintf(stderr, "error: unknown algorithm %d\n", algo); return 1; }
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // Derive output path: strip .huf.cmp, .lzw.cmp, .cmp
    std::string out_path = path;
    for (auto ext : {".huffman.cmp", ".lzw.cmp", ".huf.cmp", ".cmp"}) {
        auto pos = out_path.rfind(ext);
        if (pos != std::string::npos && pos + strlen(ext) == out_path.size()) {
            out_path = out_path.substr(0, pos);
            break;
        }
    }
    // Fallback: append .out
    if (out_path == path)
        out_path += ".out";

    write_file(out_path, result);

    printf("decompressed: %s → %s  (%.2f ms)\n", path.c_str(),
           size_str(result.size()).c_str(), ms);
    printf("original size: %s\n", size_str(orig_size).c_str());
    printf("match: %s\n", (result.size() == orig_size) ? "yes" : "no");

    return 0;
}

static int cmd_benchmark(const std::string& input) {
    std::vector<uint8_t> data;
    if (!read_file(input, data)) return 1;

    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║                    Compression Benchmark                         ║\n");
    printf("╚══════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("  file:     %s\n", input.c_str());
    printf("  size:     %s (%llu bytes)\n", size_str(data.size()).c_str(),
           (unsigned long long)data.size());
    printf("\n");

    // Run Huffman
    auto t0 = std::chrono::high_resolution_clock::now();
    auto huf = huffman::compress(data);
    auto t1 = std::chrono::high_resolution_clock::now();
    double huf_total = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // Run LZW
    auto t2 = std::chrono::high_resolution_clock::now();
    auto lz = lzw::compress(data);
    auto t3 = std::chrono::high_resolution_clock::now();
    double lzw_total = std::chrono::duration<double, std::milli>(t3 - t2).count();

    printf("  %-10s  %12s  %10s  %10s  %10s  %10s  %s\n",
           "Algo", "Compressed", "Ratio", "Encode ms", "Decode ms", "Total ms", "Speed");
    printf("  %s\n", std::string(90, '-').c_str());
    printf("  %-10s  %12s  %9.4f  %10.2f  %10.2f  %10.2f  %s\n",
           "Huffman",
           size_str(huf.compressed.size()).c_str(),
           huf.ratio,
           huf.encode_ms,
           huf.decode_ms,
           huf_total,
           (huf_total > 0 ? (std::to_string((int)(data.size() / huf_total * 1000 / 1024 / 1024)) + " MB/s").c_str() : ""));
    printf("  %-10s  %12s  %9.4f  %10.2f  %10.2f  %10.2f  %s\n",
           "LZW",
           size_str(lz.compressed.size()).c_str(),
           lz.ratio,
           lz.encode_ms,
           lz.decode_ms,
           lzw_total,
           (lzw_total > 0 ? (std::to_string((int)(data.size() / lzw_total * 1000 / 1024 / 1024)) + " MB/s").c_str() : ""));

    return 0;
}

static void print_usage(const char* prog) {
    printf("Usage:\n");
    printf("  %s <file>                        Compress (auto-select best algorithm)\n", prog);
    printf("  %s -d <file.cmp>                 Decompress\n", prog);
    printf("  %s -b <file>                     Benchmark all algorithms\n", prog);
    printf("  %s -a <algo> <file>              Compress with specific algorithm\n", prog);
    printf("\n");
    printf("Algorithms: huffman, lzw, auto\n");
    printf("Output: <file>.<algo>.cmp  (compress),  <file>.cmp → <file>  (decompress)\n");
}

int main(int argc, char* argv[]) {
    if (argc < 2) { print_usage(argv[0]); return 0; }

    std::string arg1 = argv[1];

    if (arg1 == "-d" || arg1 == "--decompress") {
        if (argc < 3) { fprintf(stderr, "error: missing file\n"); return 1; }
        return cmd_decompress(argv[2]);
    }

    if (arg1 == "-b" || arg1 == "--benchmark") {
        if (argc < 3) { fprintf(stderr, "error: missing file\n"); return 1; }
        return cmd_benchmark(argv[2]);
    }

    if (arg1 == "-a" || arg1 == "--algorithm") {
        if (argc < 4) { fprintf(stderr, "error: missing algorithm or file\n"); return 1; }
        return cmd_compress(argv[3], argv[2]);
    }

    // Default: compress
    return cmd_compress(argv[1], "auto");
}
