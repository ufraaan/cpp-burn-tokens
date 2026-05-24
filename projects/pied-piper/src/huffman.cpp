#include "huffman.hpp"
#include "bitio.hpp"
#include <vector>
#include <queue>
#include <array>
#include <algorithm>
#include <cstring>
#include <chrono>
#include <functional>

namespace huffman {

// ─── Huffman tree ──────────────────────────────────────────────────────
struct Node {
    uint8_t sym;
    uint64_t freq;
    Node* left;
    Node* right;

    Node(uint8_t s, uint64_t f) : sym(s), freq(f), left(nullptr), right(nullptr) {}
    Node(Node* l, Node* r) : sym(0), freq(l->freq + r->freq), left(l), right(r) {}
};

struct Compare {
    bool operator()(Node* a, Node* b) { return a->freq > b->freq; }
};

struct Code {
    uint32_t bits;
    int len;
};

static void gen_codes(Node* n, uint32_t bits, int len, std::array<Code, 256>& out) {
    if (!n->left && !n->right) { out[n->sym] = {bits, len}; return; }
    if (n->left)  gen_codes(n->left,  bits << 1,     len + 1, out);
    if (n->right) gen_codes(n->right, (bits << 1) | 1, len + 1, out);
}

static void kill(Node* n) { if (n) { kill(n->left); kill(n->right); delete n; } }

// File format (plain = compressed):
//   [0..3]  "CMP1"
//   [4]     algo = 1
//   [5]     flags: 0 = compressed, 1 = stored (uncompressed)
//   [6..13] original_size (uint64_t LE)
//   [14..269] code_lengths (256 bytes, 0 = unused symbol)
//   [270]   padding_bits
//   [271..] bit-packed Huffman data
//
// Stored format:
//   [0..5]  same
//   [6..13] original_size
//   [14..]  raw data

static constexpr int HEADER = 14;   // bytes before algo-specific data
static constexpr int CL_OFF = 14;   // code lengths start
static constexpr int PAD_OFF = 270; // padding bits
static constexpr int DATA_OFF = 271;// compressed data start

Result compress(const std::vector<uint8_t>& data) {
    auto t0 = std::chrono::high_resolution_clock::now();

    // Frequencies
    std::array<uint64_t, 256> freq{};
    for (uint8_t b : data) ++freq[b];

    // Build tree
    std::priority_queue<Node*, std::vector<Node*>, Compare> pq;
    for (int i = 0; i < 256; ++i)
        if (freq[i] > 0) pq.push(new Node((uint8_t)i, freq[i]));

    if (pq.size() == 1) {
        Node* s = pq.top(); pq.pop();
        pq.push(new Node(s, new Node((uint8_t)0, (uint64_t)0)));
    }
    while (pq.size() > 1) {
        Node* a = pq.top(); pq.pop();
        Node* b = pq.top(); pq.pop();
        pq.push(new Node(a, b));
    }
    Node* root = pq.empty() ? nullptr : pq.top();

    std::array<Code, 256> codes{};
    if (root) gen_codes(root, 0, 0, codes);

    // Build output
    std::vector<uint8_t> out(DATA_OFF, 0);
    std::memcpy(out.data(), "CMP1", 4);
    out[4] = 1; // Huffman
    out[5] = 0; // flags: compressed
    uint64_t sz = data.size();
    std::memcpy(out.data() + 6, &sz, 8);
    for (int i = 0; i < 256; ++i)
        out[CL_OFF + i] = (uint8_t)codes[i].len;

    // Encode
    BitWriter bw(out);
    for (uint8_t b : data) {
        auto& c = codes[b];
        if (c.len > 0) bw.write(c.bits, c.len);
    }
    bw.flush();
    out[PAD_OFF] = (uint8_t)bw.padding_bits();

    auto t2 = std::chrono::high_resolution_clock::now();
    double encode_ms = std::chrono::duration<double, std::milli>(t2 - t0).count();

    // Decode (verify + measure decode time)
    auto t3 = std::chrono::high_resolution_clock::now();
    auto dec = decompress(out);
    auto t4 = std::chrono::high_resolution_clock::now();
    double decode_ms = std::chrono::duration<double, std::milli>(t4 - t3).count();

    kill(root);

    double ratio = data.empty() ? 1.0 : (double)out.size() / (double)data.size();

    // If expansion, store uncompressed instead
    if (out.size() > data.size() + HEADER) {
        out.assign(HEADER, 0);
        std::memcpy(out.data(), "CMP1", 4);
        out[4] = 1;
        out[5] = 1; // stored
        std::memcpy(out.data() + 6, &sz, 8);
        out.insert(out.end(), data.begin(), data.end());
        ratio = (double)out.size() / (double)data.size();
    }

    return {std::move(out), ratio, encode_ms, decode_ms};
}

std::vector<uint8_t> decompress(const std::vector<uint8_t>& in) {
    if (in.size() < HEADER) return {};
    if (std::memcmp(in.data(), "CMP1", 4) != 0 || in[4] != 1) return {};

    uint64_t orig_size;
    std::memcpy(&orig_size, in.data() + 6, 8);

    if (in[5] == 1) {
        // Stored uncompressed
        size_t avail = in.size() - HEADER;
        size_t copy = (size_t)orig_size < avail ? (size_t)orig_size : avail;
        return std::vector<uint8_t>(in.begin() + HEADER, in.begin() + HEADER + copy);
    }

    if (in.size() < DATA_OFF + 1) return {};

    uint8_t pad_bits = in[PAD_OFF];

    // Read code lengths
    std::array<int, 256> lens{};
    int max_len = 0;
    for (int i = 0; i < 256; ++i) {
        lens[i] = in[CL_OFF + i];
        if (lens[i] > max_len) max_len = lens[i];
    }

    // Canonical Huffman: count per length
    std::array<int, 32> bl_count{};
    for (int l : lens)
        if (l > 0) ++bl_count[l];

    // First canonical code per length
    std::array<uint32_t, 32> first_code{};
    uint32_t code = 0;
    for (int i = 1; i <= max_len; ++i) {
        code = (code + bl_count[i - 1]) << 1;
        first_code[i] = code;
    }

    // Assign canonical codes
    std::array<uint32_t, 256> canon{};
    for (int i = 0; i < 256; ++i) {
        if (lens[i] > 0)
            canon[i] = first_code[lens[i]]++;
    }

    // Build decode tree
    struct DNode { int sym = -1; DNode* ch[2] = {}; };
    DNode* root = new DNode();
    for (int i = 0; i < 256; ++i) {
        if (lens[i] == 0) continue;
        DNode* n = root;
        for (int b = lens[i] - 1; b >= 0; --b) {
            int bit = (canon[i] >> b) & 1;
            if (!n->ch[bit]) n->ch[bit] = new DNode();
            n = n->ch[bit];
        }
        n->sym = i;
    }

    // Decode
    std::vector<uint8_t> result;
    result.reserve((size_t)orig_size);

    const uint8_t* src = in.data() + DATA_OFF;
    size_t src_size = in.size() - DATA_OFF;
    BitReader br(src, src_size);
    int total_bits = (int)(src_size * 8) - pad_bits;

    int bits_read = 0;
    DNode* node = root;
    while (bits_read < total_bits && result.size() < orig_size) {
        uint32_t bit = br.read(1);
        ++bits_read;
        if (!node->ch[bit]) break;
        node = node->ch[bit];
        if (node->sym >= 0) {
            result.push_back((uint8_t)node->sym);
            node = root;
        }
    }

    // Cleanup
    std::function<void(DNode*)> cleanup = [&](DNode* n) {
        if (!n) return;
        cleanup(n->ch[0]);
        cleanup(n->ch[1]);
        delete n;
    };
    cleanup(root);

    return result;
}

} // namespace huffman
