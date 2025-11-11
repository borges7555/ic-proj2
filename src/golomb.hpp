#ifndef GOLOMB_HPP
#define GOLOMB_HPP

#include <cstdint>
#include <string>
#include <utility>


enum class NegativeMode {
    SIGN_MAGNITUDE,
    INTERLEAVED
};

// BitWriter that accumulates bits into a string of '0'/'1'.
class BitWriter {
public:
    std::string bits; // '0' / '1'
    void writeBit(bool b);
    void writeBits(uint64_t value, int count);
};

// BitReader that reads from a string of '0'/'1'.
class BitReader {
public:
    const std::string &bits;
    size_t pos;
    BitReader(const std::string &bstr);
    bool hasMore() const;
    bool readBit();
    uint64_t readBits(int count);
};

// Golomb coder class
class Golomb {
public:
    // construct with parameter m (m >= 1) and a negative-number handling mode
    explicit Golomb(uint64_t m_, NegativeMode negMode_ = NegativeMode::INTERLEAVED);

    // Encode a signed integer -> returns a string of '0'/'1'
    std::string encode(int64_t value) const;

    // Decode a signed integer from a bit-string (starting at position 0).
    // Returns pair(value, bits_consumed).
    std::pair<int64_t, size_t> decode(const std::string &bits) const;

    // Convenience helpers for unsigned values
    std::string encodeUnsignedToString(uint64_t value) const;
    std::pair<uint64_t, size_t> decodeUnsignedFromString(const std::string &bits) const;

private:
    uint64_t m;
    uint64_t b;       // ceil(log2(m))
    uint64_t cutoff;  // (1<<b) - m
    NegativeMode negMode;

    static uint64_t toZigZag(int64_t x);
    static int64_t fromZigZag(uint64_t z);

    void encodeUnsigned(uint64_t n, BitWriter &w) const;
    uint64_t decodeUnsigned(BitReader &r) const;
};

#endif