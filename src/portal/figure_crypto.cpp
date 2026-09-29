#include "portal/figure_crypto.h"

#include <cstring>
#include <string_view>

extern "C" {
#include <aes.h>
}

// md5.h's own prototypes use a K&R-compatibility macro (__P) that collapses to an empty
// parameter list under C++ compilation, so its declaration of this function doesn't match its
// actual (C-linkage) definition -- declared directly here instead of including that header.
extern "C" void* md5_buffer(const char* buffer, size_t len, void* resblock);

namespace skylanders::portal {

namespace {

// The trailing space is part of the constant: dropping it silently produces a wrong key.
constexpr std::string_view kKeyConstant = " Copyright (C) 2010 Activision. All Rights Reserved. ";

std::array<uint8_t, AES_ROUND_KEY_SIZE> RoundKeys(const FigureData& data, uint8_t block_index) {
  std::array<uint8_t, 0x20 + 1 + kKeyConstant.size()> key_material{};
  std::memcpy(key_material.data(), data.data(), 0x20);
  key_material[0x20] = block_index;
  std::memcpy(key_material.data() + 0x21, kKeyConstant.data(), kKeyConstant.size());

  std::array<uint8_t, 16> key{};
  md5_buffer(reinterpret_cast<const char*>(key_material.data()), key_material.size(), key.data());

  std::array<uint8_t, AES_ROUND_KEY_SIZE> round_keys{};
  aes_key_schedule_128(key.data(), round_keys.data());
  return round_keys;
}

}  // namespace

FigureBlock DecryptFigureBlock(const FigureData& data, uint8_t block_index) {
  const auto round_keys = RoundKeys(data, block_index);
  FigureBlock out{};
  aes_decrypt_128(round_keys.data(), data.data() + block_index * kBlockSize, out.data());
  return out;
}

FigureBlock EncryptFigureBlock(const FigureData& data, uint8_t block_index, const FigureBlock& plain) {
  const auto round_keys = RoundKeys(data, block_index);
  FigureBlock out{};
  aes_encrypt_128(round_keys.data(), plain.data(), out.data());
  return out;
}

uint16_t FigureCrc16(const uint8_t* data, size_t size) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < size; ++i) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021)
                            : static_cast<uint16_t>(crc << 1);
    }
  }
  return crc;
}

}  // namespace skylanders::portal
