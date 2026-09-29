#include "portal/figure_stats.h"

#include <algorithm>
#include <array>
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

uint16_t Crc16CcittFalse(const uint8_t* data, size_t size) {
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

// Verified against real dumps in this project (see figure_stats.h) against the community
// reverse-engineering this is sourced from: the trailing space here is easy to drop by accident,
// and dropping it silently produces a wrong key with no compile-time or obvious runtime signal.
constexpr std::string_view kKeyConstant = " Copyright (C) 2010 Activision. All Rights Reserved. ";

using Block = std::array<uint8_t, kBlockSize>;

// Decrypts one 16-byte block in place. Each block has its own key: MD5 of the tag's first 0x20
// bytes + this block's own index + the constant above.
Block DecryptBlock(const FigureData& data, uint8_t block_index) {
  std::array<uint8_t, 0x20 + 1 + kKeyConstant.size()> key_material{};
  std::memcpy(key_material.data(), data.data(), 0x20);
  key_material[0x20] = block_index;
  std::memcpy(key_material.data() + 0x21, kKeyConstant.data(), kKeyConstant.size());

  std::array<uint8_t, 16> key{};
  md5_buffer(reinterpret_cast<const char*>(key_material.data()), key_material.size(), key.data());

  std::array<uint8_t, AES_ROUND_KEY_SIZE> round_keys{};
  aes_key_schedule_128(key.data(), round_keys.data());

  Block out{};
  aes_decrypt_128(round_keys.data(), data.data() + block_index * kBlockSize, out.data());
  return out;
}

uint16_t ReadU16LE(const uint8_t* p) {
  return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

// The Swap Force max-level table (supersedes earlier games' lower per-tier caps); total XP is
// the sum of every generation's XP field once loaded into a game new enough to have added it.
constexpr std::array<uint32_t, 20> kLevelXpTable = {
    0,      1000,   2200,   3800,   6000,   9000,   13000,  18200,  24800,  33000,
    42700,  53900,  66600,  80800,  96500,  113700, 132400, 152600, 174300, 197500,
};

uint8_t LevelForXp(uint32_t total_xp) {
  uint8_t level = 1;
  for (size_t i = 0; i < kLevelXpTable.size(); ++i) {
    if (total_xp >= kLevelXpTable[i]) level = static_cast<uint8_t>(i + 1);
  }
  return level;
}

}  // namespace

std::optional<FigureStats> ParseFigureStats(const FigureData& data) {
  const bool all_zero = std::all_of(data.begin(), data.end(), [](uint8_t b) { return b == 0; });
  if (all_zero) return std::nullopt;

  // Area 0 starts at block 0x08. Its own CRC16 (offset 0x0E, with that field itself replaced by
  // the literal bytes 0x05,0x00 before computing -- confirmed against real dumps, not documented
  // correctly in the community writeup this is sourced from) tells us the decrypt -- and so the
  // key derivation -- actually worked, before trusting gold or nickname from it.
  const Block area0 = DecryptBlock(data, 0x08);
  Block area0_for_crc = area0;
  area0_for_crc[0x0E] = 0x05;
  area0_for_crc[0x0F] = 0x00;
  const uint16_t area0_crc_stored = ReadU16LE(&area0[0x0E]);
  const uint16_t area0_crc_computed = Crc16CcittFalse(area0_for_crc.data(), area0_for_crc.size());
  if (area0_crc_stored != area0_crc_computed) return std::nullopt;

  FigureStats stats{};
  stats.gold = ReadU16LE(&area0[0x03]);

  // Nickname: UTF-16LE, null-terminated, spread across the area's third and fourth blocks
  // (0x08 + 2, 0x08 + 4), up to 15 characters (30 bytes).
  const Block nick_a = DecryptBlock(data, 0x0A);
  const Block nick_b = DecryptBlock(data, 0x0C);
  std::array<uint8_t, kBlockSize * 2> nick_bytes{};
  std::memcpy(nick_bytes.data(), nick_a.data(), kBlockSize);
  std::memcpy(nick_bytes.data() + kBlockSize, nick_b.data(), kBlockSize);
  std::u16string nickname_u16;
  for (size_t i = 0; i + 1 < nick_bytes.size(); i += 2) {
    const char16_t c = static_cast<char16_t>(ReadU16LE(&nick_bytes[i]));
    if (c == 0) break;
    nickname_u16.push_back(c);
  }
  // Narrow UTF-16 to UTF-8 by hand: nicknames are short and this avoids pulling in a full
  // Unicode conversion dependency into this SDK-independent library for one field.
  std::string nickname_utf8;
  for (char16_t c : nickname_u16) {
    if (c < 0x80) {
      nickname_utf8.push_back(static_cast<char>(c));
    } else if (c < 0x800) {
      nickname_utf8.push_back(static_cast<char>(0xC0 | (c >> 6)));
      nickname_utf8.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    } else {
      nickname_utf8.push_back(static_cast<char>(0xE0 | (c >> 12)));
      nickname_utf8.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
      nickname_utf8.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    }
  }
  stats.nickname = std::move(nickname_utf8);

  // Level: a single 24-bit LE value at area 0 offset 0x00 (bytes 0x00-0x02), immediately before
  // gold at 0x03 -- confirmed against the independent SkyReader project's Skylander::getXP(),
  // which reads exactly these three bytes the same way. Earlier revisions of this function tried
  // splitting XP across a separate, unchecksummed "area 2" block per a community writeup that
  // turned out to be wrong for this game; that area 2 read is gone now. This field lives in the
  // same already-checksum-verified area0 block gold does, so it's exactly as trustworthy.
  const uint32_t total_xp = static_cast<uint32_t>(area0[0x00]) |
                            (static_cast<uint32_t>(area0[0x01]) << 8) |
                            (static_cast<uint32_t>(area0[0x02]) << 16);
  stats.level = LevelForXp(total_xp);

  return stats;
}

}  // namespace skylanders::portal
