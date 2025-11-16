#include "golomb.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <sstream>

static void print_usage(const char *prog) {
    std::cout << "Usage:\n"
              << "  " << prog << " -m <m> -mode <sign|interleave> encode <int1> [int2 ...]\n"
              << "  " << prog << " -m <m> -mode <sign|interleave> decode <bitstring>\n\n"
              << "Examples:\n"
              << "  " << prog << " -m 3 -mode interleave encode 0 -1 5 10\n"
              << "  " << prog << " -m 4 -mode sign decode 00110110\n\n"
              << "Notes:\n"
              << "  - m must be >= 1\n"
              << "  - mode 'sign' uses SIGN_MAGNITUDE; 'interleave' uses INTERLEAVED \n"
              << "  - encode prints each encoded bitstring and then decodes the concatenated stream\n"
              << "  - decode will repeatedly decode values from the provided bitstring until exhausted\n";
}

static bool parse_mode(const std::string &s, NegativeMode &mode_out) {
    if (s == "sign" || s == "SIGN" || s == "Sign" || s == "sign-magnitude" || s == "sign_magnitude") {
        mode_out = NegativeMode::SIGN_MAGNITUDE;
        return true;
    } else if (s == "interleave" || s == "INTERLEAVE" || s == "interleaved" || s == "zigzag") {
        mode_out = NegativeMode::INTERLEAVED;
        return true;
    }
    return false;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    uint64_t m = 0;
    NegativeMode mode = NegativeMode::INTERLEAVED;
    bool have_m = false;
    bool have_mode = false;


    int argi = 1;
    while (argi < argc) {
        if (std::strcmp(argv[argi], "-m") == 0) {
            if (argi + 1 >= argc) {
                std::cerr << "Error: -m requires a value\n";
                return 2;
            }
            long long tmp = std::atoll(argv[argi + 1]);
            if (tmp < 1) {
                std::cerr << "Error: m must be >= 1\n";
                return 2;
            }
            m = static_cast<uint64_t>(tmp);
            have_m = true;
            argi += 2;
        } else if (std::strcmp(argv[argi], "-mode") == 0) {
            if (argi + 1 >= argc) {
                std::cerr << "Error: -mode requires a value\n";
                return 2;
            }
            std::string ms = argv[argi + 1];
            if (!parse_mode(ms, mode)) {
                std::cerr << "Error: unknown mode '" << ms << "'. Use 'sign' or 'interleave'.\n";
                return 2;
            }
            have_mode = true;
            argi += 2;
        } else {
            break;
        }
    }

    if (!have_m) {
        std::cerr << "Error: please specify -m <m> (m >= 1)\n";
        print_usage(argv[0]);
        return 2;
    }

    if (argi >= argc) {
        std::cerr << "Error: missing operation (encode/decode)\n";
        print_usage(argv[0]);
        return 2;
    }

    std::string op = argv[argi++];
    Golomb coder(m, mode);

    if (op == "encode") {
        if (argi >= argc) {
            std::cerr << "Error: encode requires at least one integer argument\n";
            print_usage(argv[0]);
            return 2;
        }
        std::vector<int64_t> values;
        for (; argi < argc; ++argi) {
            char *endptr = nullptr;
            long long v = std::strtoll(argv[argi], &endptr, 0);
            if (endptr == argv[argi] || *endptr != '\0') {
                std::cerr << "Warning: skipping invalid integer '" << argv[argi] << "'\n";
                continue;
            }
            values.push_back((int64_t)v);
        }
        if (values.empty()) {
            std::cerr << "Error: no valid integers to encode\n";
            return 2;
        }

        std::cout << "Parameters: m=" << m
                  << " mode=" << (mode == NegativeMode::SIGN_MAGNITUDE ? "SIGN_MAGNITUDE" : "INTERLEAVED")
                  << "\n\n";

        std::string concat;
        for (size_t i = 0; i < values.size(); ++i) {
            std::string bits = coder.encode(values[i]);
            concat += bits;
            std::cout << "Value[" << i << "] = " << values[i] << " -> bits: " << bits
                      << " (len=" << bits.size() << ")\n";
        }

        std::cout << "\nConcatenated bitstream (" << concat.size() << " bits):\n"
                  << concat << "\n\n";

        std::cout << "Decoding concatenated stream to verify round-trip:\n";
        size_t pos = 0;
        size_t index = 0;
        while (pos < concat.size()) {
            std::string tail = concat.substr(pos);
            auto res = coder.decode(tail);
            int64_t decoded = res.first;
            size_t consumed = res.second;
            if (consumed == 0) {
                std::cerr << "Decoding error: consumed 0 bits at pos " << pos << "\n";
                break;
            }
            std::cout << "Decoded[" << index << "] = " << decoded << " (consumed=" << consumed << " bits)\n";
            pos += consumed;
            ++index;
        }
        if (pos != concat.size()) {
            std::cerr << "Warning: not all bits consumed (pos=" << pos << " total=" << concat.size() << ")\n";
        } else {
            std::cout << "Round-trip OK: encoded " << values.size() << " values into " << concat.size() << " bits.\n";
        }

        return 0;
    } else if (op == "decode") {
        if (argi >= argc) {
            std::cerr << "Error: decode requires a bitstring argument\n";
            print_usage(argv[0]);
            return 2;
        }
        std::ostringstream oss;
        for (; argi < argc; ++argi) {
            if (argi > 0) oss << ' ';
            oss << argv[argi];
        }
        std::string raw = oss.str();

        std::string bits;
        bits.reserve(raw.size());
        for (char c : raw) {
            if (c == '0' || c == '1') bits.push_back(c);
        }

        if (bits.empty()) {
            std::cerr << "Error: provided bitstring contains no '0'/'1' characters\n";
            return 2;
        }

        std::cout << "Parameters: m=" << m
                  << " mode=" << (mode == NegativeMode::SIGN_MAGNITUDE ? "SIGN_MAGNITUDE" : "INTERLEAVED")
                  << "\n\n";

        std::cout << "Decoding bitstream (" << bits.size() << " bits):\n";
        std::cout << bits << "\n\n";

        size_t pos = 0;
        size_t index = 0;
        while (pos < bits.size()) {
            std::string tail = bits.substr(pos);
            try {
                auto res = coder.decode(tail);
                int64_t decoded = res.first;
                size_t consumed = res.second;
                if (consumed == 0) {
                    std::cerr << "Decoding error: consumed 0 bits at pos " << pos << "\n";
                    return 3;
                }
                std::cout << "Decoded[" << index << "] = " << decoded << " (consumed=" << consumed << " bits)\n";
                pos += consumed;
                ++index;
            } catch (const std::exception &ex) {
                std::cerr << "Decoding failed at pos " << pos << ": " << ex.what() << "\n";
                return 4;
            }
        }

        std::cout << "\nDecoded " << index << " value(s).\n";
        return 0;
    } else {
        std::cerr << "Error: unknown operation '" << op << "'. Use 'encode' or 'decode'.\n";
        print_usage(argv[0]);
        return 2;
    }
}
