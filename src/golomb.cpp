#include "golomb.hpp"
#include <stdexcept>
#include <vector>
#include <limits>
#include <iostream>

// ---------------- BitWriter implementation ----------------
void BitWriter::writeBit(bool b) {
    bits.push_back(b ? '1' : '0');
}

void BitWriter::writeBits(uint64_t value, int count) {
    for (int i = count - 1; i >= 0; --i) {
        writeBit(((value >> i) & 1ULL) != 0ULL);
    }
}

// ---------------- BitReader implementation ----------------
BitReader::BitReader(const std::string &bstr) : bits(bstr), pos(0) {}

bool BitReader::hasMore() const { return pos < bits.size(); }

bool BitReader::readBit() {
    if (pos >= bits.size()) throw std::runtime_error("BitReader: out of bits");
    return bits[pos++] == '1';
}

uint64_t BitReader::readBits(int count) {
    if (count == 0) return 0;
    uint64_t v = 0;
    for (int i = 0; i < count; ++i) {
        v = (v << 1) | (readBit() ? 1ULL : 0ULL);
    }
    return v;
}

// ---------------- Golomb implementation ----------------
Golomb::Golomb(uint64_t m_, NegativeMode negMode_) : m(m_), negMode(negMode_) {
    if (m == 0) throw std::invalid_argument("m must be >= 1");
    b = 0;
    while ((1ULL << b) < m) ++b;
    if (m == 1) {
        cutoff = 0;
    } else {
        cutoff = (1ULL << b) - m;
    }
}

std::string Golomb::encode(int64_t value) const {
    BitWriter w;
    if (value < 0) {
        if (negMode == NegativeMode::SIGN_MAGNITUDE) {
            // sign bit: 1 = negative
            w.writeBit(true);
            encodeUnsigned((uint64_t)(-value), w);
        } else {
            uint64_t z = toZigZag(value);
            encodeUnsigned(z, w);
        }
    } else {
        if (negMode == NegativeMode::SIGN_MAGNITUDE) {
            w.writeBit(false);
            encodeUnsigned((uint64_t)value, w);
        } else {
            uint64_t z = toZigZag(value);
            encodeUnsigned(z, w);
        }
    }
    return w.bits;
}

std::pair<int64_t, size_t> Golomb::decode(const std::string &bits) const {
    BitReader r(bits);
    int64_t result;
    if (negMode == NegativeMode::SIGN_MAGNITUDE) {
        if (!r.hasMore()) throw std::runtime_error("decode: no bits for sign");
        bool sign = r.readBit();
        uint64_t mag = decodeUnsigned(r);
        result = sign ? -(int64_t)mag : (int64_t)mag;
    } else {
        uint64_t z = decodeUnsigned(r);
        result = fromZigZag(z);
    }
    return {result, r.pos};
}

std::string Golomb::encodeUnsignedToString(uint64_t value) const {
    BitWriter w;
    encodeUnsigned(value, w);
    return w.bits;
}

std::pair<uint64_t, size_t> Golomb::decodeUnsignedFromString(const std::string &bits) const {
    BitReader r(bits);
    uint64_t v = decodeUnsigned(r);
    return {v, r.pos};
}

uint64_t Golomb::toZigZag(int64_t x) {
    if (x >= 0) return (uint64_t)x << 1;
    return (uint64_t)((-x << 1) - 1);
}

int64_t Golomb::fromZigZag(uint64_t z) {
    if ((z & 1ULL) == 0ULL) return (int64_t)(z >> 1);
    return -(int64_t)((z + 1) >> 1);
}

void Golomb::encodeUnsigned(uint64_t n, BitWriter &w) const {
    if (m == 1) {
        // unary: n zeros then a 1
        for (uint64_t i = 0; i < n; ++i) w.writeBit(false);
        w.writeBit(true);
        return;
    }
    uint64_t q = n / m;
    uint64_t r = n % m;
    for (uint64_t i = 0; i < q; ++i) w.writeBit(false);
    w.writeBit(true);
    if (r < cutoff) {
        w.writeBits(r, static_cast<int>(b - 1));
    } else {
        w.writeBits(r + cutoff, static_cast<int>(b));
    }
}

uint64_t Golomb::decodeUnsigned(BitReader &r) const {
    if (m == 1) {
        uint64_t q = 0;
        while (true) {
            if (!r.hasMore()) throw std::runtime_error("decodeUnsigned: unexpected end (m==1)");
            if (r.readBit()) break;
            ++q;
        }
        return q;
    }
    uint64_t q = 0;
    while (true) {
        if (!r.hasMore()) throw std::runtime_error("decodeUnsigned: unexpected end reading unary");
        if (r.readBit()) break;
        ++q;
    }
    if (b == 0) return q * m;
    uint64_t x = r.readBits(static_cast<int>(b - 1));
    uint64_t r_value;
    if (x < cutoff) {
        r_value = x;
    } else {
        uint64_t nextbit = r.readBit() ? 1ULL : 0ULL;
        uint64_t combined = (x << 1) | nextbit;
        r_value = combined - cutoff;
    }
    return q * m + r_value;
}  