#include <bits/stdc++.h>
using namespace std;

#include "golomb.hpp"

// keeping memory offsets continuous
#pragma pack(push,1)
struct WAVHeader {
    char riff[4];
    uint32_t overall_size;
    char wave[4];
    char fmt_chunk_marker[4];
    uint32_t length_of_fmt;
    uint16_t format_type;
    uint16_t channels;
    uint32_t sample_rate;
    uint32_t byterate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char data_chunk_header[4];
    uint32_t data_size;
};
#pragma pack(pop)

#pragma pack(push,1)
struct GBLHeader {
    char magic[4]; // "GBL1"
    uint16_t channels;
    uint32_t sample_rate;
    uint32_t num_frames;
    uint16_t bits_per_sample;
    uint8_t neg_mode;
};
#pragma pack(pop)

bool readWav(const string &filename, WAVHeader &hdr, vector<int16_t> &samples) {
    ifstream f(filename, ios::binary);
    if (!f) return false;
    f.read(reinterpret_cast<char*>(&hdr), sizeof(WAVHeader));
    if (!f) return false;
    if (strncmp(hdr.riff, "RIFF", 4) != 0 || strncmp(hdr.wave, "WAVE", 4) != 0) {
        cerr << "readWav: not a RIFF/WAVE file\n";
        return false;
    }
    if (hdr.format_type != 1 || hdr.bits_per_sample != 16) {
        cerr << "readWav: only PCM16 supported\n";
        return false;
    }
    size_t totalSamples = hdr.data_size / (hdr.bits_per_sample / 8);
    samples.resize(totalSamples);
    f.read(reinterpret_cast<char*>(samples.data()), hdr.data_size);
    return true;
}

bool writeWav(const string &filename, const WAVHeader &tmpl, const vector<int16_t> &samples) {
    ofstream f(filename, ios::binary);
    if (!f) return false;
    WAVHeader hdr = tmpl;
    hdr.data_size = static_cast<uint32_t>(samples.size() * sizeof(int16_t));
    hdr.overall_size = 36 + hdr.data_size;
    f.write(reinterpret_cast<const char*>(&hdr), sizeof(WAVHeader));
    f.write(reinterpret_cast<const char*>(samples.data()), hdr.data_size);
    return true;
}

void packBitsToBytes(const string &bits, vector<uint8_t> &outBytes) {
    size_t nbits = bits.size();
    size_t nbytes = (nbits + 7) / 8;
    outBytes.assign(nbytes, 0);
    for (size_t i = 0; i < nbits; ++i) {
        if (bits[i] == '1') outBytes[i/8] |= (1u << (7 - (i%8)));
    }
}

string unpackBytesToBits(const vector<uint8_t> &bytes, size_t nbits) {
    string bits;
    bits.reserve(nbits);
    for (size_t i = 0; i < nbits; ++i) {
        uint8_t b = bytes[i/8];
        bool bit = ((b >> (7 - (i%8))) & 1u) != 0;
        bits.push_back(bit ? '1' : '0');
    }
    return bits;
}

// compute m from EMA of absolute residuals.
static uint64_t choose_m_from_ema(double ema) {
    double r = floor(ema + 0.5);
    uint64_t m = (uint64_t)max<double>(1.0, r);
    return m;
}

string encodeSamples(const vector<int16_t> &samples, int channels) {
    string bits;
    bits.reserve(samples.size()*4);

    double emaL = 1.0, emaR = 1.0;
    const double alpha = 0.01;
    int64_t prevL = 0;
    bool first = true;
    size_t frames = samples.size() / channels;

    for (size_t i = 0; i < frames; ++i) {
        // left channel
        int16_t L = samples[i*channels + 0];
        int16_t R = (channels==2) ? samples[i*channels + 1] : 0;

        int64_t predL = first ? 0 : prevL;
        int64_t resL = int64_t(L) - predL;

        uint64_t mL = choose_m_from_ema(emaL);
        Golomb gL(mL, NegativeMode::INTERLEAVED);
        string encL = gL.encode(resL);
        bits += encL;
        emaL = (1.0 - alpha) * emaL + alpha * std::abs((double)resL);

        if (channels == 2) {
            // right channel
            int64_t predR = L;
            int64_t resR = int64_t(R) - predR;
            uint64_t mR = choose_m_from_ema(emaR);
            Golomb gR(mR, NegativeMode::INTERLEAVED);
            string encR = gR.encode(resR);
            bits += encR;
            emaR = (1.0 - alpha) * emaR + alpha * std::abs((double)resR);
        }

        prevL = L;
        first = false;
    }
    return bits;
}

vector<int16_t> decodeSamples(const string &bits, int channels, size_t frames) {
    vector<int16_t> out;
    out.reserve(frames * channels);

    double emaL = 1.0, emaR = 1.0;
    const double alpha = 0.01;
    int64_t prevL = 0;
    bool first = true;
    size_t bitpos = 0;
    size_t totalBits = bits.size();

    for (size_t i = 0; i < frames; ++i) {
        // left channel
        uint64_t mL = choose_m_from_ema(emaL);
        Golomb gL(mL, NegativeMode::INTERLEAVED);

        if (bitpos >= totalBits) throw runtime_error("decode: bitstream exhausted while decoding left");
        string_view tail(bits.c_str() + bitpos, totalBits - bitpos);
        string tailstr(tail);
        auto decL = gL.decode(tailstr);
        int64_t resL = decL.first;
        size_t consumedL = decL.second;
        if (consumedL == 0) throw runtime_error("decode: Golomb reported 0 bits consumed for left");
        bitpos += consumedL;

        int64_t predL = first ? 0 : prevL;
        int64_t L = predL + resL;
        L = std::clamp(L, int64_t(-32768), int64_t(32767));
        out.push_back(int16_t(L));
        emaL = (1.0 - alpha) * emaL + alpha * std::abs((double)resL);

        // right channel (if stereo)
        if (channels == 2) {
            uint64_t mR = choose_m_from_ema(emaR);
            Golomb gR(mR, NegativeMode::INTERLEAVED);

            if (bitpos >= totalBits) throw runtime_error("decode: bitstream exhausted while decoding right");
            string tailstr2(bits.c_str() + bitpos, totalBits - bitpos);
            auto decR = gR.decode(tailstr2);
            int64_t resR = decR.first;
            size_t consumedR = decR.second;
            if (consumedR == 0) throw runtime_error("decode: Golomb reported 0 bits consumed for right");
            bitpos += consumedR;

            int64_t R = L + resR;
            R = std::clamp(R, int64_t(-32768), int64_t(32767));
            out.push_back(int16_t(R));
            emaR = (1.0 - alpha) * emaR + alpha * std::abs((double)resR);
        }

        prevL = out.back() - (channels == 2 ? out[out.size() - 1] - out[out.size() - 2] : 0);
        first = false;
    }

    return out;
}


void writeCompressedFile(const string &filename, const WAVHeader &wavhdr, const string &bitstring, uint16_t channels) {
    ofstream f(filename, ios::binary);
    if (!f) throw runtime_error("Cannot open output file for writing");
    GBLHeader gh;
    memcpy(gh.magic, "GBL1", 4);
    gh.channels = channels;
    gh.sample_rate = wavhdr.sample_rate;
    gh.num_frames = wavhdr.data_size / wavhdr.block_align;
    gh.bits_per_sample = wavhdr.bits_per_sample;
    gh.neg_mode = static_cast<uint8_t>(NegativeMode::INTERLEAVED);

    f.write(reinterpret_cast<const char*>(&gh), sizeof(GBLHeader));
    uint32_t nbits = static_cast<uint32_t>(bitstring.size());
    f.write(reinterpret_cast<const char*>(&nbits), sizeof(uint32_t));
    vector<uint8_t> bytes;
    packBitsToBytes(bitstring, bytes);
    f.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

bool readCompressedFile(const string &filename, GBLHeader &gh, string &bitstring) {
    ifstream f(filename, ios::binary);
    if (!f) return false;
    f.read(reinterpret_cast<char*>(&gh), sizeof(GBLHeader));
    if (!f) return false;
    uint32_t nbits = 0;
    f.read(reinterpret_cast<char*>(&nbits), sizeof(uint32_t));
    uint32_t nbytes = (nbits + 7) / 8;
    vector<uint8_t> bytes(nbytes);
    f.read(reinterpret_cast<char*>(bytes.data()), nbytes);
    bitstring = unpackBytesToBits(bytes, nbits);
    return true;
}

int main(int argc, char **argv) {
    if (argc < 4) {
        cerr << "Usage:\n  Encode: " << argv[0] << " encode in.wav out.gbl\n  Decode: " << argv[0] << " decode in.gbl out.wav\n";
        return 1;
    }

    string mode = argv[1];
    if (mode == "encode") {
        string inwav = argv[2], outg = argv[3];
        WAVHeader wh;
        vector<int16_t> samples;
        if (!readWav(inwav, wh, samples)) {
            cerr << "Failed to read WAV: " << inwav << "\n";
            return 2;
        }
        int channels = wh.channels;
        string bits = encodeSamples(samples, channels);
        writeCompressedFile(outg, wh, bits, channels);
        cerr << "Encoded: bits=" << bits.size() << " frames=" << (wh.data_size / wh.block_align) << "\n";
        return 0;
    } else if (mode == "decode") {
        string ing = argv[2], outwav = argv[3];
        GBLHeader gh;
        string bits;
        if (!readCompressedFile(ing, gh, bits)) {
            cerr << "Failed to read compressed file: " << ing << "\n";
            return 3;
        }
        int channels = gh.channels;
        size_t frames = gh.num_frames;
        vector<int16_t> samples = decodeSamples(bits, channels, frames);

        WAVHeader wh = {};
        memcpy(wh.riff, "RIFF", 4);
        memcpy(wh.wave, "WAVE", 4);
        memcpy(wh.fmt_chunk_marker, "fmt ", 4);
        wh.length_of_fmt = 16;
        wh.format_type = 1;
        wh.channels = channels;
        wh.sample_rate = gh.sample_rate;
        wh.bits_per_sample = gh.bits_per_sample;
        wh.block_align = (wh.channels * wh.bits_per_sample) / 8;
        wh.byterate = wh.sample_rate * wh.block_align;
        memcpy(wh.data_chunk_header, "data", 4);
        wh.data_size = static_cast<uint32_t>(samples.size() * sizeof(int16_t));
        wh.overall_size = 36 + wh.data_size;

        if (!writeWav(outwav, wh, samples)) {
            cerr << "Failed to write WAV: " << outwav << "\n";
            return 4;
        }
        cerr << "Decoded: frames=" << frames << " samples=" << samples.size() << "\n";
        return 0;
    } else {
        cerr << "Unknown mode: " << mode << "\n";
        return 1;
    }
}
