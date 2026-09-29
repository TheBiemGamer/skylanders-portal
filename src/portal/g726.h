#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace skylanders::portal {

// G.726 ADPCM decoder at 32 kbit/s (4-bit codes, 8 kHz), 16-bit linear output. The Xbox 360
// Trap Team game encodes the Traptanium portal's speaker audio this way: frames with the header
// 0B 17 carry 30 bytes, 60 codes, first code in the low nibble. The encoder's state carries over
// from frame to frame, so one decoder must see the whole stream in order.
//
// An implementation of the ITU-T G.726 algorithm (adaptive quantizer, 2-pole/6-zero predictor,
// adaptation speed control), following the structure of Sun Microsystems' public g72x reference.
class G726Decoder {
 public:
  G726Decoder() { Reset(); }
  void Reset();
  int16_t Decode(uint8_t code);

 private:
  int StepSize() const;
  int PredictorZero() const;
  int PredictorPole() const;
  void Update(int y, int wi, int fi, int dq, int sr, int dqsez);

  // Widths match the reference (and the game's encoder): 32-bit, except the quantized
  // difference history, which is 16-bit.
  int32_t yl_;                  // locked quantizer scale factor
  int32_t yu_;                  // unlocked quantizer scale factor
  int32_t dms_, dml_;           // short- and long-term energy averages
  int32_t ap_;                  // linear weighting coefficient of yl and yu
  std::array<int32_t, 2> a_;    // pole predictor coefficients
  std::array<int32_t, 6> b_;    // zero predictor coefficients
  std::array<int32_t, 2> pk_;   // signs of the last two partial reconstructions
  std::array<int16_t, 6> dq_;   // last six quantized differences (floating-point form)
  std::array<int32_t, 2> sr_;   // last two reconstructed signals (floating-point form)
  bool td_;                     // tone detected
};

// Decodes packed 4-bit codes (low nibble first) and appends two samples per byte to `out`.
void DecodeSpeakerAudio(std::span<const uint8_t> codes, G726Decoder& decoder,
                        std::vector<int16_t>& out);

}  // namespace skylanders::portal
