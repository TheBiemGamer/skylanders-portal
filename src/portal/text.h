#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>

namespace skylanders {

// ASCII lowercase, for case-insensitive matching of names and extensions.
inline std::string Lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

// "0a 1b 2c ...": bytes as lowercase hex, for trace logs.
inline std::string HexBytes(const uint8_t* data, size_t n) {
  static constexpr char kHex[] = "0123456789abcdef";
  std::string out;
  out.reserve(n * 3);
  for (size_t i = 0; i < n; ++i) {
    if (i) out += ' ';
    out += kHex[data[i] >> 4];
    out += kHex[data[i] & 0xF];
  }
  return out;
}

}  // namespace skylanders
