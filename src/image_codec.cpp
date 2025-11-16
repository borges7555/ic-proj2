// image_codec.cpp
// Lossless grayscale image codec using Golomb coding of prediction residuals.
// Usage:
//  Encode: ./build/image_codec encode <input_gray_image> <output.gimg> [predictor]
//  Decode: ./build/image_codec decode <input.gimg> <output_image>
// predictor: 0=left, 1=median (JPEG-LS style). Default: 1

#include <opencv2/opencv.hpp>
#include "golomb.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <algorithm>

using namespace std;

static inline uint32_t read_u32(ifstream &f) { uint32_t v; f.read(reinterpret_cast<char*>(&v),4); return v; }
static inline uint64_t read_u64(ifstream &f) { uint64_t v; f.read(reinterpret_cast<char*>(&v),8); return v; }
static inline void write_u32(ofstream &f, uint32_t v) { f.write(reinterpret_cast<const char*>(&v),4); }
static inline void write_u64(ofstream &f, uint64_t v) { f.write(reinterpret_cast<const char*>(&v),8); }

static string pack_bits_to_bytes(const string &bits) {
    size_t n = bits.size();
    size_t nbytes = (n + 7) / 8;
    string out; out.resize(nbytes);
    for (size_t i = 0; i < nbytes; ++i) out[i] = 0;
    for (size_t i = 0; i < n; ++i) {
        if (bits[i] == '1') out[i/8] |= (1 << (7 - (i%8)));
    }
    return out;
}

static string unpack_bytes_to_bits(const string &bytes, uint64_t bits_len) {
    string out; out.reserve(bits_len);
    for (uint64_t i = 0; i < bits_len; ++i) {
        size_t b = i/8; int off = 7 - (i%8);
        bool bit = (bytes[b] >> off) & 1;
        out.push_back(bit ? '1' : '0');
    }
    return out;
}

int main(int argc, char **argv) {
    if (argc < 2) { cerr << "Usage: encode/decode ...\n"; return 1; }
    string mode = argv[1];
    if (mode == "encode") {
        if (argc < 4) { cerr << "Usage: encode <in_gray> <out.gimg> [predictor]\n"; return 1; }
        string inpath = argv[2];
        string outpath = argv[3];
        int predictor = 1;
        if (argc >= 5) predictor = atoi(argv[4]);

        cv::Mat img = cv::imread(inpath, cv::IMREAD_UNCHANGED);
        if (img.empty()) { cerr << "Failed to read input: "<<inpath<<"\n"; return 1; }
        if (img.channels() != 1) {
            cerr << "Input must be grayscale (single channel)\n"; return 1;
        }
        int h = img.rows; int w = img.cols;

        // compute residuals using predictor
        vector<int> residuals; residuals.reserve(h*w);
        for (int r=0;r<h;++r) {
            for (int c=0;c<w;++c) {
                int cur = img.ptr<uint8_t>(r)[c];
                int pred = 0;
                if (predictor == 0) {
                    pred = (c==0?0:img.ptr<uint8_t>(r)[c-1]);
                } else {
                    int left = (c==0?0:img.ptr<uint8_t>(r)[c-1]);
                    int top = (r==0?0:img.ptr<uint8_t>(r-1)[c]);
                    int topleft = (r==0||c==0?0:img.ptr<uint8_t>(r-1)[c-1]);
                    int p = left + top - topleft;
                    int a = left, b = top, cval = p;
                    int mx = max(a,max(b,cval)); int mn = min(a,min(b,cval));
                    if (a!=mx && a!=mn) pred=a; else if (b!=mx && b!=mn) pred=b; else pred=cval;
                }
                int res = cur - pred;
                residuals.push_back(res);
            }
        }

        // pick best m by trying candidates
        size_t best_len = SIZE_MAX; uint32_t best_m = 1;
        string best_bits;
        for (uint32_t m=1;m<=64;m*=2) {
            Golomb g(m, NegativeMode::INTERLEAVED);
            string bits;
            bits.reserve(residuals.size()*4);
            for (int v: residuals) {
                bits += g.encode(v);
            }
            if (bits.size() < best_len) { best_len = bits.size(); best_m = m; best_bits = std::move(bits); }
        }
        for (uint32_t m=3;m<=32;m+=2) {
            Golomb g(m, NegativeMode::INTERLEAVED);
            string bits;
            bits.reserve(residuals.size()*4);
            for (int v: residuals) bits += g.encode(v);
            if (bits.size() < best_len) { best_len = bits.size(); best_m = m; best_bits = std::move(bits); }
        }

        cerr << "Chosen m="<<best_m<<" bits="<<best_bits.size()<<"\n";

        // pack bits to bytes
        string packed = pack_bits_to_bytes(best_bits);

        // write header and data
        ofstream ofs(outpath, ios::binary);
        if (!ofs) { cerr << "Failed to open output file"<<outpath<<"\n"; return 1; }
        ofs.write("GIMG",4);
        write_u32(ofs, (uint32_t)w);
        write_u32(ofs, (uint32_t)h);
        uint8_t pred8 = (uint8_t)predictor; ofs.write(reinterpret_cast<char*>(&pred8),1);
        write_u32(ofs, best_m);
        write_u64(ofs, (uint64_t)best_bits.size());
        ofs.write(packed.data(), packed.size());
        ofs.close();
        cerr<<"Wrote encoded file: "<<outpath<<"\n";
        return 0;

    } else if (mode == "decode") {
        if (argc < 4) { cerr << "Usage: decode <in.gimg> <out_image>\n"; return 1; }
        string inpath = argv[2]; string outpath = argv[3];
        ifstream ifs(inpath, ios::binary);
        if (!ifs) { cerr<<"Failed to open "<<inpath<<"\n"; return 1; }
        char magic[4]; ifs.read(magic,4); if (ifs.gcount()!=4 || string(magic,4)!="GIMG") { cerr<<"Not a GIMG file\n"; return 1; }
        uint32_t w = read_u32(ifs); uint32_t h = read_u32(ifs);
        uint8_t pred8=0; ifs.read(reinterpret_cast<char*>(&pred8),1);
        uint32_t m = read_u32(ifs);
        uint64_t bits_len = read_u64(ifs);
        string bytes; bytes.assign(istreambuf_iterator<char>(ifs), istreambuf_iterator<char>());
        string bits = unpack_bytes_to_bits(bytes, bits_len);

        Golomb g(m, NegativeMode::INTERLEAVED);
        vector<int> residuals; residuals.reserve((size_t)w*h);
        size_t pos=0;
        while (pos < bits.size() && residuals.size() < (size_t)w*h) {
            string tail = bits.substr(pos);
            auto res = g.decode(tail);
            residuals.push_back((int)res.first);
            size_t consumed = res.second;
            if (consumed==0) { cerr<<"Decoding error\n"; return 1; }
            pos += consumed;
        }
        if (residuals.size() != (size_t)w*h) { cerr<<"Decoded count mismatch\n"; return 1; }

        cv::Mat out(h,w,CV_8UC1);
        size_t idx=0;
        for (uint32_t r=0;r<h;++r) {
            for (uint32_t c=0;c<w;++c) {
                int pred=0;
                if (pred8==0) {
                    pred = (c==0?0:out.ptr<uint8_t>(r)[c-1]);
                } else {
                    int left = (c==0?0:out.ptr<uint8_t>(r)[c-1]);
                    int top = (r==0?0:out.ptr<uint8_t>(r-1)[c]);
                    int topleft = (r==0||c==0?0:out.ptr<uint8_t>(r-1)[c-1]);
                    int p = left + top - topleft;
                    int a=left,b=top,cval=p; int mx=max(a,max(b,cval)), mn=min(a,min(b,cval));
                    if (a!=mx && a!=mn) pred=a; else if (b!=mx && b!=mn) pred=b; else pred=cval;
                }
                int val = pred + residuals[idx++];
                if (val < 0) val = 0; else if (val > 255) val = 255;
                out.ptr<uint8_t>(r)[c] = (uint8_t)val;
            }
        }
        if (!cv::imwrite(outpath, out)) { cerr<<"Failed to write output image\n"; return 1; }
        cerr<<"Decoded image written to "<<outpath<<"\n";
        return 0;
    }
    cerr<<"Unknown mode\n"; return 1;
}
