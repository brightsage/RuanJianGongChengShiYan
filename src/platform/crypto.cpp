#include "platform/crypto.hpp"

#include <array>
#include <cstdint>
#include <iomanip>
#include <random>
#include <sstream>
#include <vector>

namespace platform::crypto {
namespace {

inline std::uint32_t rotr(std::uint32_t x, std::uint32_t n) {
    return (x >> n) | (x << (32 - n));
}

inline std::uint32_t ch(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
    return (x & y) ^ (~x & z);
}

inline std::uint32_t maj(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
    return (x & y) ^ (x & z) ^ (y & z);
}

inline std::uint32_t sigma0(std::uint32_t x) {
    return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
}

inline std::uint32_t sigma1(std::uint32_t x) {
    return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
}

inline std::uint32_t gamma0(std::uint32_t x) {
    return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3);
}

inline std::uint32_t gamma1(std::uint32_t x) {
    return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10);
}

constexpr std::array<std::uint32_t, 64> K = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

std::array<std::uint8_t, 32> sha256_raw(const std::uint8_t* data, std::size_t len) {
    std::array<std::uint32_t, 8> h = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    std::vector<std::uint8_t> msg(data, data + len);
    msg.push_back(0x80);
    while ((msg.size() % 64) != 56) {
        msg.push_back(0x00);
    }
    const std::uint64_t bit_len = static_cast<std::uint64_t>(len) * 8;
    for (int i = 7; i >= 0; --i) {
        msg.push_back(static_cast<std::uint8_t>((bit_len >> (i * 8)) & 0xff));
    }

    for (std::size_t offset = 0; offset < msg.size(); offset += 64) {
        std::array<std::uint32_t, 64> w{};
        for (std::size_t t = 0; t < 16; ++t) {
            w[t] = (static_cast<std::uint32_t>(msg[offset + t * 4]) << 24) |
                   (static_cast<std::uint32_t>(msg[offset + t * 4 + 1]) << 16) |
                   (static_cast<std::uint32_t>(msg[offset + t * 4 + 2]) << 8) |
                   (static_cast<std::uint32_t>(msg[offset + t * 4 + 3]));
        }
        for (std::size_t t = 16; t < 64; ++t) {
            w[t] = gamma1(w[t - 2]) + w[t - 7] + gamma0(w[t - 15]) + w[t - 16];
        }

        std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        std::uint32_t e = h[4], f = h[5], g = h[6], h_val = h[7];

        for (std::size_t t = 0; t < 64; ++t) {
            const std::uint32_t t1 = h_val + sigma1(e) + ch(e, f, g) + K[t] + w[t];
            const std::uint32_t t2 = sigma0(a) + maj(a, b, c);
            h_val = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }

        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += h_val;
    }

    std::array<std::uint8_t, 32> digest{};
    for (std::size_t i = 0; i < 8; ++i) {
        digest[i * 4] = static_cast<std::uint8_t>((h[i] >> 24) & 0xff);
        digest[i * 4 + 1] = static_cast<std::uint8_t>((h[i] >> 16) & 0xff);
        digest[i * 4 + 2] = static_cast<std::uint8_t>((h[i] >> 8) & 0xff);
        digest[i * 4 + 3] = static_cast<std::uint8_t>(h[i] & 0xff);
    }
    return digest;
}

std::string to_hex(const std::uint8_t* data, std::size_t len) {
    std::ostringstream out;
    for (std::size_t i = 0; i < len; ++i) {
        out << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
    }
    return out.str();
}

std::array<std::uint8_t, 32> hmac_sha256_raw(const std::uint8_t* key_data, std::size_t key_len,
                                             const std::uint8_t* msg_data, std::size_t msg_len) {
    std::array<std::uint8_t, 64> k_pad{};
    if (key_len > 64) {
        const auto key_hash = sha256_raw(key_data, key_len);
        std::copy(key_hash.begin(), key_hash.end(), k_pad.begin());
    } else {
        std::copy(key_data, key_data + key_len, k_pad.begin());
    }

    std::vector<std::uint8_t> ipad(64 + msg_len);
    for (std::size_t i = 0; i < 64; ++i) {
        ipad[i] = k_pad[i] ^ 0x36;
    }
    std::copy(msg_data, msg_data + msg_len, ipad.begin() + 64);
    const auto inner_hash = sha256_raw(ipad.data(), ipad.size());

    std::vector<std::uint8_t> opad(64 + 32);
    for (std::size_t i = 0; i < 64; ++i) {
        opad[i] = k_pad[i] ^ 0x5c;
    }
    std::copy(inner_hash.begin(), inner_hash.end(), opad.begin() + 64);
    return sha256_raw(opad.data(), opad.size());
}

}  // namespace

std::string sha256_hex(const std::string& input) {
    const auto raw = sha256_raw(reinterpret_cast<const std::uint8_t*>(input.data()), input.size());
    return to_hex(raw.data(), raw.size());
}

std::string hmac_sha256_hex(const std::string& key, const std::string& input) {
    const auto raw = hmac_sha256_raw(reinterpret_cast<const std::uint8_t*>(key.data()), key.size(),
                                     reinterpret_cast<const std::uint8_t*>(input.data()), input.size());
    return to_hex(raw.data(), raw.size());
}

std::string pbkdf2_hmac_sha256_hex(const std::string& password, const std::string& salt, int iterations) {
    std::vector<std::uint8_t> salt_block(salt.begin(), salt.end());
    salt_block.push_back(0); salt_block.push_back(0); salt_block.push_back(0); salt_block.push_back(1);

    auto u = hmac_sha256_raw(reinterpret_cast<const std::uint8_t*>(password.data()), password.size(),
                             salt_block.data(), salt_block.size());
    auto t = u;

    for (int i = 1; i < iterations; ++i) {
        u = hmac_sha256_raw(reinterpret_cast<const std::uint8_t*>(password.data()), password.size(),
                            u.data(), u.size());
        for (std::size_t j = 0; j < 32; ++j) {
            t[j] ^= u[j];
        }
    }
    return to_hex(t.data(), t.size());
}

std::string random_bytes_hex(std::size_t count) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(0, 255);
    std::vector<std::uint8_t> bytes(count);
    for (std::size_t i = 0; i < count; ++i) {
        bytes[i] = static_cast<std::uint8_t>(dis(gen));
    }
    return to_hex(bytes.data(), bytes.size());
}

bool timing_safe_equal(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    volatile unsigned char result = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        result |= static_cast<unsigned char>(a[i] ^ b[i]);
    }
    return result == 0;
}

}  // namespace platform::crypto
