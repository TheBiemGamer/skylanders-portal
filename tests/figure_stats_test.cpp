#include <algorithm>

#include "portal/figure_crypto.h"
#include "portal/figure_file.h"
#include "portal/figure_stats.h"
#include "test_util.h"

using namespace skylanders::portal;

int main() {
  // A figure with no save data at all (e.g. --portal_test_figure) has nothing to report.
  {
    FigureData d{};
    CHECK(!ParseFigureStats(d).has_value());
  }

  // A freshly created blank figure (real id/variant/CRC, but never played -- its save-data
  // region is still zero) also has no stats yet, not zeros presented as real data.
  {
    FigureData d = CreateBlankFigure(112, 0, {0x11, 0x22, 0x33, 0x44});
    CHECK(!ParseFigureStats(d).has_value());
  }

  // The game saves to two areas in turn (blocks 0x08 and 0x24), counting saves in byte 0x09 of
  // each. Stats come from whichever area holds the newer save, counting past 255 back to 0.
  {
    auto with_areas = [](int seq0, uint16_t gold0, int seq1, uint16_t gold1) {
      FigureData d = CreateBlankFigure(450, 12288, {0x11, 0x22, 0x33, 0x44});
      auto put_area = [&d](uint8_t base, int seq, uint16_t gold) {
        FigureBlock plain{};
        plain[0x03] = static_cast<uint8_t>(gold);
        plain[0x04] = static_cast<uint8_t>(gold >> 8);
        plain[0x09] = static_cast<uint8_t>(seq);
        FigureBlock for_crc = plain;
        for_crc[0x0E] = 0x05;
        const uint16_t crc = FigureCrc16(for_crc.data(), for_crc.size());
        plain[0x0E] = static_cast<uint8_t>(crc);
        plain[0x0F] = static_cast<uint8_t>(crc >> 8);
        const FigureBlock enc = EncryptFigureBlock(d, base, plain);
        std::copy(enc.begin(), enc.end(), d.begin() + base * kBlockSize);
        for (uint8_t nick : {uint8_t(base + 2), uint8_t(base + 4)}) {
          const FigureBlock blank = EncryptFigureBlock(d, nick, FigureBlock{});
          std::copy(blank.begin(), blank.end(), d.begin() + nick * kBlockSize);
        }
      };
      if (seq0 >= 0) put_area(0x08, seq0, gold0);
      if (seq1 >= 0) put_area(0x24, seq1, gold1);
      return d;
    };
    auto gold = [](const FigureData& d) {
      auto s = ParseFigureStats(d);
      return s ? int(s->gold) : -1;
    };
    CHECK(gold(with_areas(4, 100, 5, 200)) == 200);  // area 1 newer
    CHECK(gold(with_areas(6, 300, 5, 200)) == 300);  // area 0 newer
    CHECK(gold(with_areas(0, 400, 255, 200)) == 400);  // 255 -> 0 wrap: area 0 newer
    CHECK(gold(with_areas(255, 100, 0, 500)) == 500);  // and the other way round
    CHECK(gold(with_areas(4, 100, -1, 0)) == 100);   // only area 0 saved yet
    CHECK(gold(with_areas(-1, 0, 7, 700)) == 700);   // only area 1 saved yet
    FigureData nickname = with_areas(4, 100, 5, 200);
    CHECK(ParseFigureStats(nickname) && ParseFigureStats(nickname)->nickname.empty());
  }

  return Finish("figure_stats");
}
