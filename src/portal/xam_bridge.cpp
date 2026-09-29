#include "portal/xam_bridge.h"

#include <algorithm>
#include <array>
#include <cstring>

#include "portal/xbox_frame.h"

namespace skylanders::portal {

uint32_t XamBridge::Read(PortalDevice* device, std::span<uint8_t> buffer, uint32_t& bytes_read,
                         uint16_t& state) {
  const uint32_t status = device ? kXamSuccess : kXamDeviceNotConnected;
  if (device) {
    std::array<uint8_t, kFrameSize> frame{};
    FrameFromReport(device->Read(), frame.data());
    const size_t n = std::min(buffer.size(), frame.size());
    std::memcpy(buffer.data(), frame.data(), n);
    bytes_read = static_cast<uint32_t>(n);
    // Xenia Canary: state is 1 when this call's status equals the previous call's.
    state = previous_status_ && *previous_status_ == status ? 1 : 0;
  }
  previous_status_ = status;
  return status;
}

uint32_t XamBridge::Write(PortalDevice* device, std::span<const uint8_t> buffer) {
  if (!device) return kXamDeviceNotConnected;
  if (buffer.size() == kAudioPacketSize) {
    AudioPacket packet;
    std::memcpy(packet.data(), buffer.data(), kAudioPacketSize);
    device->WriteAudio(packet);
    return kXamSuccess;
  }
  std::array<uint8_t, kFrameSize> frame{};
  std::memcpy(frame.data(), buffer.data(), std::min(buffer.size(), frame.size()));
  if (auto report = ReportFromFrame(frame.data())) device->Write(*report);
  return kXamSuccess;
}

}  // namespace skylanders::portal
