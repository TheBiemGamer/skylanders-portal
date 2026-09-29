#include "portal/xbox_frame.h"

#include <cstring>

namespace skylanders::portal {

std::optional<Report> ReportFromFrame(const uint8_t* frame) {
  if (frame[0] != kFrameHeader0 || frame[1] != kFrameHeader1) return std::nullopt;
  Report report{};
  std::memcpy(report.data(), frame + 2, kFramePayload);
  return report;
}

void FrameFromReport(const Report& report, uint8_t* frame) {
  frame[0] = kFrameHeader0;
  frame[1] = kFrameHeader1;
  std::memcpy(frame + 2, report.data(), kFramePayload);
}

}  // namespace skylanders::portal
