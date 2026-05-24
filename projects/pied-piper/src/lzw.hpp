#pragma once
#include <cstdint>
#include <vector>

namespace lzw {

struct Result {
    std::vector<uint8_t> compressed;
    double ratio;
    double encode_ms;
    double decode_ms;
};

Result compress(const std::vector<uint8_t>& data);
std::vector<uint8_t> decompress(const std::vector<uint8_t>& compressed);

} // namespace lzw
