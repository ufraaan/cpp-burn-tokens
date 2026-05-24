#include "lzw.hpp"
#include "bitio.hpp"
#include <vector>
#include <array>
#include <cstring>
#include <chrono>
#include <algorithm>

namespace lzw {

// LZW parameters
static constexpr int MAX_CODES = 4096;
static constexpr int CLEAR_CODE = 256;
static constexpr int EOI_CODE   = 257;

// File format:
//   [0..3]  "CMP1"
//   [4]     algo = 2
//   [5]     flags: 0 = compressed, 1 = stored
//   [6..13] original_size (uint64_t LE)
//   [14]    max_bit_width (usually 12)
//   [15..]  bit-packed LZW codes (variable 9..max_bit_width)

static constexpr int HEADER = 15;

// bit_width_for was removed; variable width is tracked inline during encoding

Result compress(const std::vector<uint8_t>& data) {
    auto t0 = std::chrono::high_resolution_clock::now();

    std::vector<uint8_t> out(HEADER, 0);
    std::memcpy(out.data(), "CMP1", 4);
    out[4] = 2; // LZW algo
    out[5] = 0; // compressed
    uint64_t sz = data.size();
    std::memcpy(out.data() + 6, &sz, 8);
    int max_bw = 12;
    out[14] = (uint8_t)max_bw;

    // Dictionary: dict[prefix][char] = code, -1 = unset
    // Use a flat array: index = (prefix << 8) | char
    std::vector<int> dict(MAX_CODES * 256, -1);

    // Initialize with single-byte entries
    for (int i = 0; i < 256; ++i)
        dict[i] = i;

    BitWriter bw(out);
    int next_code = 258;
    int cur_bw = 9;

    int prefix = data.empty() ? -1 : data[0];
    size_t i = 1;

    while (i < data.size()) {
        uint8_t c = data[i++];
        int key = (prefix << 8) | c;
        if (dict[key] >= 0) {
            prefix = dict[key];
        } else {
            bw.write(prefix, cur_bw);
            dict[key] = next_code;

            if (next_code >= (1 << cur_bw) && cur_bw < max_bw)
                ++cur_bw;

            ++next_code;

            if (next_code >= MAX_CODES) {
                bw.write(CLEAR_CODE, cur_bw);
                // Reset dictionary
                std::fill(dict.begin(), dict.end(), -1);
                for (int j = 0; j < 256; ++j)
                    dict[j] = j;
                next_code = 258;
                cur_bw = 9;
            }

            prefix = c;
        }
    }

    if (data.size() > 0)
        bw.write(prefix, cur_bw);
    bw.write(EOI_CODE, cur_bw);
    bw.flush();

    auto t1 = std::chrono::high_resolution_clock::now();
    double encode_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // Decode for verification + timing
    auto t2 = std::chrono::high_resolution_clock::now();
    auto dec = decompress(out);
    auto t3 = std::chrono::high_resolution_clock::now();
    double decode_ms = std::chrono::duration<double, std::milli>(t3 - t2).count();

    double ratio = data.empty() ? 1.0 : (double)out.size() / (double)data.size();

    // If expansion, store uncompressed
    if (out.size() > data.size() + 14) {
        out.assign(14, 0);
        std::memcpy(out.data(), "CMP1", 4);
        out[4] = 2;
        out[5] = 1; // stored
        std::memcpy(out.data() + 6, &sz, 8);
        out.insert(out.end(), data.begin(), data.end());
        ratio = (double)out.size() / (double)data.size();
    }

    return {std::move(out), ratio, encode_ms, decode_ms};
}

std::vector<uint8_t> decompress(const std::vector<uint8_t>& in) {
    if (in.size() < 15) return {};
    if (std::memcmp(in.data(), "CMP1", 4) != 0 || in[4] != 2) return {};

    uint64_t orig_size;
    std::memcpy(&orig_size, in.data() + 6, 8);

    if (in[5] == 1) {
        // Stored
        size_t avail = in.size() - 14;
        size_t copy = (size_t)orig_size < avail ? (size_t)orig_size : avail;
        return std::vector<uint8_t>(in.begin() + 14, in.begin() + 14 + copy);
    }

    int max_bw = in[14];
    if (max_bw < 9 || max_bw > 16) max_bw = 12;

    // Decoder dictionary: each entry = (prefix, char)
    // Entry 0..255: single byte (char = byte, prefix = -1)
    // Entry 258..4095: compound
    struct Entry { int prefix; uint8_t c; };
    std::array<Entry, MAX_CODES> dict{};
    for (int i = 0; i < 256; ++i)
        dict[i] = {-1, (uint8_t)i};

    std::vector<uint8_t> result;
    result.reserve((size_t)orig_size);

    const uint8_t* src = in.data() + HEADER;
    size_t src_size = in.size() - HEADER;
    BitReader br(src, src_size);

    int next_code = 258;
    int cur_bw = 9;

    // Read first code
    int old_code = (int)br.read(cur_bw);
    if (old_code == CLEAR_CODE || old_code == EOI_CODE || old_code < 0)
        return result;

    // Output first string
    // Build string for old_code
    std::vector<uint8_t> first_str;
    {
        int c = old_code;
        while (c >= 0) {
            first_str.push_back(dict[c].c);
            c = dict[c].prefix;
        }
        std::reverse(first_str.begin(), first_str.end());
    }
    result.insert(result.end(), first_str.begin(), first_str.end());

    // Main decode loop
    while (result.size() < orig_size && br.has_more()) {
        if (next_code >= (1 << cur_bw) && cur_bw < max_bw)
            ++cur_bw;

        int code = (int)br.read(cur_bw);
        if (code < 0) break;

        if (code == EOI_CODE) break;

        if (code == CLEAR_CODE) {
            next_code = 258;
            cur_bw = 9;
            // Read next code
            old_code = (int)br.read(cur_bw);
            if (old_code == EOI_CODE || old_code < 0) break;

            // Output string for old_code
            std::vector<uint8_t> str;
            {
                int c = old_code;
                while (c >= 0) {
                    str.push_back(dict[c].c);
                    c = dict[c].prefix;
                }
                std::reverse(str.begin(), str.end());
            }
            result.insert(result.end(), str.begin(), str.end());
            continue;
        }

        // Determine the string for this code
        std::vector<uint8_t> entry_str;
        uint8_t first_char;

        if (code < next_code) {
            // Code exists in dictionary
            {
                int c = code;
                while (c >= 0) {
                    entry_str.push_back(dict[c].c);
                    c = dict[c].prefix;
                }
                std::reverse(entry_str.begin(), entry_str.end());
            }
            first_char = entry_str[0];
        } else if (code == next_code) {
            // Special case: code equals next_code (Kd-tree case)
            // The string is old_string + first_char(old_string)
            {
                int c = old_code;
                while (c >= 0) {
                    entry_str.push_back(dict[c].c);
                    c = dict[c].prefix;
                }
                std::reverse(entry_str.begin(), entry_str.end());
            }
            first_char = entry_str[0];
            entry_str.push_back(first_char);
        } else {
            break; // Invalid code
        }

        // Add new entry to dictionary
        if (next_code < MAX_CODES) {
            dict[next_code].prefix = old_code;
            dict[next_code].c = first_char;
            ++next_code;
        }

        // Output
        result.insert(result.end(), entry_str.begin(), entry_str.end());
        old_code = code;
    }

    return result;
}

} // namespace lzw
