#include <array>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <vector>

#include "portal/g726.h"
#include "test_util.h"

using namespace skylanders::portal;

// 90 pseudo-random bytes (180 codes, low nibble first) and what Sun's public G.726 reference
// decoder (g72x.c/g726_32.c, with its 16-bit output clamp) makes of them. Random codes push every
// part of the codec (predictor updates, step size limits, tone/transition detection) to extremes,
// so matching all 180 samples is a strong bit-exactness check. (ffmpeg's G.726 decoder drifts
// from this reference by a few LSBs; the game's encoder follows the reference's structure.)
constexpr std::array<uint8_t, 90> kCodes = {
    0x86, 0xb6, 0xd1, 0x26, 0xcb, 0x4b, 0x2b, 0xf6, 0x14, 0x16, 0x17, 0x8b,
    0x58, 0xf8, 0xbf, 0xd1, 0xe6, 0xc1, 0x33, 0x33, 0xa0, 0xf8, 0xee, 0x4e,
    0x23, 0x22, 0xf6, 0x52, 0x2b, 0x53, 0x49, 0x40, 0x67, 0x14, 0xf5, 0xf6,
    0x74, 0x9d, 0x5d, 0x24, 0x9b, 0x09, 0xa2, 0x9c, 0xa6, 0x16, 0x5b, 0xd9,
    0x65, 0xb7, 0xfc, 0x82, 0xed, 0xdd, 0x9d, 0x9c, 0x19, 0x6b, 0x07, 0xf0,
    0xb1, 0xd6, 0x79, 0x51, 0x0c, 0x06, 0x66, 0x78, 0x03, 0x9c, 0x57, 0xc5,
    0x4d, 0x91, 0xb6, 0x4d, 0x63, 0xb1, 0xbd, 0xa2, 0x99, 0x33, 0xa5, 0x2b,
    0xeb, 0x1f, 0xda, 0xab, 0x2b, 0x05,
};
constexpr std::array<int16_t, 180> kExpected = {
    60, -92, 96, -52, 8, -20, 72, 16, -48, -36, -56, 64,
    -76, 40, 136, -12, 116, 12, 216, 24, 432, 168, -680, -1648,
    -6864, 14932, -29100, 2560, -2936, -10944, 2948, -6140, 16376, -4780, 4348, -6408,
    6808, 2224, 3900, 3424, 164, -4348, -9704, 12, -4736, -2080, -2616, 8020,
    5512, 4716, 3960, 3392, 8192, -240, 3400, 5492, -4508, 2764, 1368, 5744,
    -7400, 6836, -1192, 6012, 9680, 31828, 22000, 13016, 26236, 6024, 28008, 2296,
    21220, 30512, 1400, -17828, -10984, 14296, 13792, 11060, -8116, -19260, -32768, -10512,
    -2780, -17852, -14048, -28184, 19948, -26436, 31924, -232, -6096, 17900, -27876, -3912,
    13256, 28236, 32767, -7628, -5244, -3256, 6752, -20000, -11572, -9864, -8356, -7944,
    -9776, -18632, -14192, -24728, -32768, -11640, -26132, 13376, 32767, 12680, 12116, 4824,
    9120, 36, 14164, -2172, -11640, 22776, 6248, 29076, -4308, 3808, 22500, 7432,
    29192, 32767, -24928, 32767, 19376, 19216, 1124, -20216, 32767, 32296, 32767, -284,
    -4068, 18844, 14196, -10000, 19172, -14680, -6124, 10460, 10588, 27872, 9404, -9772,
    -6072, -10676, 5204, -10152, -20920, -31848, -28, 10124, 18492, -16348, -23488, -68,
    -7404, -2192, -4772, -1532, -8604, -8064, -9656, -10748, -11004, -2632, 5416, 1024,
};

int main() {
  {
    G726Decoder dec;
    std::vector<int16_t> out;
    DecodeSpeakerAudio(kCodes, dec, out);
    REQUIRE_OR_RETURN(out.size() == kExpected.size());
    int mismatches = 0;
    for (size_t i = 0; i < out.size(); ++i) {
      if (out[i] != kExpected[i]) {
        if (mismatches++ < 5) std::fprintf(stderr, "sample %zu: got %d, want %d\n", i, out[i], kExpected[i]);
      }
    }
    CHECK(mismatches == 0);
  }

  // Silence: the game sends 0xFF bytes (code 15) while nothing plays; that decodes to 0.
  {
    G726Decoder dec;
    std::vector<uint8_t> quiet(300, 0xFF);
    std::vector<int16_t> out;
    DecodeSpeakerAudio(quiet, dec, out);
    CHECK(out.size() == 600);
    int peak = 0;
    for (int16_t v : out) peak = std::max(peak, std::abs(int(v)));
    CHECK(peak < 64);
  }

  // Reset returns the decoder to its initial state.
  {
    G726Decoder a, b;
    std::vector<int16_t> first, again;
    DecodeSpeakerAudio(kCodes, a, first);
    a.Reset();
    DecodeSpeakerAudio(kCodes, a, again);
    CHECK(first == again);
  }

  // Real speaker audio captured from the game, when available: set SKYLANDERS_TEST_SPEAKER to the
  // raw stream and SKYLANDERS_TEST_SPEAKER_PCM to the Sun reference decoder's output for it.
  if (const char* raw = std::getenv("SKYLANDERS_TEST_SPEAKER")) {
    const char* pcm = std::getenv("SKYLANDERS_TEST_SPEAKER_PCM");
    std::ifstream r(raw, std::ios::binary), p(pcm ? pcm : "", std::ios::binary);
    std::vector<uint8_t> codes((std::istreambuf_iterator<char>(r)), {});
    std::vector<char> ref((std::istreambuf_iterator<char>(p)), {});
    G726Decoder dec;
    std::vector<int16_t> out;
    DecodeSpeakerAudio(codes, dec, out);
    REQUIRE_OR_RETURN(ref.size() == out.size() * 2);
    size_t diff = 0;
    for (size_t i = 0; i < out.size(); ++i) {
      const int16_t want = int16_t(uint8_t(ref[i * 2]) | (uint8_t(ref[i * 2 + 1]) << 8));
      diff += out[i] != want;
    }
    CHECK(diff == 0);
  } else {
    std::puts("(captured-audio check skipped: SKYLANDERS_TEST_SPEAKER not set)");
  }

  return Finish("g726");
}
