#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>
#include <algorithm>

static void clamp_ip(int &v) { if (v < 0) v = 0; else if (v > 255) v = 255; }

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <input> <output> <operation> [param]\n";
        std::cerr << "operations: neg | mirror_h | mirror_v | rotate <k> | bright <delta>\n";
        return 1;
    }

    std::string input = argv[1];
    std::string output = argv[2];
    std::string op = argv[3];

    cv::Mat img = cv::imread(input, cv::IMREAD_UNCHANGED);
    if (img.empty()) {
        std::cerr << "Failed to read input: " << input << "\n";
        return 1;
    }

    // Support both single-channel and 3-channel images; operations apply per-channel
    cv::Mat dst;

    if (op == "neg") {
        dst.create(img.rows, img.cols, img.type());
        for (int r = 0; r < img.rows; ++r) {
            const uchar* srcp = img.ptr<uchar>(r);
            uchar* dstp = dst.ptr<uchar>(r);
            int channels = img.channels();
            int rowBytes = img.cols * channels;
            for (int i = 0; i < rowBytes; ++i) {
                dstp[i] = 255 - srcp[i];
            }
        }
    } else if (op == "mirror_h") {
        dst.create(img.rows, img.cols, img.type());
        int channels = img.channels();
        for (int r = 0; r < img.rows; ++r) {
            const uchar* srcp = img.ptr<uchar>(r);
            uchar* dstp = dst.ptr<uchar>(r);
            for (int c = 0; c < img.cols; ++c) {
                for (int ch = 0; ch < channels; ++ch) {
                    dstp[c*channels + ch] = srcp[(img.cols - 1 - c)*channels + ch];
                }
            }
        }
    } else if (op == "mirror_v") {
        dst.create(img.rows, img.cols, img.type());
        int channels = img.channels();
        for (int r = 0; r < img.rows; ++r) {
            const uchar* srcp = img.ptr<uchar>(r);
            uchar* dstp = dst.ptr<uchar>(img.rows - 1 - r);
            // copy entire row
            int rowBytes = img.cols * channels;
            std::copy(srcp, srcp + rowBytes, dstp);
        }
    } else if (op == "rotate") {
        if (argc < 5) { std::cerr << "rotate needs parameter k (integer)\n"; return 1; }
        int k = std::stoi(argv[4]);
        // normalize k to [0..3]
        k %= 4; if (k < 0) k += 4;
        if (k == 0) {
            dst = img.clone();
        } else if (k == 2) {
            // 180 deg
            dst.create(img.rows, img.cols, img.type());
            int channels = img.channels();
            for (int r = 0; r < img.rows; ++r) {
                const uchar* srcp = img.ptr<uchar>(r);
                uchar* dstp = dst.ptr<uchar>(img.rows - 1 - r);
                int rowBytes = img.cols * channels;
                // reverse row
                for (int c = 0; c < img.cols; ++c) {
                    for (int ch = 0; ch < channels; ++ch) {
                        dstp[(img.cols - 1 - c)*channels + ch] = srcp[c*channels + ch];
                    }
                }
            }
        } else if (k == 1) {
            // 90 deg clockwise: size becomes cols x rows
            dst.create(img.cols, img.rows, img.type());
            int channels = img.channels();
            for (int r = 0; r < img.rows; ++r) {
                const uchar* srcp = img.ptr<uchar>(r);
                for (int c = 0; c < img.cols; ++c) {
                    for (int ch = 0; ch < channels; ++ch) {
                        // pixel (r,c) -> (c, newcol) where newcol = (rows-1 - r)
                        dst.ptr<uchar>(c)[(img.rows - 1 - r)*channels + ch] = srcp[c*channels + ch];
                    }
                }
            }
        } else if (k == 3) {
            // 270 deg clockwise (or 90 deg ccw)
            dst.create(img.cols, img.rows, img.type());
            int channels = img.channels();
            for (int r = 0; r < img.rows; ++r) {
                const uchar* srcp = img.ptr<uchar>(r);
                for (int c = 0; c < img.cols; ++c) {
                    for (int ch = 0; ch < channels; ++ch) {
                        // pixel (r,c) -> (newr, c) where newr = img.cols - 1 - c
                        dst.ptr<uchar>(img.cols - 1 - c)[r*channels + ch] = srcp[c*channels + ch];
                    }
                }
            }
        }
    } else if (op == "bright") {
        if (argc < 5) { std::cerr << "bright needs parameter delta (integer)\n"; return 1; }
        int delta = std::stoi(argv[4]);
        dst.create(img.rows, img.cols, img.type());
        int channels = img.channels();
        for (int r = 0; r < img.rows; ++r) {
            const uchar* srcp = img.ptr<uchar>(r);
            uchar* dstp = dst.ptr<uchar>(r);
            int rowBytes = img.cols * channels;
            for (int i = 0; i < rowBytes; ++i) {
                int v = srcp[i] + delta;
                clamp_ip(v);
                dstp[i] = static_cast<uchar>(v);
            }
        }
    } else {
        std::cerr << "Unknown operation: " << op << "\n";
        return 1;
    }

    // If output extension is .pgm and dst has 3 channels, convert to single-channel (grayscale)
    auto toLower = [](const std::string &s){ std::string t = s; for (char &c: t) c = std::tolower((unsigned char)c); return t; };
    auto getExt = [&](const std::string &p){ size_t pos = p.find_last_of('.'); if (pos==std::string::npos) return std::string(); return toLower(p.substr(pos)); };
    std::string ext = getExt(output);
    if (ext == ".pgm") {
        // Ensure single-channel gray for PGM
        if (dst.channels() == 1) {
            if (!cv::imwrite(output, dst)) {
                std::cerr << "Failed to write output: " << output << "\n";
                return 1;
            }
        } else {
            cv::Mat gray(dst.rows, dst.cols, CV_8UC1);
            int channels = dst.channels();
            for (int r = 0; r < dst.rows; ++r) {
                const uchar* srcp = dst.ptr<uchar>(r);
                uchar* dstp = gray.ptr<uchar>(r);
                for (int c = 0; c < dst.cols; ++c) {
                    int v = 0;
                    if (channels == 3) {
                        int b = srcp[c*3 + 0];
                        int g = srcp[c*3 + 1];
                        int rch = srcp[c*3 + 2];
                        v = static_cast<int>(0.114 * b + 0.587 * g + 0.299 * rch + 0.5);
                    } else if (channels == 4) {
                        int b = srcp[c*4 + 0];
                        int g = srcp[c*4 + 1];
                        int rch = srcp[c*4 + 2];
                        v = static_cast<int>(0.114 * b + 0.587 * g + 0.299 * rch + 0.5);
                    } else {
                        // fallback: average all channels
                        int sum = 0;
                        for (int ch = 0; ch < channels; ++ch) sum += srcp[c*channels + ch];
                        v = (sum + channels/2) / channels;
                    }
                    clamp_ip(v);
                    dstp[c] = (uchar)v;
                }
            }
            if (!cv::imwrite(output, gray)) {
                std::cerr << "Failed to write output: " << output << "\n";
                return 1;
            }
        }
    } else {
        // For other formats (ppm, png, jpg...) let OpenCV write the dst as-is
        if (!cv::imwrite(output, dst)) {
            std::cerr << "Failed to write output: " << output << "\n";
            return 1;
        }
    }

    std::cout << "Wrote: " << output << "\n";
    return 0;
}
