#include "portal/g726.h"

#include <algorithm>
#include <cstdlib>

namespace skylanders::portal {

namespace {

constexpr std::array<int, 15> kPower2 = {1,     2,     4,     8,     0x10,   0x20,   0x40,  0x80,
                                         0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000};

// Index of the first table entry greater than `value` (the table's size if none is).
int Quan(int value) {
  int i = 0;
  while (i < int(kPower2.size()) && value >= kPower2[i]) ++i;
  return i;
}

// Multiplies a predictor coefficient by a value in the codec's 11-bit floating-point form.
int FMult(int an, int srn) {
  const int anmag = an > 0 ? an : ((-an) & 0x1FFF);
  const int anexp = Quan(anmag) - 6;
  const int anmant = anmag == 0 ? 32 : anexp >= 0 ? anmag >> anexp : anmag << -anexp;
  const int wanexp = anexp + ((srn >> 6) & 0xF) - 13;
  const int wanmant = (anmant * (srn & 077) + 0x30) >> 4;
  const int retval = wanexp >= 0 ? ((wanmant << wanexp) & 0x7FFF) : (wanmant >> -wanexp);
  return (an ^ srn) < 0 ? -retval : retval;
}

// A magnitude in the 11-bit floating-point form the predictor history stores.
int ToFloat(int mag) {
  const int exp = Quan(mag);
  return (exp << 6) + ((mag << 6) >> exp);
}

// 32 kbit/s tables: log of the quantized difference, scale factor multiplier, speed control.
constexpr std::array<int, 16> kDqlnTab = {-2048, 4,   135, 213, 273, 323, 373, 425,
                                          425,   373, 323, 273, 213, 135, 4,   -2048};
constexpr std::array<int, 16> kWiTab = {-12,  18,  41,  64,  112, 198, 355, 1122,
                                        1122, 355, 198, 112, 64,  41,  18,  -12};
constexpr std::array<int, 16> kFiTab = {0,     0,     0,     0x200, 0x200, 0x200, 0x600, 0xE00,
                                        0xE00, 0x600, 0x200, 0x200, 0x200, 0,     0,     0};

// The quantized difference from its log form and the scale factor.
int Reconstruct(bool sign, int dqln, int y) {
  const int dql = dqln + (y >> 2);
  if (dql < 0) return sign ? -0x8000 : 0;
  const int dex = (dql >> 7) & 15;
  const int dqt = 128 + (dql & 127);
  const int dq = (dqt << 7) >> (14 - dex);
  return sign ? dq - 0x8000 : dq;
}

}  // namespace

void G726Decoder::Reset() {
  yl_ = 34816;
  yu_ = 544;
  dms_ = dml_ = ap_ = 0;
  a_.fill(0);
  b_.fill(0);
  pk_.fill(0);
  dq_.fill(32);
  sr_.fill(32);
  td_ = false;
}

int G726Decoder::StepSize() const {
  if (ap_ >= 256) return yu_;
  int y = yl_ >> 6;
  const int dif = yu_ - y;
  const int al = ap_ >> 2;
  if (dif > 0) {
    y += (dif * al) >> 6;
  } else if (dif < 0) {
    y += (dif * al + 0x3F) >> 6;
  }
  return y;
}

int G726Decoder::PredictorZero() const {
  int sezi = 0;
  for (size_t i = 0; i < b_.size(); ++i) sezi += FMult(b_[i] >> 2, dq_[i]);
  return sezi;
}

int G726Decoder::PredictorPole() const {
  return FMult(a_[1] >> 2, sr_[1]) + FMult(a_[0] >> 2, sr_[0]);
}

void G726Decoder::Update(int y, int wi, int fi, int dq, int sr, int dqsez) {
  const int pk0 = dqsez < 0 ? 1 : 0;
  const int mag = dq & 0x7FFF;

  // Transition detection: a large difference while a tone was detected resets the predictor.
  const int ylint = yl_ >> 15;
  const int ylfrac = (yl_ >> 10) & 0x1F;
  const int thr1 = (32 + ylfrac) << ylint;
  const int thr2 = ylint > 9 ? 31 << 10 : thr1;
  const int dqthr = (thr2 + (thr2 >> 1)) >> 1;
  const bool tr = td_ && mag > dqthr;

  // Quantizer scale factor adaptation.
  int yu = y + ((wi - y) >> 5);
  if (yu < 544) yu = 544;
  if (yu > 5120) yu = 5120;
  yu_ = yu;
  yl_ += yu + ((-yl_) >> 6);

  int a2p = 0;
  if (tr) {
    a_.fill(0);
    b_.fill(0);
  } else {
    const int pks1 = pk0 ^ pk_[0];

    // Second pole coefficient.
    a2p = a_[1] - (a_[1] >> 7);
    if (dqsez != 0) {
      const int fa1 = pks1 ? a_[0] : -a_[0];
      if (fa1 < -8191) {
        a2p -= 0x100;
      } else if (fa1 > 8191) {
        a2p += 0xFF;
      } else {
        a2p += fa1 >> 5;
      }
      if (pk0 ^ pk_[1]) {
        if (a2p <= -12160) {
          a2p = -12288;
        } else if (a2p >= 12416) {
          a2p = 12288;
        } else {
          a2p -= 0x80;
        }
      } else if (a2p <= -12416) {
        a2p = -12288;
      } else if (a2p >= 12160) {
        a2p = 12288;
      } else {
        a2p += 0x80;
      }
    }
    a_[1] = a2p;

    // First pole coefficient.
    int a1 = a_[0] - (a_[0] >> 8);
    if (dqsez != 0) a1 += pks1 == 0 ? 192 : -192;
    const int a1ul = 15360 - a2p;
    if (a1 < -a1ul) a1 = -a1ul;
    if (a1 > a1ul) a1 = a1ul;
    a_[0] = a1;

    // Zero coefficients.
    for (size_t i = 0; i < b_.size(); ++i) {
      int bi = b_[i] - (b_[i] >> 8);
      if (mag) bi += (dq ^ dq_[i]) >= 0 ? 128 : -128;
      b_[i] = bi;
    }
  }

  // History, in floating-point form.
  for (size_t i = dq_.size() - 1; i > 0; --i) dq_[i] = dq_[i - 1];
  if (mag == 0) {
    dq_[0] = int16_t(dq >= 0 ? 0x20 : 0xFC20);
  } else {
    dq_[0] = int16_t(dq >= 0 ? ToFloat(mag) : ToFloat(mag) - 0x400);
  }
  sr_[1] = sr_[0];
  if (sr == 0) {
    sr_[0] = 0x20;
  } else if (sr > 0) {
    sr_[0] = ToFloat(sr);
  } else if (sr > -32768) {
    sr_[0] = ToFloat(-sr) - 0x400;
  } else {
    sr_[0] = 0xFC20;
  }
  pk_[1] = pk_[0];
  pk_[0] = pk0;

  // Tone detection.
  td_ = !tr && a2p < -11776;

  // Adaptation speed control.
  dms_ += (fi - dms_) >> 5;
  dml_ += ((fi << 2) - dml_) >> 7;
  if (tr) {
    ap_ = 256;
  } else if (y < 1536 || td_ || std::abs((dms_ << 2) - dml_) >= (dml_ >> 3)) {
    ap_ += (0x200 - ap_) >> 4;
  } else {
    ap_ += (-ap_) >> 4;
  }
}

int16_t G726Decoder::Decode(uint8_t code) {
  const int i = code & 0x0F;
  const int sezi = PredictorZero();
  const int sez = sezi >> 1;
  const int se = (sezi + PredictorPole()) >> 1;
  const int y = StepSize();
  const int dq = Reconstruct((i & 0x08) != 0, kDqlnTab[i], y);
  const int sr = dq < 0 ? se - (dq & 0x3FFF) : se + dq;
  const int dqsez = sr - se + sez;
  Update(y, kWiTab[i] << 5, kFiTab[i], dq, sr, dqsez);
  // sr has a 14-bit range; scaled to 16 bits it can overshoot, so clamp rather than wrap.
  return int16_t(std::clamp(sr << 2, -32768, 32767));
}

void DecodeSpeakerAudio(std::span<const uint8_t> codes, G726Decoder& decoder,
                        std::vector<int16_t>& out) {
  out.reserve(out.size() + codes.size() * 2);
  for (uint8_t byte : codes) {
    out.push_back(decoder.Decode(byte & 0x0F));
    out.push_back(decoder.Decode(byte >> 4));
  }
}

}  // namespace skylanders::portal
