#include <cstring>

#include "portal/xbox_frame.h"
#include "test_util.h"

using namespace skylanders::portal;

int main() {
  // A valid frame (0B 14 + payload) becomes a report with the payload at the front.
  uint8_t frame[kFrameSize] = {0x0B, 0x14, 0x52, 0x01, 0x02};
  auto report = ReportFromFrame(frame);
  CHECK(report.has_value());
  CHECK((*report)[0] == 0x52);
  CHECK((*report)[1] == 0x01);
  CHECK((*report)[2] == 0x02);
  CHECK((*report)[3] == 0x00);
  CHECK((*report)[30] == 0x00 && (*report)[31] == 0x00);

  // A wrong header is rejected, whichever byte is wrong.
  uint8_t bad1[kFrameSize] = {0x0B, 0x15, 0x52};
  uint8_t bad2[kFrameSize] = {0x0A, 0x14, 0x52};
  uint8_t zeros[kFrameSize] = {};
  CHECK(!ReportFromFrame(bad1).has_value());
  CHECK(!ReportFromFrame(bad2).has_value());
  CHECK(!ReportFromFrame(zeros).has_value());

  // A report becomes a frame: header, then the first 30 report bytes. The last two do not fit.
  Report r{};
  r[0] = 0x53;
  r[29] = 0xEE;
  r[30] = 0xAA;
  r[31] = 0xBB;
  uint8_t out[kFrameSize];
  std::memset(out, 0xCC, sizeof(out));
  FrameFromReport(r, out);
  CHECK(out[0] == 0x0B && out[1] == 0x14);
  CHECK(out[2] == 0x53);
  CHECK(out[31] == 0xEE);

  // Round trip: a report with 30 payload bytes survives; bytes 30 and 31 come back as zero.
  auto back = ReportFromFrame(out);
  CHECK(back.has_value());
  CHECK((*back)[0] == 0x53 && (*back)[29] == 0xEE);
  CHECK((*back)[30] == 0x00 && (*back)[31] == 0x00);

  return Finish("portal_framing");
}
