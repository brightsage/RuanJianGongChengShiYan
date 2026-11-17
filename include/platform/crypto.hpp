#pragma once

#include <cstddef>
#include <string>

namespace platform::crypto {

// 计算输入字符串的 SHA-256 哈希值，返回 64 字符的小写十六进制字符串
std::string sha256_hex(const std::string& input);

// 计算 HMAC-SHA-256 签名，返回 64 字符的小写十六进制字符串
std::string hmac_sha256_hex(const std::string& key, const std::string& input);

// 生成指定字节数的加密伪随机字节，返回十六进制字符串
std::string random_bytes_hex(std::size_t count);

// 计算 PBKDF2-HMAC-SHA-256 派生密钥，返回 64 字符的小写十六进制字符串
std::string pbkdf2_hmac_sha256_hex(const std::string& password, const std::string& salt, int iterations = 10000);

// 常数时间安全字符串比较，防范时序侧信道攻击
bool timing_safe_equal(const std::string& a, const std::string& b);

}  // namespace platform::crypto
