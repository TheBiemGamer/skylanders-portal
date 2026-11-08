#include "portal/figure_stats.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <string_view>

#include "portal/figure_crypto.h"

namespace skylanders::portal {

namespace {

using Block = FigureBlock;

// The game saves to two areas in turn: area 0 from block 0x08, area 1 from block 0x24.
constexpr uint8_t kAreaStart[2] = {0x08, 0x24};

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

  // Each area's first block has its own CRC16 (offset 0x0E, with that field itself replaced by
  // the literal bytes 0x05,0x00 before computing -- confirmed against real dumps, not documented
  // correctly in the community writeup this is sourced from), which tells us the decrypt -- and
  // so the key derivation -- actually worked, before trusting gold or nickname from it. Byte 0x09
  // counts saves; the area the game wrote last is the one one ahead of the other (mod 256).
  std::optional<Block> head[2];
  for (int i = 0; i < 2; ++i) {
    const Block block = DecryptFigureBlock(data, kAreaStart[i]);
    Block for_crc = block;
    for_crc[0x0E] = 0x05;
    for_crc[0x0F] = 0x00;
    if (ReadU16LE(&block[0x0E]) == FigureCrc16(for_crc.data(), for_crc.size())) head[i] = block;
  }
  if (!head[0] && !head[1]) return std::nullopt;
  int newest = head[0] ? 0 : 1;
  if (head[0] && head[1] && static_cast<uint8_t>((*head[1])[0x09] - (*head[0])[0x09]) == 1) {
    newest = 1;
  }
  const uint8_t start = kAreaStart[newest];
  const Block& area = *head[newest];

  FigureStats stats{};
  stats.gold = ReadU16LE(&area[0x03]);

  // Nickname: UTF-16LE, null-terminated, spread across the area's third and fourth blocks
  // (start + 2, start + 4; start + 3 is a sector trailer), up to 15 characters (30 bytes).
  const Block nick_a = DecryptFigureBlock(data, start + 2);
  const Block nick_b = DecryptFigureBlock(data, start + 4);
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

  // Level: a single 24-bit LE value at the area's offset 0x00 (bytes 0x00-0x02), immediately before
  // gold at 0x03 -- confirmed against the independent SkyReader project's Skylander::getXP(),
  // which reads exactly these three bytes the same way. Earlier revisions of this function tried
  // splitting XP across a separate, unchecksummed "area 2" block per a community writeup that
  // turned out to be wrong for this game; that area 2 read is gone now. This field lives in the
  // same already-checksum-verified area block gold does, so it's exactly as trustworthy.
  const uint32_t total_xp = static_cast<uint32_t>(area[0x00]) |
                            (static_cast<uint32_t>(area[0x01]) << 8) |
                            (static_cast<uint32_t>(area[0x02]) << 16);
  stats.level = LevelForXp(total_xp);

  return stats;
}

}  // namespace skylanders::portal
