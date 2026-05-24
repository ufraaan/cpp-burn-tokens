#pragma once
#include <cstdint>
#include <vector>
#include <cassert>

class BitWriter {
public:
    explicit BitWriter(std::vector<uint8_t>& buffer)
        : buf_(&buffer), byte_(0), bit_pos_(0) {}

    void write(uint32_t value, int nbits) {
        assert(nbits > 0 && nbits <= 32);
        for (int i = nbits - 1; i >= 0; --i) {
            if (value & (1u << i))
                byte_ |= (1u << (7 - bit_pos_));
            ++bit_pos_;
            if (bit_pos_ == 8) {
                buf_->push_back(byte_);
                byte_ = 0;
                bit_pos_ = 0;
            }
        }
    }

    void flush() {
        if (bit_pos_ > 0) {
            buf_->push_back(byte_);
            byte_ = 0;
            bit_pos_ = 0;
        }
    }

    int padding_bits() const {
        return bit_pos_ == 0 ? 0 : 8 - bit_pos_;
    }

private:
    std::vector<uint8_t>* buf_;
    uint8_t byte_;
    int bit_pos_;
};

class BitReader {
public:
    explicit BitReader(const uint8_t* data, size_t size)
        : data_(data), size_(size), byte_pos_(0), bit_pos_(0) {}

    uint32_t read(int nbits) {
        assert(nbits > 0 && nbits <= 32);
        uint32_t value = 0;
        for (int i = nbits - 1; i >= 0; --i) {
            if (byte_pos_ >= size_) {
                // Assume 0 bits past end (padding)
                return value;
            }
            if (data_[byte_pos_] & (1u << (7 - bit_pos_)))
                value |= (1u << i);
            ++bit_pos_;
            if (bit_pos_ == 8) {
                ++byte_pos_;
                bit_pos_ = 0;
            }
        }
        return value;
    }

    bool has_more() const {
        return byte_pos_ < size_;
    }

    size_t bytes_consumed() const {
        return byte_pos_ + (bit_pos_ > 0 ? 1 : 0);
    }

private:
    const uint8_t* data_;
    size_t size_;
    size_t byte_pos_;
    int bit_pos_;
};
