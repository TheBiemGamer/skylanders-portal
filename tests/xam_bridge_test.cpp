#include <vector>

#include "portal/xam_bridge.h"
#include "test_util.h"

using namespace skylanders::portal;

namespace {
struct FakeDevice : PortalDevice {
  std::vector<Report> writes;
  std::vector<AudioPacket> audio;
  Report next{};
  void Write(const Report& r) override { writes.push_back(r); }
  Report Read() override { return next; }
  void WriteAudio(const AudioPacket& p) override { audio.push_back(p); }
};
}  // namespace

int main() {
  XamBridge bridge;
  FakeDevice dev;
  dev.next[0] = 'S';
  dev.next[1] = 0x01;

  // The game drains reads until one says "no new data" (state 0), then sends its commands. So
  // reads alternate: a report (state 1), then "nothing new" (state 0, no bytes).
  std::array<uint8_t, 32> buf{};
  uint32_t bytes = 0;
  uint16_t state = 7;
  CHECK(bridge.Read(&dev, buf, bytes, state) == kXamSuccess);
  CHECK(buf[0] == 0x0B && buf[1] == 0x14 && buf[2] == 'S' && buf[3] == 0x01);
  CHECK(bytes == 32);
  CHECK(state == 1);
  CHECK(bridge.Read(&dev, buf, bytes, state) == kXamSuccess);
  CHECK(state == 0);
  CHECK(bytes == 0);
  CHECK(bridge.Read(&dev, buf, bytes, state) == kXamSuccess);
  CHECK(state == 1 && bytes == 32);
  CHECK(bridge.Read(&dev, buf, bytes, state) == kXamSuccess);  // back to "nothing new"
  CHECK(state == 0);

  // A shorter buffer gets only what fits.
  std::array<uint8_t, 8> small{};
  CHECK(bridge.Read(&dev, small, bytes, state) == kXamSuccess);
  CHECK(state == 1 && bytes == 8 && small[0] == 0x0B);

  // Command write: header stripped.
  std::array<uint8_t, 32> frame{};
  frame[0] = 0x0B;
  frame[1] = 0x14;
  frame[2] = 'A';
  frame[3] = 0x01;
  CHECK(bridge.Write(&dev, frame) == kXamSuccess);
  REQUIRE_OR_RETURN(dev.writes.size() == 1);
  CHECK(dev.writes[0][0] == 'A' && dev.writes[0][1] == 0x01);

  // Frame without the header: ignored, still success (the game retries).
  std::array<uint8_t, 32> bad{};
  CHECK(bridge.Write(&dev, bad) == kXamSuccess);
  CHECK(dev.writes.size() == 1);

  // 64-byte write: speaker audio, passed as-is.
  std::array<uint8_t, 64> audio{};
  audio[0] = 0x11;
  audio[63] = 0x22;
  CHECK(bridge.Write(&dev, audio) == kXamSuccess);
  CHECK(dev.audio.size() == 1 && dev.audio[0][0] == 0x11 && dev.audio[0][63] == 0x22);

  // No device.
  CHECK(bridge.Read(nullptr, buf, bytes, state) == kXamDeviceNotConnected);
  CHECK(bridge.Write(nullptr, frame) == kXamDeviceNotConnected);
  return Finish("xam_bridge");
}
